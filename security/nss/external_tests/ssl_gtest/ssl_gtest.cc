#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "test_io.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

std::string g_working_dir_path;

int main(int argc, char **argv) {
  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);
  g_working_dir_path = ".";

  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-d")) {
      g_working_dir_path = argv[i + 1];
      ++i;
    }
  }

  NSS_Initialize(g_working_dir_path.c_str(), "", "", SECMOD_DB, NSS_INIT_READONLY);
  NSS_SetDomesticPolicy();
  int rv = RUN_ALL_TESTS();

  NSS_Shutdown();

  nss_test::Poller::Shutdown();

  return rv;
}
