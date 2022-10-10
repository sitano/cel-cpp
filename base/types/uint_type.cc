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

#include "base/types/uint_type.h"

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(UintType);

namespace {

ABSL_CONST_INIT absl::once_flag instance_once;
alignas(Handle<UintType>) char instance_storage[sizeof(Handle<UintType>)];

}  // namespace

const Handle<UintType>& UintType::Get() {
  absl::call_once(instance_once, []() {
    base_internal::HandleFactory<UintType>::MakeAt<UintType>(
        &instance_storage[0]);
  });
  return *reinterpret_cast<const Handle<UintType>*>(&instance_storage[0]);
}

}  // namespace cel
