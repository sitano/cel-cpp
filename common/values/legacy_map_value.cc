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

#include "common/values/legacy_map_value.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_manager.h"
#include "common/values/map_value_interface.h"
#include "common/values/values.h"
#include "internal/dynamic_loader.h"
#include "internal/status_macros.h"

namespace cel::common_internal {

namespace {

using LegacyMapValue_DebugString = std::string (*)(uintptr_t);
using LegacyMapValue_GetSerializedSize = absl::StatusOr<size_t> (*)(uintptr_t);
using LegacyMapValue_SerializeTo = absl::Status (*)(uintptr_t, absl::Cord&);
using LegacyMapValue_ConvertToJsonObject =
    absl::StatusOr<JsonObject> (*)(uintptr_t);
using LegacyMapValue_IsEmpty = bool (*)(uintptr_t);
using LegacyMapValue_Size = size_t (*)(uintptr_t);
using LegacyMapValue_Get = absl::StatusOr<ValueView> (*)(uintptr_t,
                                                         ValueManager&,
                                                         ValueView, Value&);
using LegacyMapValue_Find = absl::StatusOr<std::pair<ValueView, bool>> (*)(
    uintptr_t, ValueManager&, ValueView, Value&);
using LegacyMapValue_Has = absl::StatusOr<ValueView> (*)(uintptr_t,
                                                         ValueManager&,
                                                         ValueView, Value&);
using LegacyMapValue_ListKeys = absl::StatusOr<ListValueView> (*)(uintptr_t,
                                                                  ValueManager&,
                                                                  ListValue&);
using LegacyMapValue_ForEach =
    absl::Status (*)(uintptr_t, ValueManager&, LegacyMapValue::ForEachCallback);
using LegacyMapValue_NewIterator =
    absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> (*)(uintptr_t,
                                                        ValueManager&);

ABSL_CONST_INIT struct {
  absl::once_flag init_once;
  LegacyMapValue_DebugString debug_string = nullptr;
  LegacyMapValue_GetSerializedSize get_serialized_size = nullptr;
  LegacyMapValue_SerializeTo serialize_to = nullptr;
  LegacyMapValue_ConvertToJsonObject convert_to_json_object = nullptr;
  LegacyMapValue_IsEmpty is_empty = nullptr;
  LegacyMapValue_Size size = nullptr;
  LegacyMapValue_Get get = nullptr;
  LegacyMapValue_Find find = nullptr;
  LegacyMapValue_Has has = nullptr;
  LegacyMapValue_ListKeys list_keys = nullptr;
  LegacyMapValue_ForEach for_each = nullptr;
  LegacyMapValue_NewIterator new_iterator = nullptr;
} legacy_map_value_vtable;

void InitializeLegacyMapValue() {
  absl::call_once(legacy_map_value_vtable.init_once, []() -> void {
    internal::DynamicLoader dynamic_loader;
    legacy_map_value_vtable.debug_string = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_DebugString");
    legacy_map_value_vtable.get_serialized_size =
        dynamic_loader.FindSymbolOrDie(
            "cel_common_internal_LegacyMapValue_GetSerializedSize");
    legacy_map_value_vtable.serialize_to = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_SerializeTo");
    legacy_map_value_vtable.convert_to_json_object =
        dynamic_loader.FindSymbolOrDie(
            "cel_common_internal_LegacyMapValue_ConvertToJsonObject");
    legacy_map_value_vtable.is_empty = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_IsEmpty");
    legacy_map_value_vtable.size = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_Size");
    legacy_map_value_vtable.get = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_Get");
    legacy_map_value_vtable.find = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_Find");
    legacy_map_value_vtable.has = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_Has");
    legacy_map_value_vtable.list_keys = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_ListKeys");
    legacy_map_value_vtable.for_each = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_ForEach");
    legacy_map_value_vtable.new_iterator = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_NewIterator");
  });
}

}  // namespace

MapType LegacyMapValue::GetType(TypeManager& type_manager) const {
  return MapType(type_manager.GetDynDynMapType());
}

std::string LegacyMapValue::DebugString() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.debug_string)(impl_);
}

absl::StatusOr<size_t> LegacyMapValue::GetSerializedSize() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.get_serialized_size)(impl_);
}

absl::Status LegacyMapValue::SerializeTo(absl::Cord& value) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.serialize_to)(impl_, value);
}

absl::StatusOr<absl::Cord> LegacyMapValue::Serialize() const {
  absl::Cord serialized_value;
  CEL_RETURN_IF_ERROR(SerializeTo(serialized_value));
  return serialized_value;
}

