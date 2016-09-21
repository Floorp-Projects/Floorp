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

extern "C" void Rust_Test_StringReprInformation(size_t* size,
                                                size_t* align,
                                                size_t* dataOff,
                                                size_t* dataSize,
                                                size_t* lengthOff,
                                                size_t* lengthSize,
                                                size_t* flagsOff,
                                                size_t* flagsSize);
TEST(RustNsString, StringRepr) {
  class nsStringHack : public nsString {
  public:
    static void RunTest() {
      size_t size, align, dataOff, dataSize, lengthOff, lengthSize, flagsOff, flagsSize;
      Rust_Test_StringReprInformation(&size, &align,
                                      &dataOff, &dataSize,
                                      &lengthOff, &lengthSize,
                                      &flagsOff, &flagsSize);
      EXPECT_EQ(sizeof(nsString), sizeof(nsStringHack));
      EXPECT_EQ(size, sizeof(nsStringHack));
      EXPECT_EQ(align, alignof(nsStringHack));
      EXPECT_EQ(dataOff, offsetof(nsStringHack, mData));
      EXPECT_EQ(dataSize, sizeof(mozilla::DeclVal<nsStringHack>().mData));
      EXPECT_EQ(lengthOff, offsetof(nsStringHack, mLength));
      EXPECT_EQ(lengthSize, sizeof(mozilla::DeclVal<nsStringHack>().mLength));
      EXPECT_EQ(flagsOff, offsetof(nsStringHack, mFlags));
      EXPECT_EQ(flagsSize, sizeof(mozilla::DeclVal<nsStringHack>().mFlags));
    }
  };
  nsStringHack::RunTest();
}

extern "C" void Rust_Test_CStringReprInformation(size_t* size,
                                                 size_t* align,
                                                 size_t* dataOff,
                                                 size_t* dataSize,
                                                 size_t* lengthOff,
                                                 size_t* lengthSize,
                                                 size_t* flagsOff,
                                                 size_t* flagsSize);
TEST(RustNsString, CStringRepr) {
  class nsCStringHack : public nsCString {
  public:
    static void RunTest() {
      size_t size, align, dataOff, dataSize, lengthOff, lengthSize, flagsOff, flagsSize;
      Rust_Test_CStringReprInformation(&size, &align,
                                       &dataOff, &dataSize,
                                       &lengthOff, &lengthSize,
                                       &flagsOff, &flagsSize);
      EXPECT_EQ(sizeof(nsCString), sizeof(nsCStringHack));
      EXPECT_EQ(size, sizeof(nsCStringHack));
      EXPECT_EQ(align, alignof(nsCStringHack));
      EXPECT_EQ(dataOff, offsetof(nsCStringHack, mData));
      EXPECT_EQ(dataSize, sizeof(mozilla::DeclVal<nsCStringHack>().mData));
      EXPECT_EQ(lengthOff, offsetof(nsCStringHack, mLength));
      EXPECT_EQ(lengthSize, sizeof(mozilla::DeclVal<nsCStringHack>().mLength));
      EXPECT_EQ(flagsOff, offsetof(nsCStringHack, mFlags));
      EXPECT_EQ(flagsSize, sizeof(mozilla::DeclVal<nsCStringHack>().mFlags));
    }
  };
  nsCStringHack::RunTest();
}

extern "C" void Rust_Test_FixedStringReprInformation(size_t* size,
                                                     size_t* align,
                                                     size_t* baseOff,
                                                     size_t* baseSize,
                                                     size_t* capacityOff,
                                                     size_t* capacitySize,
                                                     size_t* bufferOff,
                                                     size_t* bufferSize);
TEST(RustNsString, FixedStringRepr) {
  class nsFixedStringHack : public nsFixedString {
  public:
    static void RunTest() {
      size_t size, align, baseOff, baseSize, capacityOff, capacitySize, bufferOff, bufferSize;
      Rust_Test_FixedStringReprInformation(&size, &align,
                                           &baseOff, &baseSize,
                                           &capacityOff, &capacitySize,
                                           &bufferOff, &bufferSize);
      EXPECT_EQ(sizeof(nsFixedString), sizeof(nsFixedStringHack));
      EXPECT_EQ(size, sizeof(nsFixedStringHack));
      EXPECT_EQ(align, alignof(nsFixedStringHack));
      EXPECT_EQ(baseOff, (size_t)0);
      EXPECT_EQ(baseSize, sizeof(nsString));
      EXPECT_EQ(capacityOff, offsetof(nsFixedStringHack, mFixedCapacity));
      EXPECT_EQ(capacitySize, sizeof(mozilla::DeclVal<nsFixedStringHack>().mFixedCapacity));
      EXPECT_EQ(bufferOff, offsetof(nsFixedStringHack, mFixedBuf));
      EXPECT_EQ(bufferSize, sizeof(mozilla::DeclVal<nsFixedStringHack>().mFixedBuf));
    }
  };
  nsFixedStringHack::RunTest();
}

