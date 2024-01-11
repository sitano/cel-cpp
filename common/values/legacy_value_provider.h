// Copyright 2024 Google LLC
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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_VALUE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_VALUE_PROVIDER_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/types/legacy_type_provider.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_provider.h"

namespace cel::common_internal {

class LegacyValueProvider : public LegacyTypeProvider, public ValueProvider {
 public:
  LegacyValueProvider() : LegacyTypeProvider() {}

  absl::StatusOr<absl::optional<Unique<StructValueBuilder>>>
  NewStructValueBuilder(ValueFactory& value_factory,
                        StructTypeView type) const override;

  absl::StatusOr<absl::optional<ValueView>> FindValue(
      ValueFactory& value_factory, absl::string_view name,
      Value& scratch) const override;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_VALUE_PROVIDER_H_
