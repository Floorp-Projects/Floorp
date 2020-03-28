#include "gtest/gtest.h"
#include <stdint.h>
#include <utility>
#include "nsString.h"

extern "C" {
// This function is called by the rust code in test.rs if a non-fatal test
// failure occurs.
void GTest_ExpectFailure(const char* aMessage) { EXPECT_STREQ(aMessage, ""); }
}

#define SIZE_ALIGN_CHECK(Clazz)                                   \
  extern "C" void Rust_Test_ReprSizeAlign_##Clazz(size_t* size,   \
                                                  size_t* align); \
  TEST(RustNsString, ReprSizeAlign_##Clazz)                       \
  {                                                               \
    size_t size, align;                                           \
    Rust_Test_ReprSizeAlign_##Clazz(&size, &align);               \
    EXPECT_EQ(size, sizeof(Clazz));                               \
    EXPECT_EQ(align, alignof(Clazz));                             \
  }

SIZE_ALIGN_CHECK(nsString)
SIZE_ALIGN_CHECK(nsCString)

#define MEMBER_CHECK(Clazz, Member)                                       \
  extern "C" void Rust_Test_Member_##Clazz##_##Member(                    \
      size_t* size, size_t* align, size_t* offset);                       \
  TEST(RustNsString, ReprMember_##Clazz##_##Member)                       \
  {                                                                       \
    class Hack : public Clazz {                                           \
     public:                                                              \
      static void RunTest() {                                             \
        size_t size, align, offset;                                       \
        Rust_Test_Member_##Clazz##_##Member(&size, &align, &offset);      \
        EXPECT_EQ(size, sizeof(std::declval<Hack>().Member));             \
        EXPECT_EQ(align, alignof(decltype(std::declval<Hack>().Member))); \
        EXPECT_EQ(offset, offsetof(Hack, Member));                        \
      }                                                                   \
    };                                                                    \
    static_assert(sizeof(Clazz) == sizeof(Hack), "Hack matches class");   \
    Hack::RunTest();                                                      \
  }

MEMBER_CHECK(nsString, mData)
MEMBER_CHECK(nsString, mLength)
MEMBER_CHECK(nsString, mDataFlags)
MEMBER_CHECK(nsString, mClassFlags)
MEMBER_CHECK(nsCString, mData)
MEMBER_CHECK(nsCString, mLength)
MEMBER_CHECK(nsCString, mDataFlags)
MEMBER_CHECK(nsCString, mClassFlags)

extern "C" void Rust_Test_NsStringFlags(
    uint16_t* f_terminated, uint16_t* f_voided, uint16_t* f_refcounted,
    uint16_t* f_owned, uint16_t* f_inline, uint16_t* f_literal,
    uint16_t* f_class_inline, uint16_t* f_class_null_terminated);
TEST(RustNsString, NsStringFlags)
{
  uint16_t f_terminated, f_voided, f_refcounted, f_owned, f_inline, f_literal,
      f_class_inline, f_class_null_terminated;
  Rust_Test_NsStringFlags(&f_terminated, &f_voided, &f_refcounted, &f_owned,
                          &f_inline, &f_literal, &f_class_inline,
                          &f_class_null_terminated);
  EXPECT_EQ(f_terminated, uint16_t(nsAString::DataFlags::TERMINATED));
  EXPECT_EQ(f_terminated, uint16_t(nsACString::DataFlags::TERMINATED));
  EXPECT_EQ(f_voided, uint16_t(nsAString::DataFlags::VOIDED));
  EXPECT_EQ(f_voided, uint16_t(nsACString::DataFlags::VOIDED));
  EXPECT_EQ(f_refcounted, uint16_t(nsAString::DataFlags::REFCOUNTED));
  EXPECT_EQ(f_refcounted, uint16_t(nsACString::DataFlags::REFCOUNTED));
  EXPECT_EQ(f_owned, uint16_t(nsAString::DataFlags::OWNED));
  EXPECT_EQ(f_owned, uint16_t(nsACString::DataFlags::OWNED));
  EXPECT_EQ(f_inline, uint16_t(nsAString::DataFlags::INLINE));
  EXPECT_EQ(f_inline, uint16_t(nsACString::DataFlags::INLINE));
  EXPECT_EQ(f_literal, uint16_t(nsAString::DataFlags::LITERAL));
  EXPECT_EQ(f_literal, uint16_t(nsACString::DataFlags::LITERAL));
  EXPECT_EQ(f_class_inline, uint16_t(nsAString::ClassFlags::INLINE));
  EXPECT_EQ(f_class_inline, uint16_t(nsACString::ClassFlags::INLINE));
  EXPECT_EQ(f_class_null_terminated,
            uint16_t(nsAString::ClassFlags::NULL_TERMINATED));
  EXPECT_EQ(f_class_null_terminated,
            uint16_t(nsACString::ClassFlags::NULL_TERMINATED));
}

extern "C" void Rust_StringFromCpp(const nsACString* aCStr,
                                   const nsAString* aStr);
TEST(RustNsString, StringFromCpp)
{
  nsAutoCString foo;
  foo.AssignASCII("Hello, World!");

  nsAutoString bar;
  bar.AssignASCII("Hello, World!");

  Rust_StringFromCpp(&foo, &bar);
}

extern "C" void Rust_AssignFromRust(nsACString* aCStr, nsAString* aStr);
TEST(RustNsString, AssignFromRust)
{
  nsAutoCString cs;
  nsAutoString s;
  Rust_AssignFromRust(&cs, &s);
  EXPECT_TRUE(cs.EqualsASCII("Hello, World!"));
  EXPECT_TRUE(s.EqualsASCII("Hello, World!"));
}

extern "C" {
void Cpp_AssignFromCpp(nsACString* aCStr, nsAString* aStr) {
  aCStr->AssignASCII("Hello, World!");
  aStr->AssignASCII("Hello, World!");
}
}
extern "C" void Rust_AssignFromCpp();
TEST(RustNsString, AssignFromCpp)
{ Rust_AssignFromCpp(); }

extern "C" void Rust_StringWrite();
TEST(RustNsString, StringWrite)
{ Rust_StringWrite(); }

extern "C" void Rust_FromEmptyRustString();
TEST(RustNsString, FromEmptyRustString)
{ Rust_FromEmptyRustString(); }

extern "C" void Rust_WriteToBufferFromRust(nsACString* aCStr, nsAString* aStr,
                                           nsACString* aFallibleCStr,
                                           nsAString* aFallibleStr);
TEST(RustNsString, WriteToBufferFromRust)
{
  nsAutoCString cStr;
  nsAutoString str;
  nsAutoCString fallibleCStr;
  nsAutoString fallibleStr;

  cStr.AssignLiteral("abc");
  str.AssignLiteral("abc");
  fallibleCStr.AssignLiteral("abc");
  fallibleStr.AssignLiteral("abc");

  Rust_WriteToBufferFromRust(&cStr, &str, &fallibleCStr, &fallibleStr);

  EXPECT_TRUE(cStr.EqualsASCII("ABC"));
  EXPECT_TRUE(str.EqualsASCII("ABC"));
  EXPECT_TRUE(fallibleCStr.EqualsASCII("ABC"));
  EXPECT_TRUE(fallibleStr.EqualsASCII("ABC"));
}

extern "C" void Rust_InlineCapacityFromRust(const nsACString* aCStr,
                                            const nsAString* aStr,
                                            size_t* aCStrCapacity,
                                            size_t* aStrCapacity);
TEST(RustNsString, InlineCapacityFromRust)
{
  size_t cStrCapacity;
  size_t strCapacity;
  nsAutoCStringN<93> cs;
  nsAutoStringN<93> s;
  Rust_InlineCapacityFromRust(&cs, &s, &cStrCapacity, &strCapacity);
  EXPECT_EQ(cStrCapacity, 92U);
  EXPECT_EQ(strCapacity, 92U);
}
