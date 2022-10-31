#include "eval/public/cel_value.h"

#include <cstdint>
#include <string>
#include <vector>

#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/memory_manager.h"
#include "eval/internal/errors.h"
#include "eval/public/cel_value_internal.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "extensions/protobuf/memory_manager.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Arena;
namespace interop = ::cel::interop_internal;

constexpr absl::string_view kNullTypeName = "null_type";
constexpr absl::string_view kBoolTypeName = "bool";
constexpr absl::string_view kInt64TypeName = "int";
constexpr absl::string_view kUInt64TypeName = "uint";
constexpr absl::string_view kDoubleTypeName = "double";
constexpr absl::string_view kStringTypeName = "string";
constexpr absl::string_view kBytesTypeName = "bytes";
constexpr absl::string_view kDurationTypeName = "google.protobuf.Duration";
constexpr absl::string_view kTimestampTypeName = "google.protobuf.Timestamp";
// Leading "." to prevent potential namespace clash.
constexpr absl::string_view kListTypeName = "list";
constexpr absl::string_view kMapTypeName = "map";
constexpr absl::string_view kCelTypeTypeName = "type";

// Exclusive bounds for valid duration values.
constexpr absl::Duration kDurationHigh = absl::Seconds(315576000001);
constexpr absl::Duration kDurationLow = absl::Seconds(-315576000001);

const absl::Status* DurationOverflowError() {
  static const auto* const kDurationOverflow = new absl::Status(
      absl::StatusCode::kInvalidArgument, "Duration is out of range");
  return kDurationOverflow;
}

struct DebugStringVisitor {
  google::protobuf::Arena* const arena;

  std::string operator()(bool arg) { return absl::StrFormat("%d", arg); }
  std::string operator()(int64_t arg) { return absl::StrFormat("%lld", arg); }
  std::string operator()(uint64_t arg) { return absl::StrFormat("%llu", arg); }
  std::string operator()(double arg) { return absl::StrFormat("%f", arg); }
  std::string operator()(CelValue::NullType) { return "null"; }

  std::string operator()(CelValue::StringHolder arg) {
    return absl::StrFormat("%s", arg.value());
  }

  std::string operator()(CelValue::BytesHolder arg) {
    return absl::StrFormat("%s", arg.value());
  }

  std::string operator()(const MessageWrapper& arg) {
    return arg.message_ptr() == nullptr
               ? "NULL"
               : arg.legacy_type_info()->DebugString(arg);
  }

  std::string operator()(absl::Duration arg) {
    return absl::FormatDuration(arg);
  }

  std::string operator()(absl::Time arg) {
    return absl::FormatTime(arg, absl::UTCTimeZone());
  }

  std::string operator()(const CelList* arg) {
    std::vector<std::string> elements;
    elements.reserve(arg->size());
    for (int i = 0; i < arg->size(); i++) {
      elements.push_back(arg->Get(arena, i).DebugString());
    }
    return absl::StrCat("[", absl::StrJoin(elements, ", "), "]");
  }

  std::string operator()(const CelMap* arg) {
    const CelList* keys = arg->ListKeys(arena).value();
    std::vector<std::string> elements;
    elements.reserve(keys->size());
    for (int i = 0; i < keys->size(); i++) {
      const auto& key = (*keys).Get(arena, i);
      const auto& optional_value = arg->Get(arena, key);
      elements.push_back(absl::StrCat("<", key.DebugString(), ">: <",
                                      optional_value.has_value()
                                          ? optional_value->DebugString()
                                          : "nullopt",
                                      ">"));
    }
    return absl::StrCat("{", absl::StrJoin(elements, ", "), "}");
  }

  std::string operator()(const UnknownSet* arg) {
    return "?";  // Not implemented.
  }

  std::string operator()(CelValue::CelTypeHolder arg) {
    return absl::StrCat(arg.value());
  }

  std::string operator()(const CelError* arg) { return arg->ToString(); }
};

}  // namespace

CelValue CelValue::CreateDuration(absl::Duration value) {
  if (value >= kDurationHigh || value <= kDurationLow) {
    return CelValue(DurationOverflowError());
  }
  return CelValue(value);
}

