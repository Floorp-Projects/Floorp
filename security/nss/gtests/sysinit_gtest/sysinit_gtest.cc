#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

int main(int argc, char** argv) {
  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);
  int rv = RUN_ALL_TESTS();
  return rv;
}
