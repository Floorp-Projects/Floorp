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