extern "C" void Rust_Test_FixedCStringReprInformation(size_t* size,
                                                      size_t* align,
                                                      size_t* baseOff,
                                                      size_t* baseSize,
                                                      size_t* capacityOff,
                                                      size_t* capacitySize,
                                                      size_t* bufferOff,
                                                      size_t* bufferSize);
TEST(RustNsString, FixedCStringRepr) {
  class nsFixedCStringHack : public nsFixedCString {
  public:
    static void RunTest() {
      size_t size, align, baseOff, baseSize, capacityOff, capacitySize, bufferOff, bufferSize;
      Rust_Test_FixedCStringReprInformation(&size, &align,
                                            &baseOff, &baseSize,
                                            &capacityOff, &capacitySize,
                                            &bufferOff, &bufferSize);
      EXPECT_EQ(sizeof(nsFixedCString), sizeof(nsFixedCStringHack));
      EXPECT_EQ(size, sizeof(nsFixedCStringHack));
      EXPECT_EQ(align, alignof(nsFixedCStringHack));
      EXPECT_EQ(baseOff, (size_t)0);
      EXPECT_EQ(baseSize, sizeof(nsCString));
      EXPECT_EQ(capacityOff, offsetof(nsFixedCStringHack, mFixedCapacity));
      EXPECT_EQ(capacitySize, sizeof(mozilla::DeclVal<nsFixedCStringHack>().mFixedCapacity));
      EXPECT_EQ(bufferOff, offsetof(nsFixedCStringHack, mFixedBuf));
      EXPECT_EQ(bufferSize, sizeof(mozilla::DeclVal<nsFixedCStringHack>().mFixedBuf));
    }
  };
  nsFixedCStringHack::RunTest();
}

extern "C" void Rust_Test_NsStringFlags(uint32_t* f_none,
                                        uint32_t* f_terminated,
                                        uint32_t* f_voided,
                                        uint32_t* f_shared,
                                        uint32_t* f_owned,
                                        uint32_t* f_fixed,
                                        uint32_t* f_literal,
                                        uint32_t* f_class_fixed);
TEST(RustNsString, NsStringFlags) {
  uint32_t f_none, f_terminated, f_voided, f_shared, f_owned, f_fixed, f_literal, f_class_fixed;
  Rust_Test_NsStringFlags(&f_none, &f_terminated,
                          &f_voided, &f_shared,
                          &f_owned, &f_fixed,
                          &f_literal, &f_class_fixed);
  EXPECT_EQ(f_none, nsAString::F_NONE);
  EXPECT_EQ(f_none, nsACString::F_NONE);
  EXPECT_EQ(f_terminated, nsAString::F_TERMINATED);
  EXPECT_EQ(f_terminated, nsACString::F_TERMINATED);
  EXPECT_EQ(f_voided, nsAString::F_VOIDED);
  EXPECT_EQ(f_voided, nsACString::F_VOIDED);
  EXPECT_EQ(f_shared, nsAString::F_SHARED);
  EXPECT_EQ(f_shared, nsACString::F_SHARED);
  EXPECT_EQ(f_owned, nsAString::F_OWNED);
  EXPECT_EQ(f_owned, nsACString::F_OWNED);
  EXPECT_EQ(f_fixed, nsAString::F_FIXED);
  EXPECT_EQ(f_fixed, nsACString::F_FIXED);
  EXPECT_EQ(f_literal, nsAString::F_LITERAL);
  EXPECT_EQ(f_literal, nsACString::F_LITERAL);
  EXPECT_EQ(f_class_fixed, nsAString::F_CLASS_FIXED);
  EXPECT_EQ(f_class_fixed, nsACString::F_CLASS_FIXED);
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
