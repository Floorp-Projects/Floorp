#include "gtest/gtest.h"
#include <stdint.h>
#include "nsString.h"

extern "C" {
  // This function is called by the rust code in test.rs if a non-fatal test
  // failure occurs.
  void GTest_ExpectFailure(const char* aMessage) {
    EXPECT_STREQ(aMessage, "");
  }
}

#define SIZE_ALIGN_CHECK(Clazz)                                         \
  extern "C" void Rust_Test_ReprSizeAlign_##Clazz(size_t* size, size_t* align); \
  TEST(RustNsString, ReprSizeAlign_##Clazz) {                           \
    size_t size, align;                                                 \
    Rust_Test_ReprSizeAlign_##Clazz(&size, &align);                     \
    EXPECT_EQ(size, sizeof(Clazz));                                     \
    EXPECT_EQ(align, alignof(Clazz));                                   \
  }

SIZE_ALIGN_CHECK(nsString)
SIZE_ALIGN_CHECK(nsCString)
SIZE_ALIGN_CHECK(nsFixedString)
SIZE_ALIGN_CHECK(nsFixedCString)

#define MEMBER_CHECK(Clazz, Member)                                     \
  extern "C" void Rust_Test_Member_##Clazz##_##Member(size_t* size,     \
                                                      size_t* align,    \
                                                      size_t* offset);  \
  TEST(RustNsString, ReprMember_##Clazz##_##Member) {                   \
    class Hack : public Clazz {                                         \
    public:                                                             \
      static void RunTest() {                                           \
        size_t size, align, offset;                                     \
        Rust_Test_Member_##Clazz##_##Member(&size, &align, &offset);    \
        EXPECT_EQ(size, sizeof(mozilla::DeclVal<Hack>().Member));       \
        EXPECT_EQ(size, alignof(decltype(mozilla::DeclVal<Hack>().Member))); \
        EXPECT_EQ(offset, offsetof(Hack, Member));                      \
      }                                                                 \
    };                                                                  \
    static_assert(sizeof(Clazz) == sizeof(Hack), "Hack matches class"); \
    Hack::RunTest();                                                    \
  }

MEMBER_CHECK(nsString, mData)
MEMBER_CHECK(nsString, mLength)
MEMBER_CHECK(nsString, mDataFlags)
MEMBER_CHECK(nsString, mClassFlags)
MEMBER_CHECK(nsCString, mData)
MEMBER_CHECK(nsCString, mLength)
MEMBER_CHECK(nsCString, mDataFlags)
MEMBER_CHECK(nsCString, mClassFlags)
MEMBER_CHECK(nsFixedString, mFixedCapacity)
MEMBER_CHECK(nsFixedString, mFixedBuf)
MEMBER_CHECK(nsFixedCString, mFixedCapacity)
MEMBER_CHECK(nsFixedCString, mFixedBuf)

extern "C" void Rust_Test_NsStringFlags(uint16_t* f_terminated,
                                        uint16_t* f_voided,
                                        uint16_t* f_shared,
                                        uint16_t* f_owned,
                                        uint16_t* f_fixed,
                                        uint16_t* f_literal,
                                        uint16_t* f_class_fixed,
                                        uint16_t* f_class_null_terminated);
TEST(RustNsString, NsStringFlags) {
  uint16_t f_terminated, f_voided, f_shared, f_owned, f_fixed, f_literal,
           f_class_fixed, f_class_null_terminated;
  Rust_Test_NsStringFlags(&f_terminated,
                          &f_voided, &f_shared,
                          &f_owned, &f_fixed,
                          &f_literal, &f_class_fixed, &f_class_null_terminated);
  EXPECT_EQ(f_terminated, uint16_t(nsAString::DataFlags::TERMINATED));
  EXPECT_EQ(f_terminated, uint16_t(nsACString::DataFlags::TERMINATED));
  EXPECT_EQ(f_voided, uint16_t(nsAString::DataFlags::VOIDED));
  EXPECT_EQ(f_voided, uint16_t(nsACString::DataFlags::VOIDED));
  EXPECT_EQ(f_shared, uint16_t(nsAString::DataFlags::SHARED));
  EXPECT_EQ(f_shared, uint16_t(nsACString::DataFlags::SHARED));
  EXPECT_EQ(f_owned, uint16_t(nsAString::DataFlags::OWNED));
  EXPECT_EQ(f_owned, uint16_t(nsACString::DataFlags::OWNED));
  EXPECT_EQ(f_fixed, uint16_t(nsAString::DataFlags::FIXED));
  EXPECT_EQ(f_fixed, uint16_t(nsACString::DataFlags::FIXED));
  EXPECT_EQ(f_literal, uint16_t(nsAString::DataFlags::LITERAL));
  EXPECT_EQ(f_literal, uint16_t(nsACString::DataFlags::LITERAL));
  EXPECT_EQ(f_class_fixed, uint16_t(nsAString::ClassFlags::FIXED));
  EXPECT_EQ(f_class_fixed, uint16_t(nsACString::ClassFlags::FIXED));
  EXPECT_EQ(f_class_null_terminated, uint16_t(nsAString::ClassFlags::NULL_TERMINATED));
  EXPECT_EQ(f_class_null_terminated, uint16_t(nsACString::ClassFlags::NULL_TERMINATED));
}

extern "C" void Rust_StringFromCpp(const nsACString* aCStr, const nsAString* aStr);
TEST(RustNsString, StringFromCpp) {
  nsAutoCString foo;
  foo.AssignASCII("Hello, World!");

  nsAutoString bar;
  bar.AssignASCII("Hello, World!");

  Rust_StringFromCpp(&foo, &bar);
}

extern "C" void Rust_AssignFromRust(nsACString* aCStr, nsAString* aStr);
TEST(RustNsString, AssignFromRust) {
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
TEST(RustNsString, AssignFromCpp) {
  Rust_AssignFromCpp();
}
extern "C" void Rust_FixedAssignFromCpp();
TEST(RustNsString, FixedAssignFromCpp) {
  Rust_FixedAssignFromCpp();
}
extern "C" void Rust_AutoAssignFromCpp();
TEST(RustNsString, AutoAssignFromCpp) {
  Rust_AutoAssignFromCpp();
}

extern "C" void Rust_StringWrite();
TEST(RustNsString, StringWrite) {
  Rust_StringWrite();
}

extern "C" void Rust_FromEmptyRustString();
TEST(RustNsString, FromEmptyRustString) {
  Rust_FromEmptyRustString();
}

extern "C" void Rust_WriteToBufferFromRust(nsACString* aCStr, nsAString* aStr, nsACString* aFallibleCStr, nsAString* aFallibleStr);
TEST(RustNsString, WriteToBufferFromRust) {
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

