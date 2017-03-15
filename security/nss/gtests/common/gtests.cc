#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include <cstdlib>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    return 1;
  }
  if (NSS_SetDomesticPolicy() != SECSuccess) {
    return 1;
  }
  int rv = RUN_ALL_TESTS();

  if (NSS_Shutdown() != SECSuccess) {
    return 1;
  }

  return rv;
}
