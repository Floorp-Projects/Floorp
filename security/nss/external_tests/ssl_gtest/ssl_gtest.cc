#include "ssl.h"
#include "nspr.h"
#include "nss.h"
#include "prenv.h"

#include <cstdlib>

#include "test_io.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

std::string g_working_dir_path;
bool g_ssl_gtest_verbose;

int main(int argc, char** argv) {
  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);
  g_working_dir_path = ".";
  g_ssl_gtest_verbose = false;

  char* workdir = PR_GetEnvSecure("NSS_GTEST_WORKDIR");
  if (workdir) g_working_dir_path = workdir;

  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-d")) {
      g_working_dir_path = argv[i + 1];
      ++i;
    } else if (!strcmp(argv[i], "-v")) {
      g_ssl_gtest_verbose = true;
    }
  }

  NSS_Initialize(g_working_dir_path.c_str(), "", "", SECMOD_DB,
                 NSS_INIT_READONLY);
  NSS_SetDomesticPolicy();
  int rv = RUN_ALL_TESTS();

  NSS_Shutdown();

  nss_test::Poller::Shutdown();

  return rv;
}
