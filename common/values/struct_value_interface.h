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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_INTERFACE_H_

#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/attribute.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/type.h"
#include "common/type_manager.h"
#include "common/value_interface.h"
#include "common/value_kind.h"

namespace cel {

class StructValue;
class StructValueView;
class ValueView;

class StructValueInterface : public ValueInterface {
 public:
  using alternative_type = StructValue;
  using view_alternative_type = StructValueView;

  static constexpr ValueKind kKind = ValueKind::kStruct;

  ValueKind kind() const final { return kKind; }

  StructType GetType(TypeManager& type_manager) const {
    return Cast<StructType>(GetTypeImpl(type_manager));
  }

  absl::StatusOr<Json> ConvertToJson() const final {
    return ConvertToJsonObject();
  }

  virtual absl::StatusOr<JsonObject> ConvertToJsonObject() const = 0;

  using ForEachFieldCallback =
      absl::FunctionRef<absl::StatusOr<bool>(absl::string_view, ValueView)>;

 protected:
  Type GetTypeImpl(TypeManager& type_manager) const override {
    return type_manager.CreateStructType(GetTypeName());
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_INTERFACE_H_
