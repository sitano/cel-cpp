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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_manager.h"
#include "common/value_interface.h"  // IWYU pragma: export
#include "common/value_kind.h"
#include "common/values/bool_value.h"  // IWYU pragma: export
#include "common/values/bytes_value.h"  // IWYU pragma: export
#include "common/values/double_value.h"  // IWYU pragma: export
#include "common/values/duration_value.h"  // IWYU pragma: export
#include "common/values/error_value.h"  // IWYU pragma: export
#include "common/values/int_value.h"  // IWYU pragma: export
#include "common/values/list_value.h"  // IWYU pragma: export
#include "common/values/map_value.h"  // IWYU pragma: export
#include "common/values/null_value.h"  // IWYU pragma: export
#include "common/values/opaque_value.h"  // IWYU pragma: export
#include "common/values/optional_value.h"  // IWYU pragma: export
#include "common/values/string_value.h"  // IWYU pragma: export
#include "common/values/struct_value.h"  // IWYU pragma: export
#include "common/values/timestamp_value.h"  // IWYU pragma: export
#include "common/values/type_value.h"  // IWYU pragma: export
#include "common/values/uint_value.h"  // IWYU pragma: export
#include "common/values/unknown_value.h"  // IWYU pragma: export
#include "common/values/values.h"

namespace cel {

class Value;
class ValueView;

// `Value` is a composition type which encompasses all values supported by the
// Common Expression Language. When default constructed or moved, `Value` is in
// a known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
class Value final {
 public:
  using view_alternative_type = ValueView;

  Value() = default;

  Value(const Value& other)
      : variant_((other.AssertIsValid(), other.variant_)) {}

  Value& operator=(const Value& other) {
    other.AssertIsValid();
    ABSL_DCHECK(this != std::addressof(other))
        << "Value should not be copied to itself";
    variant_ = other.variant_;
    return *this;
  }

  Value(Value&& other) noexcept
      : variant_((other.AssertIsValid(), std::move(other.variant_))) {
    other.variant_.emplace<absl::monostate>();
  }

