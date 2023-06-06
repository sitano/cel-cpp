// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "extensions/protobuf/struct_type.h"

#include <limits>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "base/values/struct_value_builder.h"
#include "eval/internal/errors.h"
#include "extensions/protobuf/enum_type.h"
#include "extensions/protobuf/internal/reflection.h"
#include "extensions/protobuf/internal/time.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/struct_value.h"
#include "extensions/protobuf/type.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"

namespace cel::extensions {

absl::StatusOr<Handle<ProtoStructType>> ProtoStructType::Resolve(
    TypeManager& type_manager, const google::protobuf::Descriptor& descriptor) {
  CEL_ASSIGN_OR_RETURN(auto type,
                       type_manager.ResolveType(descriptor.full_name()));
  if (ABSL_PREDICT_FALSE(!type.has_value())) {
    return absl::NotFoundError(absl::StrCat(
        "Missing protocol buffer message type implementation for \"",
        descriptor.full_name(), "\""));
  }
  if (ABSL_PREDICT_FALSE(!(*type)->Is<ProtoStructType>())) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Unexpected protocol buffer message type implementation for \"",
        descriptor.full_name(), "\": ", (*type)->DebugString()));
  }
  return std::move(type).value().As<ProtoStructType>();
}

namespace {

absl::StatusOr<Handle<Type>> FieldDescriptorToTypeSingular(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  switch (field_desc->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return type_manager.type_factory().GetDoubleType();
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      return type_manager.type_factory().GetIntType();
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return type_manager.type_factory().GetUintType();
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return type_manager.type_factory().GetBoolType();
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return type_manager.type_factory().GetStringType();
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return ProtoType::Resolve(type_manager, *field_desc->message_type());
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return type_manager.type_factory().GetBytesType();
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return ProtoType::Resolve(type_manager, *field_desc->enum_type());
  }
}

absl::StatusOr<Handle<Type>> FieldDescriptorToTypeRepeated(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToTypeSingular(type_manager, field_desc));
  // The wrapper types make zero sense as a list element, list elements of
  // wrapper types can never be null.
  return type_manager.type_factory().CreateListType(
      UnwrapType(std::move(type)));
}

absl::StatusOr<Handle<Type>> FieldDescriptorToType(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  if (field_desc->is_map()) {
    const auto* key_desc = field_desc->message_type()->map_key();
    CEL_ASSIGN_OR_RETURN(auto key_type,
                         FieldDescriptorToTypeSingular(type_manager, key_desc));
    const auto* value_desc = field_desc->message_type()->map_value();
    CEL_ASSIGN_OR_RETURN(auto value_type, FieldDescriptorToTypeSingular(
                                              type_manager, value_desc));
    // The wrapper types make zero sense as a map value, map values of
    // wrapper types can never be null.
    return type_manager.type_factory().CreateMapType(
        std::move(key_type), UnwrapType(std::move(value_type)));
  }
  if (field_desc->is_repeated()) {
    return FieldDescriptorToTypeRepeated(type_manager, field_desc);
  }
  return FieldDescriptorToTypeSingular(type_manager, field_desc);
}

}  // namespace

class ProtoStructTypeFieldIterator final : public StructType::FieldIterator {
 public:
  explicit ProtoStructTypeFieldIterator(const google::protobuf::Descriptor& descriptor)
      : descriptor_(descriptor) {}

  bool HasNext() override { return index_ < descriptor_.field_count(); }

