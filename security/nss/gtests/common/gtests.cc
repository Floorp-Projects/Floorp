#include "nspr.h"
#include "nss.h"

#include <cstdlib>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  const char *workdir = "";
  uint32_t flags = NSS_INIT_READONLY;

  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-d")) {
      if (i + 1 >= argc) {
        PR_fprintf(PR_STDERR, "Usage: %s [-d <dir> [-w]]\n", argv[0]);
        exit(2);
      }
      workdir = argv[i + 1];
      i++;
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