  Value& operator=(Value&& other) noexcept {
    other.AssertIsValid();
    ABSL_DCHECK(this != std::addressof(other))
        << "Value should not be moved to itself";
    variant_ = std::move(other.variant_);
    other.variant_.emplace<absl::monostate>();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ListValue& value)
      : Value(CompositionTraits<ListValue>::Get<Value>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ListValue&& value)
      : Value(CompositionTraits<ListValue>::Get<Value>(std::move(value))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ListValue& value) {
    return *this = CompositionTraits<ListValue>::Get<Value>(value);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ListValue&& value) {
    return *this = CompositionTraits<ListValue>::Get<Value>(std::move(value));
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const MapValue& value)
      : Value(CompositionTraits<MapValue>::Get<Value>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(MapValue&& value)
      : Value(CompositionTraits<MapValue>::Get<Value>(std::move(value))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const MapValue& value) {
    return *this = CompositionTraits<MapValue>::Get<Value>(value);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(MapValue&& value) {
    return *this = CompositionTraits<MapValue>::Get<Value>(std::move(value));
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const StructValue& value)
      : Value(CompositionTraits<StructValue>::Get<Value>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(StructValue&& value)
      : Value(CompositionTraits<StructValue>::Get<Value>(std::move(value))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const StructValue& value) {
    return *this = CompositionTraits<StructValue>::Get<Value>(value);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(StructValue&& value) {
    return *this = CompositionTraits<StructValue>::Get<Value>(std::move(value));
  }

  explicit Value(ValueView other);

  Value& operator=(ValueView other);

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const Shared<const T>& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueAlternativeForT<T>>,
            interface) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(Shared<const T>&& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueAlternativeForT<T>>,
            std::move(interface)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(T&& alternative) noexcept
      : variant_(absl::in_place_type<common_internal::BaseValueAlternativeForT<
                     absl::remove_cvref_t<T>>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(T&& type) noexcept {
    variant_.emplace<
        common_internal::BaseValueAlternativeForT<absl::remove_cvref_t<T>>>(
        std::forward<T>(type));
    return *this;
  }

  ValueKind kind() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> ValueKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return ValueKind::kError. In debug
            // builds we cannot reach here.
            return ValueKind::kError;
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  Type GetType(TypeManager& type_manager) const {
    AssertIsValid();
    return absl::visit(
        [&type_manager](const auto& alternative) -> Type {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an invalid type. In debug
            // builds we cannot reach here.
            return Type();
          } else {
            return alternative.GetType(type_manager);
          }
        },
        variant_);
  }

  absl::string_view GetTypeName() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::string_view {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an empty string. In debug
            // builds we cannot reach here.
            return absl::string_view();
          } else {
            return alternative.GetTypeName();
          }
        },
        variant_);
  }

  std::string DebugString() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> std::string {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an empty string. In debug
            // builds we cannot reach here.
            return std::string();
          } else {
            return alternative.DebugString();
          }
        },
        variant_);
  }

  // `GetSerializedSize` determines the serialized byte size that would result
  // from serialization, without performing the serialization. If this value
  // does not support serialization, `FAILED_PRECONDITION` is returned. If this
  // value does not support calculating serialization size ahead of time,
  // `UNIMPLEMENTED` is returned.
  absl::StatusOr<size_t> GetSerializedSize() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::StatusOr<size_t> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid Value");
          } else {
            return alternative.GetSerializedSize();
          }
        },
        variant_);
  }

  // `SerializeTo` serializes this value and appends it to `value`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::Status SerializeTo(absl::Cord& value) const {
    AssertIsValid();
    return absl::visit(
        [&value](const auto& alternative) -> absl::Status {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid Value");
          } else {
            return alternative.SerializeTo(value);
          }
        },
        variant_);
  }

  // `Serialize` serializes this value and returns it as `absl::Cord`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::StatusOr<absl::Cord> Serialize() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::StatusOr<absl::Cord> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid Value");
          } else {
            return alternative.Serialize();
          }
        },
        variant_);
  }

  // 'GetTypeUrl' returns the type URL that can be used as the type URL for
  // `Any`. If this value does not support serialization, `FAILED_PRECONDITION`
  // is returned.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    AssertIsValid();
    return absl::visit(
        [prefix](const auto& alternative) -> absl::StatusOr<std::string> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid Value");
          } else {
            return alternative.GetTypeUrl(prefix);
          }
        },
        variant_);
  }

  // 'ConvertToAny' converts this value to `Any`. If this value does not support
  // serialization, `FAILED_PRECONDITION` is returned.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    AssertIsValid();
    return absl::visit(
        [prefix](const auto& alternative) -> absl::StatusOr<Any> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid Value");
          } else {
            return alternative.ConvertToAny(prefix);
          }
        },
        variant_);
  }

  absl::StatusOr<Json> ConvertToJson() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::StatusOr<Json> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug
            // builds we cannot reach here.
            return absl::InternalError("use of invalid Value");
          } else {
            return alternative.ConvertToJson();
          }
        },
        variant_);
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> bool {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return false. In debug
            // builds we cannot reach here.
            return false;
          } else {
            return alternative.IsZeroValue();
          }
        },
        variant_);
  }

  void swap(Value& other) noexcept {
    AssertIsValid();
    other.AssertIsValid();
    variant_.swap(other.variant_);
  }

  friend std::ostream& operator<<(std::ostream& out, const Value& value) {
    value.AssertIsValid();
    return absl::visit(
        [&out](const auto& alternative) -> std::ostream& {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we do nothing. In debug builds we cannot
            // reach here.
            return out;
          } else {
            return out << alternative;
          }
        },
        value.variant_);
  }

 private:
  friend class ValueView;
  friend struct NativeTypeTraits<Value>;
  friend struct CompositionTraits<Value>;

  common_internal::ValueViewVariant ToViewVariant() const {
    return absl::visit(
        [](const auto& alternative) -> common_internal::ValueViewVariant {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            return common_internal::ValueViewVariant{};
          } else {
            return common_internal::ValueViewVariant(
                absl::in_place_type<typename absl::remove_cvref_t<
                    decltype(alternative)>::view_alternative_type>,
                alternative);
          }
        },
        variant_);
  }

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid Value";
  }

  common_internal::ValueVariant variant_;
};

