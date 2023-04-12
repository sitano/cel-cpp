// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_

#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "base/handle.h"
#include "base/memory_manager.h"
#include "base/types/any_type.h"
#include "base/types/bool_type.h"
#include "base/types/bytes_type.h"
#include "base/types/double_type.h"
#include "base/types/duration_type.h"
#include "base/types/dyn_type.h"
#include "base/types/enum_type.h"
#include "base/types/error_type.h"
#include "base/types/int_type.h"
#include "base/types/list_type.h"
#include "base/types/map_type.h"
#include "base/types/null_type.h"
#include "base/types/optional_type.h"
#include "base/types/string_type.h"
#include "base/types/struct_type.h"
#include "base/types/timestamp_type.h"
#include "base/types/type_type.h"
#include "base/types/uint_type.h"
#include "base/types/unknown_type.h"
#include "base/types/wrapper_type.h"

namespace cel {

// TypeFactory provides member functions to get and create type implementations
// of builtin types.
//
// While TypeFactory is not final and has a virtual destructor, inheriting it is
// forbidden outside of the CEL codebase.
class TypeFactory final {
 private:
  template <typename T, typename U, typename V>
  using EnableIfBaseOfT = std::enable_if_t<std::is_base_of_v<T, U>, V>;

 public:
  explicit TypeFactory(
      MemoryManager& memory_manager ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : memory_manager_(memory_manager) {}

  TypeFactory(const TypeFactory&) = delete;
  TypeFactory& operator=(const TypeFactory&) = delete;

  Handle<NullType> GetNullType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<ErrorType> GetErrorType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<DynType> GetDynType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<AnyType> GetAnyType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<BoolType> GetBoolType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<IntType> GetIntType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<UintType> GetUintType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<DoubleType> GetDoubleType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<StringType> GetStringType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<BytesType> GetBytesType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<DurationType> GetDurationType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<TimestampType> GetTimestampType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<BoolWrapperType> GetBoolWrapperType() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return BoolWrapperType::Get();
  }

  Handle<BytesWrapperType> GetBytesWrapperType() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return BytesWrapperType::Get();
  }

  Handle<DoubleWrapperType> GetDoubleWrapperType()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return DoubleWrapperType::Get();
  }

  Handle<IntWrapperType> GetIntWrapperType() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return IntWrapperType::Get();
  }

  Handle<StringWrapperType> GetStringWrapperType()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return StringWrapperType::Get();
  }

  Handle<UintWrapperType> GetUintWrapperType() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return UintWrapperType::Get();
  }

  template <typename T, typename... Args>
  EnableIfBaseOfT<EnumType, T, absl::StatusOr<Handle<T>>> CreateEnumType(
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<T>(
        memory_manager(), std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  EnableIfBaseOfT<StructType, T, absl::StatusOr<Handle<T>>> CreateStructType(
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<T>(
        memory_manager(), std::forward<Args>(args)...);
  }

  absl::StatusOr<Handle<ListType>> CreateListType(const Handle<Type>& element)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Handle<MapType>> CreateMapType(const Handle<Type>& key,
                                                const Handle<Type>& value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Handle<OptionalType>> CreateOptionalType(Handle<Type> type)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<TypeType> GetTypeType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<UnknownType> GetUnknownType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  MemoryManager& memory_manager() const { return memory_manager_; }

 private:
  MemoryManager& memory_manager_;

  absl::Mutex list_types_mutex_;
  // Mapping from list element types to the list type. This allows us to cache
  // list types and avoid re-creating the same type.
  absl::flat_hash_map<Handle<Type>, Handle<ListType>> list_types_
      ABSL_GUARDED_BY(list_types_mutex_);

  absl::Mutex map_types_mutex_;
  // Mapping from map key and value types to the map type. This allows us to
  // cache map types and avoid re-creating the same type.
  absl::flat_hash_map<std::pair<Handle<Type>, Handle<Type>>, Handle<MapType>>
      map_types_ ABSL_GUARDED_BY(map_types_mutex_);

  absl::Mutex optional_types_mutex_;
  absl::flat_hash_map<Handle<Type>, Handle<OptionalType>> optional_types_
      ABSL_GUARDED_BY(optional_types_mutex_);
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_
