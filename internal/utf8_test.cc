#include "internal/utf8.h"

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "internal/benchmark.h"
#include "internal/testing.h"

// Tests is based on
// https://go.googlesource.com/go/+/refs/heads/master/src/unicode/utf8/utf8.go
// but adapted for C++.

namespace cel::internal {
namespace {

TEST(Utf8IsValid, String) {
  EXPECT_TRUE(Utf8IsValid(""));
  EXPECT_TRUE(Utf8IsValid("a"));
  EXPECT_TRUE(Utf8IsValid("abc"));
  EXPECT_TRUE(Utf8IsValid("\xd0\x96"));
  EXPECT_TRUE(Utf8IsValid("\xd0\x96\xd0\x96"));
  EXPECT_TRUE(Utf8IsValid(
      "\xd0\xb1\xd1\x80\xd1\x8d\xd0\xb4-\xd0\x9b\xd0\x93\xd0\xa2\xd0\x9c"));
  EXPECT_TRUE(Utf8IsValid("\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9"));
  EXPECT_TRUE(Utf8IsValid("a\ufffdb"));
  EXPECT_TRUE(Utf8IsValid("\xf4\x8f\xbf\xbf"));

  EXPECT_FALSE(Utf8IsValid("\x42\xfa"));
  EXPECT_FALSE(Utf8IsValid("\x42\xfa\x43"));
  EXPECT_FALSE(Utf8IsValid("\xf4\x90\x80\x80"));
  EXPECT_FALSE(Utf8IsValid("\xf7\xbf\xbf\xbf"));
  EXPECT_FALSE(Utf8IsValid("\xfb\xbf\xbf\xbf\xbf"));
  EXPECT_FALSE(Utf8IsValid("\xc0\x80"));
  EXPECT_FALSE(Utf8IsValid("\xed\xa0\x80"));
  EXPECT_FALSE(Utf8IsValid("\xed\xbf\xbf"));
}

TEST(Utf8IsValid, Cord) {
  EXPECT_TRUE(Utf8IsValid(absl::Cord("")));
  EXPECT_TRUE(Utf8IsValid(absl::Cord("a")));
  EXPECT_TRUE(Utf8IsValid(absl::Cord("abc")));
  EXPECT_TRUE(Utf8IsValid(absl::Cord("\xd0\x96")));
  EXPECT_TRUE(Utf8IsValid(absl::Cord("\xd0\x96\xd0\x96")));
  EXPECT_TRUE(Utf8IsValid(absl::Cord(
      "\xd0\xb1\xd1\x80\xd1\x8d\xd0\xb4-\xd0\x9b\xd0\x93\xd0\xa2\xd0\x9c")));
  EXPECT_TRUE(Utf8IsValid(absl::Cord("\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9")));
  EXPECT_TRUE(Utf8IsValid(absl::Cord("a\ufffdb")));
  EXPECT_TRUE(Utf8IsValid(absl::Cord("\xf4\x8f\xbf\xbf")));

  EXPECT_FALSE(Utf8IsValid(absl::Cord("\x42\xfa")));
  EXPECT_FALSE(Utf8IsValid(absl::Cord("\x42\xfa\x43")));
  EXPECT_FALSE(Utf8IsValid(absl::Cord("\xf4\x90\x80\x80")));
  EXPECT_FALSE(Utf8IsValid(absl::Cord("\xf7\xbf\xbf\xbf")));
  EXPECT_FALSE(Utf8IsValid(absl::Cord("\xfb\xbf\xbf\xbf\xbf")));
  EXPECT_FALSE(Utf8IsValid(absl::Cord("\xc0\x80")));
  EXPECT_FALSE(Utf8IsValid(absl::Cord("\xed\xa0\x80")));
  EXPECT_FALSE(Utf8IsValid(absl::Cord("\xed\xbf\xbf")));
}

TEST(Utf8CodePointCount, String) {
  EXPECT_EQ(Utf8CodePointCount("abcd"), 4);
  EXPECT_EQ(Utf8CodePointCount("1,2,3,4"), 7);
  EXPECT_EQ(Utf8CodePointCount("\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9"), 3);
  EXPECT_EQ(Utf8CodePointCount(absl::string_view("\xe2\x00", 2)), 2);
  EXPECT_EQ(Utf8CodePointCount("\xe2\x80"), 2);
  EXPECT_EQ(Utf8CodePointCount("a\xe2\x80"), 3);
}

TEST(Utf8CodePointCount, Cord) {
  EXPECT_EQ(Utf8CodePointCount(absl::Cord("abcd")), 4);
  EXPECT_EQ(Utf8CodePointCount(absl::Cord("1,2,3,4")), 7);
  EXPECT_EQ(
      Utf8CodePointCount(absl::Cord("\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9")),
      3);
  EXPECT_EQ(Utf8CodePointCount(absl::Cord(absl::string_view("\xe2\x00", 2))),
            2);
  EXPECT_EQ(Utf8CodePointCount(absl::Cord("\xe2\x80")), 2);
  EXPECT_EQ(Utf8CodePointCount(absl::Cord("a\xe2\x80")), 3);
}

TEST(Utf8Validate, String) {
  EXPECT_TRUE(Utf8Validate("").second);
  EXPECT_TRUE(Utf8Validate("a").second);
  EXPECT_TRUE(Utf8Validate("abc").second);
  EXPECT_TRUE(Utf8Validate("\xd0\x96").second);
  EXPECT_TRUE(Utf8Validate("\xd0\x96\xd0\x96").second);
  EXPECT_TRUE(
      Utf8Validate(
          "\xd0\xb1\xd1\x80\xd1\x8d\xd0\xb4-\xd0\x9b\xd0\x93\xd0\xa2\xd0\x9c")
          .second);
  EXPECT_TRUE(Utf8Validate("\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9").second);
  EXPECT_TRUE(Utf8Validate("a\ufffdb").second);
  EXPECT_TRUE(Utf8Validate("\xf4\x8f\xbf\xbf").second);

  EXPECT_FALSE(Utf8Validate("\x42\xfa").second);
  EXPECT_FALSE(Utf8Validate("\x42\xfa\x43").second);
  EXPECT_FALSE(Utf8Validate("\xf4\x90\x80\x80").second);
  EXPECT_FALSE(Utf8Validate("\xf7\xbf\xbf\xbf").second);
  EXPECT_FALSE(Utf8Validate("\xfb\xbf\xbf\xbf\xbf").second);
  EXPECT_FALSE(Utf8Validate("\xc0\x80").second);
  EXPECT_FALSE(Utf8Validate("\xed\xa0\x80").second);
  EXPECT_FALSE(Utf8Validate("\xed\xbf\xbf").second);

  EXPECT_EQ(Utf8Validate("abcd").first, 4);
  EXPECT_EQ(Utf8Validate("1,2,3,4").first, 7);
  EXPECT_EQ(Utf8Validate("\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9").first, 3);
  EXPECT_EQ(Utf8Validate(absl::string_view("\xe2\x00", 2)).first, 0);
  EXPECT_EQ(Utf8Validate("\xe2\x80").first, 0);
  EXPECT_EQ(Utf8Validate("a\xe2\x80").first, 1);
}

TEST(Utf8Validate, Cord) {
  EXPECT_TRUE(Utf8Validate(absl::Cord("")).second);
  EXPECT_TRUE(Utf8Validate(absl::Cord("a")).second);
  EXPECT_TRUE(Utf8Validate(absl::Cord("abc")).second);
  EXPECT_TRUE(Utf8Validate(absl::Cord("\xd0\x96")).second);
  EXPECT_TRUE(Utf8Validate(absl::Cord("\xd0\x96\xd0\x96")).second);
  EXPECT_TRUE(Utf8Validate(absl::Cord("\xd0\xb1\xd1\x80\xd1\x8d\xd0\xb4-"
                                      "\xd0\x9b\xd0\x93\xd0\xa2\xd0\x9c"))
                  .second);
  EXPECT_TRUE(
      Utf8Validate(absl::Cord("\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9")).second);
  EXPECT_TRUE(Utf8Validate(absl::Cord("a\ufffdb")).second);
  EXPECT_TRUE(Utf8Validate(absl::Cord("\xf4\x8f\xbf\xbf")).second);

  EXPECT_FALSE(Utf8Validate(absl::Cord("\x42\xfa")).second);
  EXPECT_FALSE(Utf8Validate(absl::Cord("\x42\xfa\x43")).second);
  EXPECT_FALSE(Utf8Validate(absl::Cord("\xf4\x90\x80\x80")).second);
  EXPECT_FALSE(Utf8Validate(absl::Cord("\xf7\xbf\xbf\xbf")).second);
  EXPECT_FALSE(Utf8Validate(absl::Cord("\xfb\xbf\xbf\xbf\xbf")).second);
  EXPECT_FALSE(Utf8Validate(absl::Cord("\xc0\x80")).second);
  EXPECT_FALSE(Utf8Validate(absl::Cord("\xed\xa0\x80")).second);
  EXPECT_FALSE(Utf8Validate(absl::Cord("\xed\xbf\xbf")).second);

  EXPECT_EQ(Utf8Validate(absl::Cord("abcd")).first, 4);
  EXPECT_EQ(Utf8Validate(absl::Cord("1,2,3,4")).first, 7);
  EXPECT_EQ(
      Utf8Validate(absl::Cord("\xe2\x98\xba\xe2\x98\xbb\xe2\x98\xb9")).first,
      3);
  EXPECT_EQ(Utf8Validate(absl::Cord(absl::string_view("\xe2\x00", 2))).first,
            0);
  EXPECT_EQ(Utf8Validate(absl::Cord("\xe2\x80")).first, 0);
  EXPECT_EQ(Utf8Validate(absl::Cord("a\xe2\x80")).first, 1);
}

void BM_Utf8CodePointCount_String_AsciiTen(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8CodePointCount("0123456789"));
  }
}