inline void swap(Value& lhs, Value& rhs) noexcept { lhs.swap(rhs); }

template <>
struct NativeTypeTraits<Value> final {
  static NativeTypeId Id(const Value& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return
            // `NativeTypeId::For<absl::monostate>()`. In debug builds we cannot
            // reach here.
            return NativeTypeId::For<absl::monostate>();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        value.variant_);
  }

  static bool SkipDestructor(const Value& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> bool {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just say we should skip the destructor.
            // In debug builds we cannot reach here.
            return true;
          } else {
            return NativeType::SkipDestructor(alternative);
          }
        },
        value.variant_);
  }
};

template <>
struct CompositionTraits<Value> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return absl::holds_alternative<Base>(value.variant_) &&
             InstanceOf<U>(Get<U>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyListValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedListValue>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyMapValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedMapValue>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyStructValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedStructValue>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, const U&>
  Get(const Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U&> Get(
      Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U> Get(
      Value&& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedListValue>(std::move(value.variant_))};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedMapValue>(std::move(value.variant_))};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedStructValue>(std::move(value.variant_))};
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<Value, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<Value>);
static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_copy_assignable_v<Value>);
static_assert(std::is_nothrow_move_constructible_v<Value>);
static_assert(std::is_nothrow_move_assignable_v<Value>);
static_assert(std::is_nothrow_swappable_v<Value>);

// `ValueView` is a composition type which acts as a view of `Value` and its
// composed types. Like `Value`, it is also invalid when default constructed and
// must be assigned another type.
class ValueView final {
 public:
  using alternative_type = Value;

  ValueView() = default;
  ValueView(const ValueView&) = default;
  ValueView(ValueView&&) = default;
  ValueView& operator=(const ValueView&) = default;
  ValueView& operator=(ValueView&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(const Value& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : variant_((value.AssertIsValid(), value.ToViewVariant())) {}

  ValueView(Value&&) = delete;

  template <typename T, typename = std::enable_if_t<
                            common_internal::IsValueAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(const T& alternative ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : variant_(absl::in_place_type<
                     common_internal::BaseValueViewAlternativeForT<T>>,
                 alternative) {}

  template <typename T, typename = std::enable_if_t<
                            common_internal::IsValueViewAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(T alternative) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueViewAlternativeForT<
                absl::remove_cvref_t<T>>>,
            alternative) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(SharedView<const T> interface) noexcept
      : variant_(absl::in_place_type<
                     common_internal::BaseValueViewAlternativeForT<T>>,
                 interface) {}

  ValueView& operator=(const Value& other) {
    variant_ = (other.AssertIsValid(), other.ToViewVariant());
    return *this;
  }

  ValueView& operator=(Value&&) = delete;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(const ListValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ValueView(ListValueView(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(const MapValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ValueView(MapValueView(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(const StructValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ValueView(StructValueView(value)) {}

  ValueView(ListValue&&) = delete;

  ValueView(MapValue&&) = delete;

  ValueView(StructValue&&) = delete;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(ListValueView value)
      : ValueView(CompositionTraits<ListValueView>::Get<ValueView>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(MapValueView value)
      : ValueView(CompositionTraits<MapValueView>::Get<ValueView>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(StructValueView value)
      : ValueView(CompositionTraits<StructValueView>::Get<ValueView>(value)) {}

  template <typename T>
  std::enable_if_t<common_internal::IsValueAlternativeV<T>, ValueView&>
  operator=(const T& alternative ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    variant_.emplace<common_internal::BaseValueViewAlternativeForT<T>>(
        alternative);
    return *this;
  }

  template <typename T>
  std::enable_if_t<
      std::conjunction_v<
          std::negation<std::is_lvalue_reference<T>>,
          common_internal::IsValueAlternative<absl::remove_cvref_t<T>>>,
      ValueView&>
  operator=(T&&) = delete;

  template <typename T>
  std::enable_if_t<common_internal::IsValueViewAlternativeV<T>, ValueView&>
  operator=(T alternative) {
    variant_.emplace<
        common_internal::BaseValueViewAlternativeForT<absl::remove_cvref_t<T>>>(
        alternative);
    return *this;
  }

  ValueKind kind() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> ValueKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return ValueKind::kError. In debug
            // builds we cannot reach here.
            return ValueKind::kError;
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  Type GetType(TypeManager& type_manager) const {
    AssertIsValid();
    return absl::visit(
        [&type_manager](auto alternative) -> Type {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an invalid type. In debug
            // builds we cannot reach here.
            return Type();
          } else {
            return alternative.GetType(type_manager);
          }
        },
        variant_);
  }

  absl::string_view GetTypeName() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::string_view {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an empty string. In debug
            // builds we cannot reach here.
            return absl::string_view();
          } else {
            return alternative.GetTypeName();
          }
        },
        variant_);
  }

  std::string DebugString() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> std::string {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an empty string. In debug
            // builds we cannot reach here.
            return std::string();
          } else {
            return alternative.DebugString();
          }
        },
        variant_);
  }

  // `GetSerializedSize` determines the serialized byte size that would result
  // from serialization, without performing the serialization. If this value
  // does not support serialization, `FAILED_PRECONDITION` is returned. If this
  // value does not support calculating serialization size ahead of time,
  // `UNIMPLEMENTED` is returned.
  absl::StatusOr<size_t> GetSerializedSize() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::StatusOr<size_t> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid ValueView");
          } else {
            return alternative.GetSerializedSize();
          }
        },
        variant_);
  }