  absl::StatusOr<StructType::Field> Next(TypeManager& type_manager) override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.field_count())) {
      return absl::FailedPreconditionError(
          "StructType::FieldIterator::Next() called when "
          "StructType::FieldIterator::HasNext() returns false");
    }
    const auto* field = descriptor_.field(index_);
    CEL_ASSIGN_OR_RETURN(auto type, FieldDescriptorToType(type_manager, field));
    ++index_;
    return StructType::Field(ProtoStructType::MakeFieldId(field->number()),
                             field->name(), field->number(), std::move(type),
                             field);
  }

  absl::StatusOr<FieldId> NextId(TypeManager& type_manager) override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.field_count())) {
      return absl::FailedPreconditionError(
          "StructType::FieldIterator::Next() called when "
          "StructType::FieldIterator::HasNext() returns false");
    }
    return ProtoStructType::MakeFieldId(descriptor_.field(index_++)->number());
  }

  absl::StatusOr<absl::string_view> NextName(
      TypeManager& type_manager) override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.field_count())) {
      return absl::FailedPreconditionError(
          "StructType::FieldIterator::Next() called when "
          "StructType::FieldIterator::HasNext() returns false");
    }
    return descriptor_.field(index_++)->name();
  }

  absl::StatusOr<int64_t> NextNumber(TypeManager& type_manager) override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.field_count())) {
      return absl::FailedPreconditionError(
          "StructType::FieldIterator::Next() called when "
          "StructType::FieldIterator::HasNext() returns false");
    }
    return descriptor_.field(index_++)->number();
  }

 private:
  const google::protobuf::Descriptor& descriptor_;
  int index_ = 0;
};

size_t ProtoStructType::field_count() const {
  return descriptor().field_count();
}

absl::StatusOr<UniqueRef<StructType::FieldIterator>>
ProtoStructType::NewFieldIterator(MemoryManager& memory_manager) const {
  return MakeUnique<ProtoStructTypeFieldIterator>(memory_manager, descriptor());
}

absl::StatusOr<absl::optional<ProtoStructType::Field>>
ProtoStructType::FindFieldByName(TypeManager& type_manager,
                                 absl::string_view name) const {
  const auto* field_desc = descriptor().FindFieldByName(name);
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return absl::nullopt;
  }
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToType(type_manager, field_desc));
  return Field(MakeFieldId(field_desc->number()), field_desc->name(),
               field_desc->number(), std::move(type), field_desc);
}

absl::StatusOr<absl::optional<ProtoStructType::Field>>
ProtoStructType::FindFieldByNumber(TypeManager& type_manager,
                                   int64_t number) const {
  if (ABSL_PREDICT_FALSE(number < std::numeric_limits<int>::min() ||
                         number > std::numeric_limits<int>::max())) {
    // Treat it as not found.
    return absl::nullopt;
  }
  const auto* field_desc =
      descriptor().FindFieldByNumber(static_cast<int>(number));
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return absl::nullopt;
  }
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToType(type_manager, field_desc));
  return Field(MakeFieldId(field_desc->number()), field_desc->name(),
               field_desc->number(), std::move(type), field_desc);
}

absl::Status TypeConversionError(const Type& from, const Type& to) {
  return absl::InvalidArgumentError(absl::StrCat("type conversion error from ",
                                                 from.DebugString(), " to ",
                                                 to.DebugString()));
}

template <typename F, typename T>
struct CheckedCast;

template <typename T>
struct CheckedCast<T, T> {
  static absl::StatusOr<T> Cast(T value) { return value; }
};

template <>
struct CheckedCast<double, float> {
  static absl::StatusOr<float> Cast(double value) {
    if (ABSL_PREDICT_FALSE(static_cast<double>(static_cast<float>(value)) !=
                           value)) {
      return absl::OutOfRangeError("double to float overflow");
    }
    return static_cast<float>(value);
  }
};

template <>
struct CheckedCast<int64_t, int32_t> {
  static absl::StatusOr<int32_t> Cast(int64_t value) {
    if (ABSL_PREDICT_FALSE(value < std::numeric_limits<int32_t>::min() ||
                           value > std::numeric_limits<int32_t>::max())) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    return static_cast<int32_t>(value);
  }
};

template <>
struct CheckedCast<uint64_t, uint32_t> {
  static absl::StatusOr<uint32_t> Cast(uint64_t value) {
    if (ABSL_PREDICT_FALSE(value > std::numeric_limits<int32_t>::max())) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    return static_cast<uint32_t>(value);
  }
};

class ProtoStructValueBuilder final : public StructValueBuilderInterface {
 public:
  ProtoStructValueBuilder(ValueFactory& value_factory,
                          Handle<ProtoStructType> type,
                          google::protobuf::MessageFactory* factory,
                          google::protobuf::Message* message)
      : value_factory_(value_factory),
        type_(std::move(type)),
        factory_(factory),
        message_(ABSL_DIE_IF_NULL(message)) {}  // Crash OK