BENCHMARK(BM_Utf8CodePointCount_String_AsciiTen);

void BM_Utf8CodePointCount_Cord_AsciiTen(benchmark::State& state) {
  absl::Cord value("0123456789");
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8CodePointCount(value));
  }
}

BENCHMARK(BM_Utf8CodePointCount_Cord_AsciiTen);

void BM_Utf8CodePointCount_String_JapaneseTen(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8CodePointCount(
        "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa"
        "\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5"));
  }
}

BENCHMARK(BM_Utf8CodePointCount_String_JapaneseTen);

void BM_Utf8CodePointCount_Cord_JapaneseTen(benchmark::State& state) {
  absl::Cord value(
      "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa"
      "\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5");
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8CodePointCount(value));
  }
}

BENCHMARK(BM_Utf8CodePointCount_Cord_JapaneseTen);

void BM_Utf8IsValid_String_AsciiTen(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8IsValid("0123456789"));
  }
}

BENCHMARK(BM_Utf8IsValid_String_AsciiTen);

void BM_Utf8IsValid_Cord_AsciiTen(benchmark::State& state) {
  absl::Cord value("0123456789");
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8IsValid(value));
  }
}

BENCHMARK(BM_Utf8IsValid_Cord_AsciiTen);

void BM_Utf8IsValid_String_JapaneseTen(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8IsValid(
        "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa"
        "\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5"));
  }
}