  // `SerializeTo` serializes this value and appends it to `value`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::Status SerializeTo(absl::Cord& value) const {
    AssertIsValid();
    return absl::visit(
        [&value](auto alternative) -> absl::Status {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid ValueView");
          } else {
            return alternative.SerializeTo(value);
          }
        },
        variant_);
  }

  // `Serialize` serializes this value and returns it as `absl::Cord`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::StatusOr<absl::Cord> Serialize() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::StatusOr<absl::Cord> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid ValueView");
          } else {
            return alternative.Serialize();
          }
        },
        variant_);
  }

  // 'GetTypeUrl' returns the type URL that can be used as the type URL for
  // `Any`. If this value does not support serialization, `FAILED_PRECONDITION`
  // is returned.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    AssertIsValid();
    return absl::visit(
        [prefix](auto alternative) -> absl::StatusOr<std::string> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid ValueView");
          } else {
            return alternative.GetTypeUrl(prefix);
          }
        },
        variant_);
  }

  // 'ConvertToAny' converts this value to `Any`. If this value does not support
  // serialization, `FAILED_PRECONDITION` is returned.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    AssertIsValid();
    return absl::visit(
        [prefix](auto alternative) -> absl::StatusOr<Any> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug builds we
            // cannot reach here.
            return absl::InternalError("use of invalid ValueView");
          } else {
            return alternative.ConvertToAny(prefix);
          }
        },
        variant_);
  }

  absl::StatusOr<Json> ConvertToJson() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::StatusOr<Json> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug
            // builds we cannot reach here.
            return absl::InternalError("use of invalid ValueView");
          } else {
            return alternative.ConvertToJson();
          }
        },
        variant_);
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const {
    AssertIsValid();
    return absl::visit(
        [&value_manager, other,
         &scratch](auto alternative) -> absl::StatusOr<ValueView> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an error. In debug
            // builds we cannot reach here.
            return absl::InternalError("use of invalid ValueView");
          } else {
            return alternative.Equal(value_manager, other, scratch);
          }
        },
        variant_);
  }

  bool IsZeroValue() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> bool {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return false. In debug
            // builds we cannot reach here.
            return false;
          } else {
            return alternative.IsZeroValue();
          }
        },
        variant_);
  }

  void swap(ValueView& other) noexcept {
    AssertIsValid();
    other.AssertIsValid();
    variant_.swap(other.variant_);
  }

  friend std::ostream& operator<<(std::ostream& out, ValueView value) {
    value.AssertIsValid();
    return absl::visit(
        [&out](auto alternative) -> std::ostream& {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we do nothing. In debug builds we cannot
            // reach here.
            return out;
          } else {
            return out << alternative;
          }
        },
        value.variant_);
  }

 private:
  friend class Value;
  friend struct NativeTypeTraits<ValueView>;
  friend struct CompositionTraits<ValueView>;

  common_internal::ValueVariant ToVariant() const {
    return absl::visit(
        [](auto alternative) -> common_internal::ValueVariant {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            return common_internal::ValueVariant{};
          } else {
            return common_internal::ValueVariant(
                absl::in_place_type<typename absl::remove_cvref_t<
                    decltype(alternative)>::alternative_type>,
                alternative);
          }
        },
        variant_);
  }

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid ValueView";
  }

  common_internal::ValueViewVariant variant_;
};