  ~ProtoStructValueBuilder() override {
    if (message_ != nullptr && message_->GetArena() == nullptr) {
      delete message_;
    }
  }

  absl::Status SetFieldByName(absl::string_view name,
                              Handle<Value> value) override {
    CEL_ASSIGN_OR_RETURN(
        auto field_type,
        type_->FindFieldByName(value_factory_.type_manager(), name));
    if (ABSL_PREDICT_FALSE(!field_type)) {
      return interop_internal::CreateNoSuchFieldError(name);
    }
    return SetField(*field_type, std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    CEL_ASSIGN_OR_RETURN(
        auto field_type,
        type_->FindFieldByNumber(value_factory_.type_manager(), number));
    if (ABSL_PREDICT_FALSE(!field_type)) {
      return interop_internal::CreateNoSuchFieldError(absl::StrCat(number));
    }
    return SetField(*field_type, std::move(value));
  }

  absl::StatusOr<Handle<StructValue>> Build() && override {
    return protobuf_internal::ParsedProtoStructValue::Create(
        value_factory_, std::move(*message_));
  }

 private:
  absl::Status SetField(const StructType::Field& field, Handle<Value>&& value) {
    const auto* field_desc =
        static_cast<const google::protobuf::FieldDescriptor*>(field.hint);
    const auto* reflect = message_->GetReflection();
    if (field_desc->is_map()) {
      return SetMapField(field, *reflect, *field_desc, std::move(value));
    }
    if (field_desc->is_repeated()) {
      if (ABSL_PREDICT_FALSE(!value->Is<ListValue>())) {
        return TypeConversionError(*field.type, *value->type());
      }
      return SetRepeatedField(field, *reflect, *field_desc,
                              std::move(value).As<ListValue>());
    }
    return SetSingularField(field, *reflect, *field_desc, std::move(value));
  }

  absl::Status SetMapField(const StructType::Field& field,
                           const google::protobuf::Reflection& reflect,
                           const google::protobuf::FieldDescriptor& field_desc,
                           Handle<Value>&& value) {
    return absl::UnimplementedError(
        "StructValueBuilderInterface::SetField does not yet implement support "
        "for setting map fields");
  }

  // Sets a repeated scalar field. `T` is a subclass of `cel::Type`,
  // `V` is a subclass of `cel::Value`, and `P` is the primitive C++ type.
  template <typename T, typename V, typename P>
  absl::Status SetRepeatedScalarField(
      const ListType& from_list_type, const ListType& to_list_type,
      const ListValue& value, const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) {
    const auto& to_element_type = *to_list_type.element();
    const auto& from_element_type = *from_list_type.element();
    ABSL_DCHECK(to_element_type.Is<T>());
    if (ABSL_PREDICT_FALSE(!from_element_type.Is<T>() &&
                           !from_element_type.Is<DynType>())) {
      return TypeConversionError(from_list_type, to_list_type);
    }
    auto repeated_field_ref =
        reflect.GetMutableRepeatedFieldRef<P>(message_, &field_desc);
    repeated_field_ref.Clear();
    CEL_ASSIGN_OR_RETURN(auto iterator,
                         value.NewIterator(value_factory_.memory_manager()));
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(
          auto element,
          iterator->NextValue(ListValue::GetContext(value_factory_)));
      if (ABSL_PREDICT_FALSE(!element->Is<V>())) {
        return TypeConversionError(*element->type(), to_element_type);
      }
      CEL_ASSIGN_OR_RETURN(
          auto casted_element,
          (CheckedCast<typename base_internal::ValueTraits<V>::underlying_type,
                       P>::Cast(element->As<V>().value())));
      repeated_field_ref.Add(std::move(casted_element));
    }
    return absl::OkStatus();
  }

