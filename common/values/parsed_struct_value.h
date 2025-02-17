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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_manager.h"
#include "common/value_kind.h"
#include "common/values/struct_value_interface.h"

namespace cel {

class ParsedStructValueInterface;
class ParsedStructValue;
class ParsedStructValueView;
class Value;
class ValueView;
class ValueManager;

class ParsedStructValueInterface : public StructValueInterface {
 public:
  using alternative_type = ParsedStructValue;
  using view_alternative_type = ParsedStructValueView;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  virtual bool IsZeroValue() const = 0;

  virtual absl::StatusOr<ValueView> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const = 0;

  virtual absl::StatusOr<ValueView> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const = 0;

  virtual absl::StatusOr<bool> HasFieldByName(absl::string_view name) const = 0;

  virtual absl::StatusOr<bool> HasFieldByNumber(int64_t number) const = 0;

  virtual absl::Status ForEachField(ValueManager& value_manager,
                                    ForEachFieldCallback callback) const = 0;

  virtual absl::StatusOr<std::pair<ValueView, int>> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

 private:
  virtual absl::StatusOr<ValueView> EqualImpl(
      ValueManager& value_manager, ParsedStructValueView other,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const = 0;
};

class ParsedStructValue {
 public:
  using interface_type = ParsedStructValueInterface;
  using view_alternative_type = ParsedStructValueView;

  static constexpr ValueKind kKind = ParsedStructValueInterface::kKind;

  explicit ParsedStructValue(ParsedStructValueView value);

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedStructValue(Shared<const ParsedStructValueInterface> interface)
      : interface_(std::move(interface)) {}

  ParsedStructValue(const ParsedStructValue&) = default;
  ParsedStructValue(ParsedStructValue&&) = default;
  ParsedStructValue& operator=(const ParsedStructValue&) = default;
  ParsedStructValue& operator=(ParsedStructValue&&) = default;

  ValueKind kind() const { return interface_->kind(); }

  StructType GetType(TypeManager& type_manager) const {
    return interface_->GetType(type_manager);
  }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  absl::StatusOr<size_t> GetSerializedSize() const {
    return interface_->GetSerializedSize();
  }

  absl::Status SerializeTo(absl::Cord& value) const {
    return interface_->SerializeTo(value);
  }

  absl::StatusOr<absl::Cord> Serialize() const {
    return interface_->Serialize();
  }

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->GetTypeUrl(prefix);
  }

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->ConvertToAny(prefix);
  }

  absl::StatusOr<Json> ConvertToJson() const {
    return interface_->ConvertToJson();
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject() const {
    return interface_->ConvertToJsonObject();
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  void swap(ParsedStructValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  absl::StatusOr<ValueView> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ValueView> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const {
    return interface_->HasFieldByName(name);
  }

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const {
    return interface_->HasFieldByNumber(number);
  }

  using ForEachFieldCallback = StructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<std::pair<ValueView, int>> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

 private:
  friend class ParsedStructValueView;
  friend struct NativeTypeTraits<ParsedStructValue>;

  Shared<const ParsedStructValueInterface> interface_;
};

inline void swap(ParsedStructValue& lhs, ParsedStructValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedStructValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<ParsedStructValue> final {
  static NativeTypeId Id(const ParsedStructValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const ParsedStructValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<
    T, std::enable_if_t<
           std::conjunction_v<std::negation<std::is_same<ParsedStructValue, T>>,
                              std::is_base_of<ParsedStructValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<ParsedStructValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<ParsedStructValue>::SkipDestructor(type);
  }
};

class ParsedStructValueView {
 public:
  using interface_type = ParsedStructValueInterface;
  using alternative_type = ParsedStructValue;

  static constexpr ValueKind kKind = ParsedStructValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedStructValueView(SharedView<const ParsedStructValueInterface> interface)
      : interface_(interface) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedStructValueView(
      const ParsedStructValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : interface_(value.interface_) {}

  ParsedStructValueView(ParsedStructValue&&) = delete;

  ParsedStructValueView(const ParsedStructValueView&) = default;
  ParsedStructValueView& operator=(const ParsedStructValueView&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedStructValueView& operator=(
      const ParsedStructValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    interface_ = value.interface_;
    return *this;
  }

  ParsedStructValueView& operator=(ParsedStructValue&&) = delete;

  ValueKind kind() const { return interface_->kind(); }

  StructType GetType(TypeManager& type_manager) const {
    return interface_->GetType(type_manager);
  }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  absl::StatusOr<size_t> GetSerializedSize() const {
    return interface_->GetSerializedSize();
  }

  absl::Status SerializeTo(absl::Cord& value) const {
    return interface_->SerializeTo(value);
  }

  absl::StatusOr<absl::Cord> Serialize() const {
    return interface_->Serialize();
  }

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->GetTypeUrl(prefix);
  }

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->ConvertToAny(prefix);
  }

  absl::StatusOr<Json> ConvertToJson() const {
    return interface_->ConvertToJson();
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject() const {
    return interface_->ConvertToJsonObject();
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  void swap(ParsedStructValueView& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  absl::StatusOr<ValueView> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ValueView> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const {
    return interface_->HasFieldByName(name);
  }

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const {
    return interface_->HasFieldByNumber(number);
  }

  using ForEachFieldCallback = StructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<std::pair<ValueView, int>> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

 private:
  friend class ParsedStructValue;
  friend struct NativeTypeTraits<ParsedStructValueView>;

  SharedView<const ParsedStructValueInterface> interface_;
};

inline void swap(ParsedStructValueView& lhs,
                 ParsedStructValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                ParsedStructValueView value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<ParsedStructValueView> final {
  static NativeTypeId Id(ParsedStructValueView type) {
    return NativeTypeId::Of(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<
    T, std::enable_if_t<std::conjunction_v<
           std::negation<std::is_same<ParsedStructValueView, T>>,
           std::is_base_of<ParsedStructValueView, T>>>>
    final {
  static NativeTypeId Id(T type) {
    return NativeTypeTraits<ParsedStructValueView>::Id(type);
  }
};

inline ParsedStructValue::ParsedStructValue(ParsedStructValueView value)
    : interface_(value.interface_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_