inline void swap(ValueView& lhs, ValueView& rhs) noexcept { lhs.swap(rhs); }

template <>
struct NativeTypeTraits<ValueView> final {
  static NativeTypeId Id(ValueView value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return
            // `NativeTypeId::For<absl::monostate>()`. In debug builds we cannot
            // reach here.
            return NativeTypeId::For<absl::monostate>();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        value.variant_);
  }
};

template <>
struct CompositionTraits<ValueView> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsValueViewAlternativeV<U>, bool>
  HasA(ValueView value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return InstanceOf<U>(Get<Base>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValueView, U>, bool> HasA(
      ValueView value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyListValueView>(
               value.variant_) ||
           absl::holds_alternative<ParsedListValueView>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValueView, U>, bool> HasA(
      ValueView value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyMapValueView>(
               value.variant_) ||
           absl::holds_alternative<ParsedMapValueView>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValueView, U>, bool> HasA(
      ValueView value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyStructValueView>(
               value.variant_) ||
           absl::holds_alternative<ParsedStructValueView>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueViewAlternativeV<U>, U> Get(
      ValueView value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValueView, U>, U> Get(
      ValueView value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValueView>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValueView>(value.variant_)};
    }
    return U{absl::get<ParsedListValueView>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValueView, U>, U> Get(
      ValueView value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValueView>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValueView>(value.variant_)};
    }
    return U{absl::get<ParsedMapValueView>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValueView, U>, U> Get(
      ValueView value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValueView>(
            value.variant_)) {
      return U{
          absl::get<common_internal::LegacyStructValueView>(value.variant_)};
    }
    return U{absl::get<ParsedStructValueView>(value.variant_)};
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<ValueView, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<ValueView>);
static_assert(std::is_nothrow_copy_constructible_v<ValueView>);
static_assert(std::is_nothrow_copy_assignable_v<ValueView>);
static_assert(std::is_nothrow_move_constructible_v<ValueView>);
static_assert(std::is_nothrow_move_assignable_v<ValueView>);
static_assert(std::is_nothrow_swappable_v<ValueView>);
static_assert(std::is_trivially_copyable_v<ValueView>);
static_assert(std::is_trivially_destructible_v<ValueView>);

inline Value::Value(ValueView other)
    : variant_((other.AssertIsValid(), other.ToVariant())) {}

inline Value& Value::operator=(ValueView other) {
  other.AssertIsValid();
  variant_ = other.ToVariant();
  return *this;
}

inline absl::StatusOr<ValueView> Value::Equal(ValueManager& value_manager,
                                              ValueView other,
                                              Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.Equal(value_manager, other, scratch);
        }
      },
      variant_);
}

using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

class ValueIterator {
 public:
  virtual ~ValueIterator() = default;

  virtual bool HasNext() = 0;

  // Returns a view of the next value. If the underlying implementation cannot
  // directly return a view of a value, the value will be stored in `scratch`,
  // and the returned view will be that of `scratch`.
  virtual absl::StatusOr<ValueView> Next(
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) = 0;
};

class ValueBuilder {
 public:
  virtual ~ValueBuilder() = default;

  virtual absl::Status SetFieldByName(absl::string_view name, Value value) = 0;

  virtual absl::Status SetFieldByNumber(int64_t number, Value value) = 0;

  virtual Value Build() && = 0;
};

// Now that Value and ValueView are complete, we can define various parts of
// list, map, opaque, and struct which depend on Value and ValueView.

inline ErrorValue::ErrorValue()
    : ErrorValue(common_internal::GetDefaultErrorValue()) {}

inline ErrorValueView::ErrorValueView()
    : ErrorValueView(common_internal::GetDefaultErrorValue()) {}

inline absl::StatusOr<ValueView> ParsedListValue::Get(
    ValueManager& value_manager, size_t index, Value& scratch) const {
  return interface_->Get(value_manager, index, scratch);
}

inline absl::Status ParsedListValue::ForEach(ValueManager& value_manager,
                                             ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::Status ParsedListValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedListValue::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::StatusOr<ValueView> ParsedListValue::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return interface_->Equal(value_manager, other, scratch);
}

inline absl::StatusOr<ValueView> ParsedListValueView::Get(
    ValueManager& value_manager, size_t index, Value& scratch) const {
  return interface_->Get(value_manager, index, scratch);
}

inline absl::Status ParsedListValueView::ForEach(
    ValueManager& value_manager, ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::Status ParsedListValueView::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedListValueView::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::StatusOr<ValueView> ParsedListValueView::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return interface_->Equal(value_manager, other, scratch);
}

inline absl::StatusOr<ValueView> ListValue::Get(ValueManager& value_manager,
                                                size_t index,
                                                Value& scratch) const {
  return absl::visit(
      [&value_manager, index,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Get(value_manager, index, scratch);
      },
      variant_);
}

inline absl::Status ListValue::ForEach(ValueManager& value_manager,
                                       ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

inline absl::Status ListValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> ListValue::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative)
          -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

inline absl::StatusOr<ValueView> ListValue::Equal(ValueManager& value_manager,
                                                  ValueView other,
                                                  Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Equal(value_manager, other, scratch);
      },
      variant_);
}

