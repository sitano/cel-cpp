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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_PRE_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_PRE_H_

#include <type_traits>

namespace cel::base_internal {

template <typename T>
struct MemoryManagerDestructor final {
  static void Destruct(void* pointer) { static_cast<T*>(pointer)->~T(); }
};

template <typename, typename = void>
struct HasIsDestructorSkippable : std::false_type {};

template <typename T>
struct HasIsDestructorSkippable<
    T, std::void_t<decltype(std::declval<const T&>().IsDestructorSkippable())>>
    : std::true_type {};

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_PRE_H_
