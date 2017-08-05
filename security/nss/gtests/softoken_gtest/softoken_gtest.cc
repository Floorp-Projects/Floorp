#include <cstdlib>

#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

#include "scoped_ptrs.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

namespace nss_test {

// Given a prefix, attempts to create a unique directory that the user can do
// work in without impacting other tests. For example, if given the prefix
// "scratch", a directory like "scratch05c17b25" will be created in the current
// working directory (or the location specified by NSS_GTEST_WORKDIR, if
// defined).
// Upon destruction, the implementation will attempt to delete the directory.
// However, no attempt is made to first remove files in the directory - the
// user is responsible for this. If the directory is not empty, deleting it will
// fail.
// Statistically, it is technically possible to fail to create a unique
// directory name, but this is extremely unlikely given the expected workload of
// this implementation.
class ScopedUniqueDirectory {
 public:
  explicit ScopedUniqueDirectory(const std::string& prefix);

  // NB: the directory must be empty upon destruction
  ~ScopedUniqueDirectory() { assert(rmdir(mPath.c_str()) == 0); }

  const std::string& GetPath() { return mPath; }

 private:
  static const int RETRY_LIMIT = 5;
  static void GenerateRandomName(/*in/out*/ std::string& prefix);
  static bool TryMakingDirectory(/*in/out*/ std::string& prefix);

  std::string mPath;
};

ScopedUniqueDirectory::ScopedUniqueDirectory(const std::string& prefix) {
  std::string path;
  const char* workingDirectory = PR_GetEnvSecure("NSS_GTEST_WORKDIR");
  if (workingDirectory) {
    path.assign(workingDirectory);
  }
  path.append(prefix);
  for (int i = 0; i < RETRY_LIMIT; i++) {
    std::string pathCopy(path);
    // TryMakingDirectory will modify its input. If it fails, we want to throw
    // away the modified result.
    if (TryMakingDirectory(pathCopy)) {
      mPath.assign(pathCopy);
      break;
    }
  }
  assert(mPath.length() > 0);
}

void ScopedUniqueDirectory::GenerateRandomName(std::string& prefix) {
  std::stringstream ss;
  ss << prefix;
  // RAND_MAX is at least 32767.
  ss << std::setfill('0') << std::setw(4) << std::hex << rand() << rand();
  // This will overwrite the value of prefix. This is a little inefficient, but
  // at least it makes the code simple.
  ss >> prefix;
}

bool ScopedUniqueDirectory::TryMakingDirectory(std::string& prefix) {
  GenerateRandomName(prefix);
#if defined(_WIN32)
  return _mkdir(prefix.c_str()) == 0;
#else
  return mkdir(prefix.c_str(), 0777) == 0;
#endif
}

class SoftokenTest : public ::testing::Test {
 protected:
  SoftokenTest() : mNSSDBDir("SoftokenTest.d-") {}

  virtual void SetUp() {
    std::string nssInitArg("sql:");
    nssInitArg.append(mNSSDBDir.GetPath());
    ASSERT_EQ(SECSuccess, NSS_Initialize(nssInitArg.c_str(), "", "", SECMOD_DB,
                                         NSS_INIT_NOROOTINIT));
  }

  virtual void TearDown() {
    ASSERT_EQ(SECSuccess, NSS_Shutdown());
    const std::string& nssDBDirPath = mNSSDBDir.GetPath();
    ASSERT_EQ(0, unlink((nssDBDirPath + "/cert9.db").c_str()));
    ASSERT_EQ(0, unlink((nssDBDirPath + "/key4.db").c_str()));
    ASSERT_EQ(0, unlink((nssDBDirPath + "/pkcs11.txt").c_str()));
  }

  ScopedUniqueDirectory mNSSDBDir;
};

TEST_F(SoftokenTest, ResetSoftokenEmptyPassword) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, nullptr));
  EXPECT_EQ(SECSuccess, PK11_ResetToken(slot.get(), nullptr));
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, nullptr));
}

TEST_F(SoftokenTest, ResetSoftokenNonEmptyPassword) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  EXPECT_EQ(SECSuccess, PK11_ResetToken(slot.get(), nullptr));
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password2"));
}

}  // namespace nss_test

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