inline absl::StatusOr<ValueView> ListValueView::Get(ValueManager& value_manager,
                                                    size_t index,
                                                    Value& scratch) const {
  return absl::visit(
      [&value_manager, index,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Get(value_manager, index, scratch);
      },
      variant_);
}

inline absl::Status ListValueView::ForEach(ValueManager& value_manager,
                                           ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](auto alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

inline absl::Status ListValueView::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return absl::visit(
      [&value_manager, callback](auto alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ListValueView::NewIterator(ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](
          auto alternative) -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

inline absl::StatusOr<ValueView> ListValueView::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Equal(value_manager, other, scratch);
      },
      variant_);
}

inline absl::StatusOr<ValueView> OpaqueValue::Equal(ValueManager& value_manager,
                                                    ValueView other,
                                                    Value& scratch) const {
  return interface_->Equal(value_manager, other, scratch);
}

inline absl::StatusOr<ValueView> OpaqueValueView::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return interface_->Equal(value_manager, other, scratch);
}

inline OptionalValue OptionalValue::None() {
  return OptionalValue(common_internal::GetEmptyDynOptionalValue());
}

inline ValueView OptionalValue::Value(cel::Value& scratch) const {
  return (*this)->Value(scratch);
}

inline OptionalValueView OptionalValueView::None() {
  return common_internal::GetEmptyDynOptionalValue();
}

