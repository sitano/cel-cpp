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

#include "extensions/protobuf/internal/any.h"

#include <memory>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_provider.h"
#include "base/value_factory.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"

namespace cel::extensions::protobuf_internal {
namespace {

using cel::internal::StatusIs;

TEST(Any, GeneratedRoundtrip) {
  google::protobuf::Any proto;
  ASSERT_OK(WrapGeneratedAnyProto("type.googleapis.com/foo.Bar",
                                  absl::Cord("baz"), proto));
  EXPECT_EQ(proto.type_url(), "type.googleapis.com/foo.Bar");
  EXPECT_EQ(proto.value(), "baz");
  ASSERT_OK_AND_ASSIGN(auto any, UnwrapGeneratedAnyProto(proto));
  EXPECT_EQ(any.type_url(), proto.type_url());
  EXPECT_EQ(any.value(), proto.value());
}

TEST(Any, CustomRoundtrip) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::Any::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  std::unique_ptr<google::protobuf::Message> proto = absl::WrapUnique(
      factory.GetPrototype(pool.FindMessageTypeByName("google.protobuf.Any"))
          ->New());
  const auto* descriptor = proto->GetDescriptor();
  const auto* type_url_field = descriptor->FindFieldByName("type_url");
  ASSERT_NE(type_url_field, nullptr);
  const auto* value_field = descriptor->FindFieldByName("value");
  ASSERT_NE(value_field, nullptr);

  ASSERT_OK(WrapDynamicAnyProto("type.googleapis.com/foo.Bar",
                                absl::Cord("baz"), *proto));

  ASSERT_OK_AND_ASSIGN(auto any, UnwrapDynamicAnyProto(*proto));
  EXPECT_EQ(any.type_url(), "type.googleapis.com/foo.Bar");
  EXPECT_EQ(any.value(), "baz");
}

TEST(Any, ToJson) {
  MemoryManager& memory_manager = MemoryManager::Global();
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.Value",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.ListValue",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.Struct",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.BoolValue",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.BytesValue",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(AnyToJson(value_factory,
                        "type.googleapis.com/google.protobuf.DoubleValue",
                        absl::Cord()),
              StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.FloatValue",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.Int32Value",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.Int64Value",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(AnyToJson(value_factory,
                        "type.googleapis.com/google.protobuf.UInt32Value",
                        absl::Cord()),
              StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(AnyToJson(value_factory,
                        "type.googleapis.com/google.protobuf.UInt64Value",
                        absl::Cord()),
              StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(AnyToJson(value_factory,
                        "type.googleapis.com/google.protobuf.StringValue",
                        absl::Cord()),
              StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.Duration",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(
      AnyToJson(value_factory, "type.googleapis.com/google.protobuf.Timestamp",
                absl::Cord()),
      StatusIs(absl::StatusCode::kUnimplemented));

  EXPECT_THAT(AnyToJson(value_factory,
                        "type.googleapis.com/message.that.does.not.Exist",
                        absl::Cord()),
              StatusIs(absl::StatusCode::kNotFound));
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