// TODO(issues/136): These don't match the CEL runtime typenames. They should
// be updated where possible for consistency.
std::string CelValue::TypeName(Type value_type) {
  switch (value_type) {
    case Type::kNullType:
      return "null_type";
    case Type::kBool:
      return "bool";
    case Type::kInt64:
      return "int64";
    case Type::kUint64:
      return "uint64";
    case Type::kDouble:
      return "double";
    case Type::kString:
      return "string";
    case Type::kBytes:
      return "bytes";
    case Type::kMessage:
      return "Message";
    case Type::kDuration:
      return "Duration";
    case Type::kTimestamp:
      return "Timestamp";
    case Type::kList:
      return "CelList";
    case Type::kMap:
      return "CelMap";
    case Type::kCelType:
      return "CelType";
    case Type::kUnknownSet:
      return "UnknownSet";
    case Type::kError:
      return "CelError";
    case Type::kAny:
      return "Any type";
    default:
      return "unknown";
  }
}

absl::Status CelValue::CheckMapKeyType(const CelValue& key) {
  switch (key.type()) {
    case CelValue::Type::kString:
    case CelValue::Type::kInt64:
    case CelValue::Type::kUint64:
    case CelValue::Type::kBool:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", CelValue::TypeName(key.type()), "'"));
  }
}

CelValue CelValue::ObtainCelType() const {
  switch (type()) {
    case Type::kNullType:
      return CreateCelType(CelTypeHolder(kNullTypeName));
    case Type::kBool:
      return CreateCelType(CelTypeHolder(kBoolTypeName));
    case Type::kInt64:
      return CreateCelType(CelTypeHolder(kInt64TypeName));
    case Type::kUint64:
      return CreateCelType(CelTypeHolder(kUInt64TypeName));
    case Type::kDouble:
      return CreateCelType(CelTypeHolder(kDoubleTypeName));
    case Type::kString:
      return CreateCelType(CelTypeHolder(kStringTypeName));
    case Type::kBytes:
      return CreateCelType(CelTypeHolder(kBytesTypeName));
    case Type::kMessage: {
      MessageWrapper wrapper;
      CelValue::GetValue(&wrapper);
      if (wrapper.message_ptr() == nullptr) {
        return CreateCelType(CelTypeHolder(kNullTypeName));
      }
      // Descritptor::full_name() returns const reference, so using pointer
      // should be safe.
      return CreateCelType(
          CelTypeHolder(wrapper.legacy_type_info()->GetTypename(wrapper)));
    }
    case Type::kDuration:
      return CreateCelType(CelTypeHolder(kDurationTypeName));
    case Type::kTimestamp:
      return CreateCelType(CelTypeHolder(kTimestampTypeName));
    case Type::kList:
      return CreateCelType(CelTypeHolder(kListTypeName));
    case Type::kMap:
      return CreateCelType(CelTypeHolder(kMapTypeName));
    case Type::kCelType:
      return CreateCelType(CelTypeHolder(kCelTypeTypeName));
    case Type::kUnknownSet:
      return *this;
    case Type::kError:
      return *this;
    default: {
      static const CelError* invalid_type_error =
          new CelError(absl::InvalidArgumentError("Unsupported CelValue type"));
      return CreateError(invalid_type_error);
    }
  }
}

// Returns debug string describing a value
const std::string CelValue::DebugString() const {
  google::protobuf::Arena arena;
  return absl::StrCat(CelValue::TypeName(type()), ": ",
                      InternalVisit<std::string>(DebugStringVisitor{&arena}));
}

CelValue CreateErrorValue(cel::MemoryManager& manager,
                          absl::string_view message,
                          absl::StatusCode error_code) {
  // TODO(issues/5): assume arena-style allocator while migrating to new
  // value type.
  Arena* arena = cel::extensions::ProtoMemoryManager::CastToProtoArena(manager);
  return CreateErrorValue(arena, message, error_code);
}

CelValue CreateErrorValue(Arena* arena, absl::string_view message,
                          absl::StatusCode error_code) {
  CelError* error = Arena::Create<CelError>(arena, error_code, message);
  return CelValue::CreateError(error);
}