inline ValueView OptionalValueView::Value(cel::Value& scratch) const {
  return (*this)->Value(scratch);
}

inline absl::StatusOr<ValueView> ParsedMapValue::Get(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return interface_->Get(value_manager, key, scratch);
}

inline absl::StatusOr<std::pair<ValueView, bool>> ParsedMapValue::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return interface_->Find(value_manager, key, scratch);
}

inline absl::StatusOr<ValueView> ParsedMapValue::Has(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return interface_->Has(value_manager, key, scratch);
}

inline absl::StatusOr<ListValueView> ParsedMapValue::ListKeys(
    ValueManager& value_manager, ListValue& scratch) const {
  return interface_->ListKeys(value_manager, scratch);
}

inline absl::Status ParsedMapValue::ForEach(ValueManager& value_manager,
                                            ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedMapValue::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::StatusOr<ValueView> ParsedMapValue::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return interface_->Equal(value_manager, other, scratch);
}

inline absl::StatusOr<ValueView> ParsedMapValueView::Get(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return interface_->Get(value_manager, key, scratch);
}

inline absl::StatusOr<std::pair<ValueView, bool>> ParsedMapValueView::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return interface_->Find(value_manager, key, scratch);
}

inline absl::StatusOr<ValueView> ParsedMapValueView::Has(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return interface_->Has(value_manager, key, scratch);
}

inline absl::StatusOr<ListValueView> ParsedMapValueView::ListKeys(
    ValueManager& value_manager, ListValue& scratch) const {
  return interface_->ListKeys(value_manager, scratch);
}

inline absl::Status ParsedMapValueView::ForEach(
    ValueManager& value_manager, ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedMapValueView::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::StatusOr<ValueView> ParsedMapValueView::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return interface_->Equal(value_manager, other, scratch);
}

inline absl::StatusOr<ValueView> MapValue::Get(ValueManager& value_manager,
                                               ValueView key,
                                               Value& scratch) const {
  return absl::visit(
      [&value_manager, key,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Get(value_manager, key, scratch);
      },
      variant_);
}

inline absl::StatusOr<std::pair<ValueView, bool>> MapValue::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return absl::visit(
      [&value_manager, key, &scratch](const auto& alternative)
          -> absl::StatusOr<std::pair<ValueView, bool>> {
        return alternative.Find(value_manager, key, scratch);
      },
      variant_);
}

inline absl::StatusOr<ValueView> MapValue::Has(ValueManager& value_manager,
                                               ValueView key,
                                               Value& scratch) const {
  return absl::visit(
      [&value_manager, key,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Has(value_manager, key, scratch);
      },
      variant_);
}

inline absl::StatusOr<ListValueView> MapValue::ListKeys(
    ValueManager& value_manager, ListValue& scratch) const {
  return absl::visit(
      [&value_manager,
       &scratch](const auto& alternative) -> absl::StatusOr<ListValueView> {
        return alternative.ListKeys(value_manager, scratch);
      },
      variant_);
}

inline absl::Status MapValue::ForEach(ValueManager& value_manager,
                                      ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> MapValue::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative)
          -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

inline absl::StatusOr<ValueView> MapValue::Equal(ValueManager& value_manager,
                                                 ValueView other,
                                                 Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Equal(value_manager, other, scratch);
      },
      variant_);
}

inline absl::StatusOr<ValueView> MapValueView::Get(ValueManager& value_manager,
                                                   ValueView key,
                                                   Value& scratch) const {
  return absl::visit(
      [&value_manager, key,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Get(value_manager, key, scratch);
      },
      variant_);
}

inline absl::StatusOr<std::pair<ValueView, bool>> MapValueView::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return absl::visit(
      [&value_manager, key, &scratch](
          auto alternative) -> absl::StatusOr<std::pair<ValueView, bool>> {
        return alternative.Find(value_manager, key, scratch);
      },
      variant_);
}

