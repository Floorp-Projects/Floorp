#include "nspr.h"
#include "nss.h"
#include "prenv.h"
#include "ssl.h"

#include <cstdlib>

#include "test_io.h"
#include "databuffer.h"

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
      nss_test::DataBuffer::SetLogLimit(16384);
    }
  }

  if (NSS_Initialize(g_working_dir_path.c_str(), "", "", SECMOD_DB,
                     NSS_INIT_READONLY) != SECSuccess) {
    return 1;
  }
  if (NSS_SetDomesticPolicy() != SECSuccess) {
    return 1;
  }
  if (NSS_SetAlgorithmPolicy(SEC_OID_XYBER768D00, NSS_USE_ALG_IN_SSL_KX, 0) !=
      SECSuccess) {
    return 1;
  }
  int rv = RUN_ALL_TESTS();

  if (NSS_Shutdown() != SECSuccess) {
    return 1;
  }

  nss_test::Poller::Shutdown();

  return rv;
}