BENCHMARK(BM_Utf8IsValid_String_JapaneseTen);

void BM_Utf8IsValid_Cord_JapaneseTen(benchmark::State& state) {
  absl::Cord value(
      "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa"
      "\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5");
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8IsValid(value));
  }
}

BENCHMARK(BM_Utf8IsValid_Cord_JapaneseTen);

void BM_Utf8Validate_String_AsciiTen(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8Validate("0123456789"));
  }
}

BENCHMARK(BM_Utf8Validate_String_AsciiTen);

void BM_Utf8Validate_Cord_AsciiTen(benchmark::State& state) {
  absl::Cord value("0123456789");
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8Validate(value));
  }
}

BENCHMARK(BM_Utf8Validate_Cord_AsciiTen);

void BM_Utf8Validate_String_JapaneseTen(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8Validate(
        "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa"
        "\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5"));
  }
}

BENCHMARK(BM_Utf8Validate_String_JapaneseTen);

void BM_Utf8Validate_Cord_JapaneseTen(benchmark::State& state) {
  absl::Cord value(
      "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa"
      "\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5");
  for (auto s : state) {
    benchmark::DoNotOptimize(Utf8Validate(value));
  }
}

BENCHMARK(BM_Utf8Validate_Cord_JapaneseTen);

}  // namespace
}  // namespace cel::internal