inline absl::StatusOr<ValueView> MapValueView::Has(ValueManager& value_manager,
                                                   ValueView key,
                                                   Value& scratch) const {
  return absl::visit(
      [&value_manager, key,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Has(value_manager, key, scratch);
      },
      variant_);
}

inline absl::StatusOr<ListValueView> MapValueView::ListKeys(
    ValueManager& value_manager, ListValue& scratch) const {
  return absl::visit(
      [&value_manager,
       &scratch](auto alternative) -> absl::StatusOr<ListValueView> {
        return alternative.ListKeys(value_manager, scratch);
      },
      variant_);
}

inline absl::Status MapValueView::ForEach(ValueManager& value_manager,
                                          ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](auto alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
MapValueView::NewIterator(ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](
          auto alternative) -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

inline absl::StatusOr<ValueView> MapValueView::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Equal(value_manager, other, scratch);
      },
      variant_);
}

inline absl::StatusOr<ValueView> ParsedStructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch) const {
  return interface_->GetFieldByName(value_manager, name, scratch);
}

inline absl::StatusOr<ValueView> ParsedStructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch) const {
  return interface_->GetFieldByNumber(value_manager, number, scratch);
}

inline absl::StatusOr<ValueView> ParsedStructValue::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return interface_->Equal(value_manager, other, scratch);
}

inline absl::Status ParsedStructValue::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  return interface_->ForEachField(value_manager, callback);
}

inline absl::StatusOr<std::pair<ValueView, int>> ParsedStructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& scratch) const {
  return interface_->Qualify(value_manager, qualifiers, presence_test, scratch);
}

inline absl::StatusOr<ValueView> ParsedStructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch) const {
  return interface_->GetFieldByName(value_manager, name, scratch);
}

inline absl::StatusOr<ValueView> ParsedStructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch) const {
  return interface_->GetFieldByNumber(value_manager, number, scratch);
}

inline absl::StatusOr<ValueView> ParsedStructValueView::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  return interface_->Equal(value_manager, other, scratch);
}

inline absl::Status ParsedStructValueView::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  return interface_->ForEachField(value_manager, callback);
}

inline absl::StatusOr<std::pair<ValueView, int>> ParsedStructValueView::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& scratch) const {
  return interface_->Qualify(value_manager, qualifiers, presence_test, scratch);
}

inline absl::StatusOr<ValueView> StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, name,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByName(value_manager, name, scratch);
        }
      },
      variant_);
}

inline absl::StatusOr<ValueView> StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, number,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByNumber(value_manager, number, scratch);
        }
      },
      variant_);
}

inline absl::StatusOr<ValueView> StructValue::Equal(ValueManager& value_manager,
                                                    ValueView other,
                                                    Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Equal(value_manager, other, scratch);
        }
      },
      variant_);
}

inline absl::Status StructValue::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ForEachField(value_manager, callback);
        }
      },
      variant_);
}

inline absl::StatusOr<std::pair<ValueView, int>> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, qualifiers, presence_test,
       &scratch](const auto& alternative)
          -> absl::StatusOr<std::pair<ValueView, int>> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Qualify(value_manager, qualifiers, presence_test,
                                     scratch);
        }
      },
      variant_);
}

inline absl::StatusOr<ValueView> StructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, name,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.GetFieldByName(value_manager, name, scratch);
        }
      },
      variant_);
}

inline absl::StatusOr<ValueView> StructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, number,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.GetFieldByNumber(value_manager, number, scratch);
        }
      },
      variant_);
}

inline absl::StatusOr<ValueView> StructValueView::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.Equal(value_manager, other, scratch);
        }
      },
      variant_);
}

inline absl::Status StructValueView::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, callback](auto alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.ForEachField(value_manager, callback);
        }
      },
      variant_);
}

inline absl::StatusOr<std::pair<ValueView, int>> StructValueView::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, qualifiers, presence_test, &scratch](
          auto alternative) -> absl::StatusOr<std::pair<ValueView, int>> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.Qualify(value_manager, qualifiers, presence_test,
                                     scratch);
        }
      },
      variant_);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
