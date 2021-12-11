#include "nspr.h"
#include "nss.h"

#include <cstdlib>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

// Tests are passed the location of their source directory
// so that they can load extra resources from there.
std::string g_source_dir;

void usage(const char *progname) {
  PR_fprintf(PR_STDERR, "Usage: %s [-s <dir>] [-d <dir> [-w]]\n", progname);
  exit(2);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  const char *workdir = "";
  uint32_t flags = NSS_INIT_READONLY;

  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-s")) {
      if (i + 1 >= argc) {
        usage(argv[0]);
      }
      i++;
      g_source_dir = argv[i];
    } else if (!strcmp(argv[i], "-d")) {
      if (i + 1 >= argc) {
        usage(argv[0]);
      }
      i++;
      workdir = argv[i];
    } else if (!strcmp(argv[i], "-w")) {
      flags &= ~NSS_INIT_READONLY;
    }
  }

  if (NSS_Initialize(workdir, "", "", SECMOD_DB, flags) != SECSuccess) {
    return 1;
  }
  int rv = RUN_ALL_TESTS();

  if (NSS_Shutdown() != SECSuccess) {
    return 1;
  }

  return rv;
}