CelValue CreateNoMatchingOverloadError(cel::MemoryManager& manager,
                                       absl::string_view fn) {
  return CelValue::CreateError(
      interop::CreateNoMatchingOverloadError(manager, fn));
}

CelValue CreateNoMatchingOverloadError(google::protobuf::Arena* arena,
                                       absl::string_view fn) {
  return CelValue::CreateError(
      interop::CreateNoMatchingOverloadError(arena, fn));
}

bool CheckNoMatchingOverloadError(CelValue value) {
  return value.IsError() &&
         value.ErrorOrDie()->code() == absl::StatusCode::kUnknown &&
         absl::StrContains(value.ErrorOrDie()->message(),
                           interop::kErrNoMatchingOverload);
}

CelValue CreateNoSuchFieldError(cel::MemoryManager& manager,
                                absl::string_view field) {
  return CelValue::CreateError(interop::CreateNoSuchFieldError(manager, field));
}

CelValue CreateNoSuchFieldError(google::protobuf::Arena* arena, absl::string_view field) {
  return CelValue::CreateError(interop::CreateNoSuchFieldError(arena, field));
}

CelValue CreateNoSuchKeyError(cel::MemoryManager& manager,
                              absl::string_view key) {
  return CelValue::CreateError(interop::CreateNoSuchKeyError(manager, key));
}

CelValue CreateNoSuchKeyError(google::protobuf::Arena* arena, absl::string_view key) {
  return CelValue::CreateError(interop::CreateNoSuchKeyError(arena, key));
}

bool CheckNoSuchKeyError(CelValue value) {
  return value.IsError() && absl::StartsWith(value.ErrorOrDie()->message(),
                                             interop::kErrNoSuchKey);
}

CelValue CreateUnknownValueError(google::protobuf::Arena* arena,
                                 absl::string_view unknown_path) {
  return CelValue::CreateError(
      interop::CreateUnknownValueError(arena, unknown_path));
}

bool IsUnknownValueError(const CelValue& value) {
  // TODO(issues/41): replace with the implementation of go/cel-known-unknowns
  if (!value.IsError()) return false;
  const CelError* error = value.ErrorOrDie();
  if (error && error->code() == absl::StatusCode::kUnavailable) {
    auto path = error->GetPayload(interop::kPayloadUrlUnknownPath);
    return path.has_value();
  }
  return false;
}

CelValue CreateMissingAttributeError(google::protobuf::Arena* arena,
                                     absl::string_view missing_attribute_path) {
  return CelValue::CreateError(
      interop::CreateMissingAttributeError(arena, missing_attribute_path));
}

CelValue CreateMissingAttributeError(cel::MemoryManager& manager,
                                     absl::string_view missing_attribute_path) {
  // TODO(issues/5): assume arena-style allocator while migrating
  // to new value type.
  return CelValue::CreateError(
      interop::CreateMissingAttributeError(manager, missing_attribute_path));
}

bool IsMissingAttributeError(const CelValue& value) {
  const CelError* error;
  if (!value.GetValue(&error)) return false;
  if (error && error->code() == absl::StatusCode::kInvalidArgument) {
    auto path = error->GetPayload(interop::kPayloadUrlMissingAttributePath);
    return path.has_value();
  }
  return false;
}

CelValue CreateUnknownFunctionResultError(cel::MemoryManager& manager,
                                          absl::string_view help_message) {
  return CelValue::CreateError(
      interop::CreateUnknownFunctionResultError(manager, help_message));
}

CelValue CreateUnknownFunctionResultError(google::protobuf::Arena* arena,
                                          absl::string_view help_message) {
  return CelValue::CreateError(
      interop::CreateUnknownFunctionResultError(arena, help_message));
}

bool IsUnknownFunctionResult(const CelValue& value) {
  const CelError* error;
  if (!value.GetValue(&error)) return false;

  if (error == nullptr || error->code() != absl::StatusCode::kUnavailable) {
    return false;
  }
  auto payload = error->GetPayload(interop::kPayloadUrlUnknownFunctionResult);
  return payload.has_value() && payload.value() == "true";
}

}  // namespace google::api::expr::runtime