absl::StatusOr<std::string> LegacyMapValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.Struct");
}

absl::StatusOr<Any> LegacyMapValue::ConvertToAny(
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<JsonObject> LegacyMapValue::ConvertToJsonObject() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.convert_to_json_object)(impl_);
}

bool LegacyMapValue::IsEmpty() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.is_empty)(impl_);
}

size_t LegacyMapValue::Size() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.size)(impl_);
}

absl::StatusOr<ValueView> LegacyMapValue::Get(ValueManager& value_manager,
                                              ValueView key,
                                              Value& scratch) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.get)(impl_, value_manager, key, scratch);
}

absl::StatusOr<std::pair<ValueView, bool>> LegacyMapValue::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.find)(impl_, value_manager, key, scratch);
}

absl::StatusOr<ValueView> LegacyMapValue::Has(ValueManager& value_manager,
                                              ValueView key,
                                              Value& scratch) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.has)(impl_, value_manager, key, scratch);
}

absl::StatusOr<ListValueView> LegacyMapValue::ListKeys(
    ValueManager& value_manager, ListValue& scratch) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.list_keys)(impl_, value_manager, scratch);
}

absl::Status LegacyMapValue::ForEach(ValueManager& value_manager,
                                     ForEachCallback callback) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.for_each)(impl_, value_manager, callback);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> LegacyMapValue::NewIterator(
    ValueManager& value_manager) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.new_iterator)(impl_, value_manager);
}

absl::StatusOr<ValueView> LegacyMapValue::Equal(ValueManager& value_manager,
                                                ValueView other,
                                                Value& scratch) const {
  if (auto map_value = As<MapValueView>(other); map_value.has_value()) {
    return MapValueEqual(value_manager, *this, *map_value, scratch);
  }
  return BoolValueView{false};
}

MapType LegacyMapValueView::GetType(TypeManager& type_manager) const {
  return MapType(type_manager.GetDynDynMapType());
}

std::string LegacyMapValueView::DebugString() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.debug_string)(impl_);
}

absl::StatusOr<size_t> LegacyMapValueView::GetSerializedSize() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.get_serialized_size)(impl_);
}

absl::Status LegacyMapValueView::SerializeTo(absl::Cord& value) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.serialize_to)(impl_, value);
}

// See `ValueInterface::Serialize`.
absl::StatusOr<absl::Cord> LegacyMapValueView::Serialize() const {
  absl::Cord serialized_value;
  CEL_RETURN_IF_ERROR(SerializeTo(serialized_value));
  return serialized_value;
}

absl::StatusOr<std::string> LegacyMapValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.Struct");
}

absl::StatusOr<Any> LegacyMapValueView::ConvertToAny(
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<JsonObject> LegacyMapValueView::ConvertToJsonObject() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.convert_to_json_object)(impl_);
}

bool LegacyMapValueView::IsEmpty() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.is_empty)(impl_);
}

size_t LegacyMapValueView::Size() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.size)(impl_);
}

absl::StatusOr<ValueView> LegacyMapValueView::Get(ValueManager& value_manager,
                                                  ValueView key,
                                                  Value& scratch) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.get)(impl_, value_manager, key, scratch);
}

absl::StatusOr<std::pair<ValueView, bool>> LegacyMapValueView::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.find)(impl_, value_manager, key, scratch);
}

absl::StatusOr<ValueView> LegacyMapValueView::Has(ValueManager& value_manager,
                                                  ValueView key,
                                                  Value& scratch) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.has)(impl_, value_manager, key, scratch);
}

absl::StatusOr<ListValueView> LegacyMapValueView::ListKeys(
    ValueManager& value_manager, ListValue& scratch) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.list_keys)(impl_, value_manager, scratch);
}

absl::Status LegacyMapValueView::ForEach(ValueManager& value_manager,
                                         ForEachCallback callback) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.for_each)(impl_, value_manager, callback);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> LegacyMapValueView::NewIterator(
    ValueManager& value_manager) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.new_iterator)(impl_, value_manager);
}

absl::StatusOr<ValueView> LegacyMapValueView::Equal(ValueManager& value_manager,
                                                    ValueView other,
                                                    Value& scratch) const {
  if (auto map_value = As<MapValueView>(other); map_value.has_value()) {
    return MapValueEqual(value_manager, *this, *map_value, scratch);
  }
  return BoolValueView{false};
}

}  // namespace cel::common_internal
