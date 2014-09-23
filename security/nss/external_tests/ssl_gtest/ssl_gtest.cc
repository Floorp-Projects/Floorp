#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "test_io.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

int main(int argc, char **argv) {
  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);
  std::string path = ".";

  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-d")) {
      path = argv[i + 1];
      ++i;
    }
  }

  NSS_Initialize(path.c_str(), "", "", SECMOD_DB, NSS_INIT_READONLY);
  NSS_SetDomesticPolicy();

  int rv = RUN_ALL_TESTS();

  NSS_Shutdown();

  nss_test::Poller::Shutdown();

  return rv;
}