  // Sets a repeated string field. `T` is a subclass of `cel::Type` and
  // `V` is a subclass of `cel::Value`. This is used for bytes and strings.
  template <typename T, typename V>
  absl::Status SetRepeatedStringField(
      const ListType& from_list_type, const ListType& to_list_type,
      const ListValue& value, const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) {
    const auto& to_element_type = *to_list_type.element();
    const auto& from_element_type = *from_list_type.element();
    ABSL_DCHECK(to_element_type.Is<T>());
    if (ABSL_PREDICT_FALSE(!from_element_type.Is<T>() &&
                           !from_element_type.Is<DynType>())) {
      return TypeConversionError(from_list_type, to_list_type);
    }
    auto repeated_field_ref =
        reflect.GetMutableRepeatedFieldRef<std::string>(message_, &field_desc);
    repeated_field_ref.Clear();
    CEL_ASSIGN_OR_RETURN(auto iterator,
                         value.NewIterator(value_factory_.memory_manager()));
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(
          auto element,
          iterator->NextValue(ListValue::GetContext(value_factory_)));
      if (ABSL_PREDICT_FALSE(!element->Is<V>())) {
        return TypeConversionError(*element->type(), to_element_type);
      }
      repeated_field_ref.Add(element->As<V>().ToString());
    }
    return absl::OkStatus();
  }

  // Sets an enum repeated field.
  absl::Status SetRepeatedEnumField(const ListType& from_list_type,
                                    const ListType& to_list_type,
                                    const ListValue& value,
                                    const google::protobuf::Reflection& reflect,
                                    const google::protobuf::FieldDescriptor& field_desc) {
    const auto& to_element_type = *to_list_type.element();
    const auto& from_element_type = *from_list_type.element();
    if (to_element_type.Is<NullType>()) {
      // google.protobuf.NullValue
      if (ABSL_PREDICT_FALSE(!from_element_type.Is<NullType>() &&
                             !from_element_type.Is<DynType>())) {
        return TypeConversionError(from_list_type, to_list_type);
      }
      auto repeated_field_ref =
          reflect.GetMutableRepeatedFieldRef<int32_t>(message_, &field_desc);
      repeated_field_ref.Clear();
      CEL_ASSIGN_OR_RETURN(auto iterator,
                           value.NewIterator(value_factory_.memory_manager()));
      while (iterator->HasNext()) {
        CEL_ASSIGN_OR_RETURN(
            auto element,
            iterator->NextValue(ListValue::GetContext(value_factory_)));
        if (ABSL_PREDICT_FALSE(!element->Is<NullValue>())) {
          return TypeConversionError(*element->type(), to_element_type);
        }
        repeated_field_ref.Add(static_cast<int>(google::protobuf::NULL_VALUE));
      }
      return absl::OkStatus();
    }
    ABSL_DCHECK(to_element_type.Is<EnumType>());
    if (ABSL_PREDICT_FALSE(!from_element_type.Is<EnumType>() &&
                           !from_element_type.Is<IntType>() &&
                           !from_element_type.Is<DynType>())) {
      return TypeConversionError(from_list_type, to_list_type);
    }
    auto repeated_field_ref =
        reflect.GetMutableRepeatedFieldRef<int32_t>(message_, &field_desc);
    repeated_field_ref.Clear();
    CEL_ASSIGN_OR_RETURN(auto iterator,
                         value.NewIterator(value_factory_.memory_manager()));
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(
          auto element,
          iterator->NextValue(ListValue::GetContext(value_factory_)));
      if (element->Is<EnumValue>()) {
        if (ABSL_PREDICT_FALSE(element->As<EnumValue>().type()->name() !=
                               field_desc.enum_type()->full_name())) {
          return TypeConversionError(*element->type(), to_element_type);
        }
        int64_t raw_value = element->As<EnumValue>().number();
        if (ABSL_PREDICT_FALSE(raw_value < std::numeric_limits<int>::min() ||
                               raw_value > std::numeric_limits<int>::max())) {
          return absl::OutOfRangeError("int64 to int32_t overflow");
        }
        repeated_field_ref.Add(static_cast<int>(raw_value));
      } else if (element->Is<IntValue>()) {
        int64_t raw_value = element->As<IntValue>().value();
        if (ABSL_PREDICT_FALSE(raw_value < std::numeric_limits<int>::min() ||
                               raw_value > std::numeric_limits<int>::max())) {
          return absl::OutOfRangeError("int64 to int32_t overflow");
        }
        repeated_field_ref.Add(static_cast<int>(raw_value));
      } else {
        return TypeConversionError(*element->type(), to_element_type);
      }
    }
    return absl::OkStatus();
  }

  // Sets a `google::protobuf::Message` repeated field. `T` is a subclass of `cel::Type`,
  // `V` is a subclass of `cel::Value`, and `P` is the primitive C++ type.
  // `valuer` extracts the primitive C++ type from an instance of `V` and
  // `wrapper` converts the C++ primitive type to the protobuf wrapper type.
  template <typename T, typename V, typename P>
  absl::Status SetRepeatedWrapperMessageField(
      const ListType& from_list_type, const ListType& to_list_type,
      const Type& to_element_type, const ListValue& value,
      const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc,
      absl::FunctionRef<std::decay_t<P>(const V&)> valuer,
      absl::FunctionRef<absl::Status(google::protobuf::Message&, P)> wrapper) {
    const auto& from_element_type = *from_list_type.element();
    ABSL_DCHECK(to_element_type.Is<T>());
    if (ABSL_PREDICT_FALSE(!from_element_type.Is<T>() &&
                           !from_element_type.Is<DynType>())) {
      return TypeConversionError(from_list_type, to_list_type);
    }
    auto repeated_field_ref =
        reflect.GetMutableRepeatedFieldRef<google::protobuf::Message>(message_,
                                                            &field_desc);
    repeated_field_ref.Clear();
    CEL_ASSIGN_OR_RETURN(auto iterator,
                         value.NewIterator(value_factory_.memory_manager()));
    auto scratch = absl::WrapUnique(repeated_field_ref.NewMessage());
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(
          auto element,
          iterator->NextValue(ListValue::GetContext(value_factory_)));
      if (ABSL_PREDICT_FALSE(!element->Is<V>())) {
        return TypeConversionError(*element->type(), to_element_type);
      }
      scratch->Clear();
      CEL_RETURN_IF_ERROR(wrapper(*scratch, valuer(element->As<V>())));
      repeated_field_ref.Add(*scratch);
    }
    return absl::OkStatus();
  }

  absl::Status SetRepeatedMessageField(
      const ListType& from_list_type, const ListType& to_list_type,
      const ListValue& value, const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) {
    const auto& to_element_type = *to_list_type.element();
    switch (to_element_type.kind()) {
      case TypeKind::kAny: {
        // google.protobuf.Any
        return absl::UnimplementedError(
            "StructValueBuilderInterface::SetField does not yet implement "
            "google.protobuf.Any support");
      }
      case TypeKind::kDyn: {
        // google.protobuf.Value
        return absl::UnimplementedError(
            "StructValueBuilderInterface::SetField does not yet implement "
            "google.protobuf.Value support");
      }
      case TypeKind::kList: {
        // google.protobuf.ListValue
        return absl::UnimplementedError(
            "StructValueBuilderInterface::SetField does not yet implement "
            "google.protobuf.ListValue support");
      }
      case TypeKind::kMap: {
        // google.protobuf.Struct
        return absl::UnimplementedError(
            "StructValueBuilderInterface::SetField does not yet implement "
            "google.protobuf.Struct support");
      }
      case TypeKind::kDuration:
        // google.protobuf.Duration
        return SetRepeatedWrapperMessageField<DurationType, DurationValue,
                                              absl::Duration>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc,
            [](const DurationValue& value) { return value.value(); },
            protobuf_internal::AbslDurationToDurationProto);
      case TypeKind::kTimestamp:
        // google.protobuf.Timestamp
        return SetRepeatedWrapperMessageField<TimestampType, TimestampValue,
                                              absl::Time>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc,
            [](const TimestampValue& value) { return value.value(); },
            protobuf_internal::AbslTimeToTimestampProto);
      case TypeKind::kBool:
        // google.protobuf.BoolValue
        return SetRepeatedWrapperMessageField<BoolType, BoolValue, bool>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const BoolValue& value) { return value.value(); },
            protobuf_internal::WrapBoolValueProto);
      case TypeKind::kInt:
        // google.protobuf.{Int32,Int64}Value
        return SetRepeatedWrapperMessageField<IntType, IntValue, int64_t>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const IntValue& value) { return value.value(); },
            protobuf_internal::WrapIntValueProto);
      case TypeKind::kUint:
        // google.protobuf.{UInt32,UInt64}Value
        return SetRepeatedWrapperMessageField<UintType, UintValue, uint64_t>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const UintValue& value) { return value.value(); },
            protobuf_internal::WrapUIntValueProto);
      case TypeKind::kDouble:
        // google.protobuf.{Float,Double}Value
        return SetRepeatedWrapperMessageField<DoubleType, DoubleValue, double>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const DoubleValue& value) { return value.value(); },
            protobuf_internal::WrapDoubleValueProto);
      case TypeKind::kBytes:
        // google.protobuf.BytesValue
        return SetRepeatedWrapperMessageField<BytesType, BytesValue,
                                              const absl::Cord&>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const BytesValue& value) { return value.ToCord(); },
            protobuf_internal::WrapBytesValueProto);
      case TypeKind::kString:
        // google.protobuf.StringValue
        return SetRepeatedWrapperMessageField<StringType, StringValue,
                                              const absl::Cord&>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const StringValue& value) { return value.ToCord(); },
            protobuf_internal::WrapStringValueProto);
      case TypeKind::kStruct: {
        auto repeated_field_ref =
            reflect.GetMutableRepeatedFieldRef<google::protobuf::Message>(message_,
                                                                &field_desc);
        repeated_field_ref.Clear();
        CEL_ASSIGN_OR_RETURN(
            auto iterator, value.NewIterator(value_factory_.memory_manager()));
        auto scratch = absl::WrapUnique(repeated_field_ref.NewMessage());
        while (iterator->HasNext()) {
          CEL_ASSIGN_OR_RETURN(
              auto element,
              iterator->NextValue(ListValue::GetContext(value_factory_)));
          if (ABSL_PREDICT_FALSE(!element->Is<ProtoStructValue>())) {
            return TypeConversionError(*element->type(), to_element_type);
          }
          scratch->Clear();
          CEL_RETURN_IF_ERROR(element->As<ProtoStructValue>().CopyTo(*scratch));
          repeated_field_ref.Add(*scratch);
        }
        return absl::OkStatus();
      }
      default:
        return TypeConversionError(from_list_type, to_list_type);
    }
  }

  absl::Status SetRepeatedField(const StructType::Field& field,
                                const google::protobuf::Reflection& reflect,
                                const google::protobuf::FieldDescriptor& field_desc,
                                Handle<ListValue>&& value) {
    const auto& to_list_type = field.type->As<ListType>();
    auto from_list_type = value->type();
    switch (field_desc.type()) {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
        return SetRepeatedScalarField<DoubleType, DoubleValue, double>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
        return SetRepeatedScalarField<DoubleType, DoubleValue, float>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
        return SetRepeatedScalarField<UintType, UintValue, uint64_t>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
        return SetRepeatedScalarField<BoolType, BoolValue, bool>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_STRING:
        return SetRepeatedStringField<StringType, StringValue>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
        return SetRepeatedMessageField(*from_list_type, to_list_type, *value,
                                       reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_BYTES:
        return SetRepeatedStringField<BytesType, BytesValue>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
        return SetRepeatedScalarField<UintType, UintValue, uint32_t>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_ENUM:
        return SetRepeatedEnumField(*from_list_type, to_list_type, *value,
                                    reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
        return SetRepeatedScalarField<IntType, IntValue, int32_t>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
        return SetRepeatedScalarField<IntType, IntValue, int64_t>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
    }
  }

  absl::Status SetSingularMessageField(
      const StructType::Field& field, const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc, Handle<Value>&& value) {
    switch (field.type->kind()) {
      case TypeKind::kAny: {
        // google.protobuf.Any
        return absl::UnimplementedError(
            "StructValueBuilderInterface::SetField does not yet implement "
            "google.protobuf.Any support");
      }
      case TypeKind::kDyn: {
        // google.protobuf.Value
        return absl::UnimplementedError(
            "StructValueBuilderInterface::SetField does not yet implement "
            "google.protobuf.Value support");
      }
      case TypeKind::kList: {
        // google.protobuf.ListValue
        return absl::UnimplementedError(
            "StructValueBuilderInterface::SetField does not yet implement "
            "google.protobuf.ListValue support");
      }
      case TypeKind::kMap: {
        // google.protobuf.Struct
        return absl::UnimplementedError(
            "StructValueBuilderInterface::SetField does not yet implement "
            "google.protobuf.Struct support");
      }
      case TypeKind::kDuration: {
        // google.protobuf.Duration
        if (ABSL_PREDICT_FALSE(!value->Is<DurationValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        return protobuf_internal::AbslDurationToDurationProto(
            *reflect.MutableMessage(message_, &field_desc, factory_),
            value->As<DurationValue>().value());
      }
      case TypeKind::kTimestamp: {
        // google.protobuf.Timestamp
        if (ABSL_PREDICT_FALSE(!value->Is<TimestampValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        return protobuf_internal::AbslTimeToTimestampProto(
            *reflect.MutableMessage(message_, &field_desc, factory_),
            value->As<TimestampValue>().value());
      }
      case TypeKind::kWrapper: {
        if (value->Is<NullValue>()) {
          reflect.ClearField(message_, &field_desc);
          return absl::OkStatus();
        }
        switch (field.type->As<WrapperType>().wrapped()->kind()) {
          case TypeKind::kBool: {
            // google.protobuf.BoolValue
            if (ABSL_PREDICT_FALSE(!value->Is<BoolValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapBoolValueProto(
                *reflect.MutableMessage(message_, &field_desc, factory_),
                value->As<BoolValue>().value());
          }
          case TypeKind::kInt: {
            // google.protobuf.{Int32,Int64}Value
            if (ABSL_PREDICT_FALSE(!value->Is<IntValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapIntValueProto(
                *reflect.MutableMessage(message_, &field_desc, factory_),
                value->As<IntValue>().value());
          }
          case TypeKind::kUint: {
            // google.protobuf.{UInt32,UInt64}Value
            if (ABSL_PREDICT_FALSE(!value->Is<UintValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapUIntValueProto(
                *reflect.MutableMessage(message_, &field_desc, factory_),
                value->As<UintValue>().value());
          }
          case TypeKind::kDouble: {
            // google.protobuf.{Float,Double}Value
            if (ABSL_PREDICT_FALSE(!value->Is<DoubleValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapDoubleValueProto(
                *reflect.MutableMessage(message_, &field_desc, factory_),
                value->As<DoubleValue>().value());
          }
          case TypeKind::kBytes: {
            // google.protobuf.BytesValue
            if (ABSL_PREDICT_FALSE(!value->Is<BytesValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapBytesValueProto(
                *reflect.MutableMessage(message_, &field_desc, factory_),
                value->As<BytesValue>().ToCord());
          }
          case TypeKind::kString: {
            // google.protobuf.StringValue
            if (ABSL_PREDICT_FALSE(!value->Is<StringValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapStringValueProto(
                *reflect.MutableMessage(message_, &field_desc, factory_),
                value->As<StringValue>().ToCord());
          }
          default:
            // There are only 6 wrapper types.
            ABSL_UNREACHABLE();
        }
      }
      case TypeKind::kStruct: {
        if (ABSL_PREDICT_FALSE(!value->Is<ProtoStructValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        return value->As<ProtoStructValue>().CopyTo(
            *reflect.MutableMessage(message_, &field_desc, factory_));
      }
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "type conversion error from ", field.type->DebugString(), " to ",
            value->type()->DebugString()));
    }
  }

  absl::Status SetSingularField(const StructType::Field& field,
                                const google::protobuf::Reflection& reflect,
                                const google::protobuf::FieldDescriptor& field_desc,
                                Handle<Value>&& value) {
    switch (field_desc.type()) {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE: {
        if (ABSL_PREDICT_FALSE(!value->Is<DoubleValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        reflect.SetDouble(message_, &field_desc,
                          value->As<DoubleValue>().value());
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_FLOAT: {
        if (ABSL_PREDICT_FALSE(!value->Is<DoubleValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        auto raw_value = value->As<DoubleValue>().value();
        if (ABSL_PREDICT_FALSE(static_cast<double>(static_cast<float>(
                                   raw_value)) != raw_value)) {
          return absl::InvalidArgumentError("double to float overflow");
        }
        reflect.SetFloat(message_, &field_desc, static_cast<float>(raw_value));
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT64: {
        if (ABSL_PREDICT_FALSE(!value->Is<UintValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        reflect.SetUInt64(message_, &field_desc,
                          value->As<UintValue>().value());
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_BOOL: {
        if (ABSL_PREDICT_FALSE(!value->Is<BoolValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        reflect.SetBool(message_, &field_desc, value->As<BoolValue>().value());
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_STRING: {
        if (ABSL_PREDICT_FALSE(!value->Is<StringValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        if (protobuf_internal::IsCordField(field_desc)) {
          reflect.SetString(message_, &field_desc,
                            value->As<StringValue>().ToCord());
        } else {
          reflect.SetString(message_, &field_desc,
                            value->As<StringValue>().ToString());
        }
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
        return SetSingularMessageField(field, reflect, field_desc,
                                       std::move(value));
      case google::protobuf::FieldDescriptor::TYPE_BYTES: {
        if (ABSL_PREDICT_FALSE(!value->Is<BytesValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        if (protobuf_internal::IsCordField(field_desc)) {
          reflect.SetString(message_, &field_desc,
                            value->As<BytesValue>().ToCord());
        } else {
          reflect.SetString(message_, &field_desc,
                            value->As<BytesValue>().ToString());
        }
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT32: {
        if (ABSL_PREDICT_FALSE(!value->Is<UintValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        auto raw_value = value->As<UintValue>().value();
        if (ABSL_PREDICT_FALSE(raw_value >
                               std::numeric_limits<uint32_t>::max())) {
          return absl::InvalidArgumentError("uint64 to uint32_t overflow");
        }
        reflect.SetUInt32(message_, &field_desc,
                          static_cast<uint32_t>(raw_value));
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_ENUM: {
        if (value->Is<NullValue>()) {
          // google.protobuf.NullValue
          reflect.ClearField(message_, &field_desc);
          return absl::OkStatus();
        }
        int64_t raw_value;
        if (value->Is<IntValue>()) {
          raw_value = value->As<IntValue>().value();
        } else if (value->Is<EnumValue>()) {
          raw_value = value->As<EnumValue>().number();
        } else {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        if (ABSL_PREDICT_FALSE(raw_value < std::numeric_limits<int>::min() ||
                               raw_value > std::numeric_limits<int>::max())) {
          return absl::InvalidArgumentError("int64 to int32_t overflow");
        }
        reflect.SetEnumValue(message_, &field_desc,
                             static_cast<int>(raw_value));
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT32: {
        if (ABSL_PREDICT_FALSE(!value->Is<IntValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        auto raw_value = value->As<IntValue>().value();
        if (ABSL_PREDICT_FALSE(
                raw_value < std::numeric_limits<int32_t>::min() ||
                raw_value > std::numeric_limits<int32_t>::max())) {
          return absl::InvalidArgumentError("int64 to int32_t overflow");
        }
        reflect.SetInt32(message_, &field_desc,
                         static_cast<int32_t>(raw_value));
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT64: {
        if (ABSL_PREDICT_FALSE(!value->Is<IntValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        reflect.SetInt64(message_, &field_desc, value->As<IntValue>().value());
        return absl::OkStatus();
      }
    }
  }

  ValueFactory& value_factory_;
  Handle<ProtoStructType> type_;
  google::protobuf::MessageFactory* const factory_;
  google::protobuf::Message* message_;
};

absl::StatusOr<UniqueRef<StructValueBuilderInterface>>
ProtoStructType::NewValueBuilder(ValueFactory& value_factory) const {
  const auto* prototype = factory_->GetPrototype(&descriptor());
  if (prototype == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Unable to retrieve prototype from protocol buffer "
                     "message factory for type ",
                     descriptor().full_name()));
  }
  google::protobuf::Message* message;
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    message = prototype->New(
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager()));
  } else {
    message = prototype->New();
  }
  return MakeUnique<ProtoStructValueBuilder>(
      value_factory.memory_manager(), value_factory,
      handle_from_this().As<ProtoStructType>(), factory_, message);
}

}  // namespace cel::extensions
