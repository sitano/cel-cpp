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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"

namespace cel {

class TypeFactory;

// `TypeIntrospector` is an interface which allows querying type-related
// information. It handles type introspection, but not type reflection. That is,
// it is not capable of instantiating new values or understanding values. Its
// primary usage is for type checking, and a subset of that shared functionality
// is used by the runtime.
class TypeIntrospector {
 public:
  virtual ~TypeIntrospector() = default;

  // `FindType` find the type corresponding to name `name`.
  absl::StatusOr<absl::optional<TypeView>> FindType(
      TypeFactory& type_factory, absl::string_view name,
      Type& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  // `FindStructTypeFieldByName` find the name, number, and type of the field
  // `name` in type `type`.
  absl::StatusOr<absl::optional<StructTypeFieldView>> FindStructTypeFieldByName(
      TypeFactory& type_factory, absl::string_view type, absl::string_view name,
      StructTypeField& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  // `FindStructTypeFieldByName` find the name, number, and type of the field
  // `name` in struct type `type`.
  absl::StatusOr<absl::optional<StructTypeFieldView>> FindStructTypeFieldByName(
      TypeFactory& type_factory, StructTypeView type, absl::string_view name,
      StructTypeField& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const {
    return FindStructTypeFieldByName(type_factory, type.name(), name, scratch);
  }

 protected:
  virtual absl::StatusOr<absl::optional<TypeView>> FindTypeImpl(
      TypeFactory& type_factory, absl::string_view name,
      Type& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const = 0;

  virtual absl::StatusOr<absl::optional<StructTypeFieldView>>
  FindStructTypeFieldByNameImpl(
      TypeFactory& type_factory, absl::string_view type, absl::string_view name,
      StructTypeField& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const = 0;
};

Shared<TypeIntrospector> NewThreadCompatibleTypeIntrospector(
    MemoryManagerRef memory_manager);

Shared<TypeIntrospector> NewThreadSafeTypeIntrospector(
    MemoryManagerRef memory_manager);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_
