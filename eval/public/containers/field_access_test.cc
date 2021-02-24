#include "eval/public/containers/field_access.h"

#include "google/protobuf/message.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "internal/proto_util.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::api::expr::internal::MakeGoogleApiDurationMax;
using google::api::expr::internal::MakeGoogleApiTimeMax;
using google::protobuf::FieldDescriptor;
using test::v1::proto3::TestAllTypes;

TEST(FieldAccessTest, SetDuration) {
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(
      CelValue::CreateDuration(MakeGoogleApiDurationMax()), field, &msg);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetDurationBadDuration) {
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(
      CelValue::CreateDuration(MakeGoogleApiDurationMax() + absl::Seconds(1)),
      field, &msg);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetDurationBadInputType) {
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(CelValue::CreateInt64(1), field, &msg);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetTimestamp) {
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(
      CelValue::CreateTimestamp(MakeGoogleApiTimeMax()), field, &msg);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetTimestampBadTime) {
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(
      CelValue::CreateTimestamp(MakeGoogleApiTimeMax() + absl::Seconds(1)),
      field, &msg);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetTimestampBadInputType) {
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(CelValue::CreateInt64(1), field, &msg);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
