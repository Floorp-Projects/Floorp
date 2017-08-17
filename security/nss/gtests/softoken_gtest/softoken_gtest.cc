#include <cstdlib>

#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"

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
  explicit ScopedUniqueDirectory(const std::string &prefix);

  // NB: the directory must be empty upon destruction
  ~ScopedUniqueDirectory() { assert(rmdir(mPath.c_str()) == 0); }

  const std::string &GetPath() { return mPath; }

 private:
  static const int RETRY_LIMIT = 5;
  static void GenerateRandomName(/*in/out*/ std::string &prefix);
  static bool TryMakingDirectory(/*in/out*/ std::string &prefix);

  std::string mPath;
};

ScopedUniqueDirectory::ScopedUniqueDirectory(const std::string &prefix) {
  std::string path;
  const char *workingDirectory = PR_GetEnvSecure("NSS_GTEST_WORKDIR");
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

void ScopedUniqueDirectory::GenerateRandomName(std::string &prefix) {
  std::stringstream ss;
  ss << prefix;
  // RAND_MAX is at least 32767.
  ss << std::setfill('0') << std::setw(4) << std::hex << rand() << rand();
  // This will overwrite the value of prefix. This is a little inefficient, but
  // at least it makes the code simple.
  ss >> prefix;
}

bool ScopedUniqueDirectory::TryMakingDirectory(std::string &prefix) {
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
    const std::string &nssDBDirPath = mNSSDBDir.GetPath();
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

// Test certificate to use in the CreateObject tests.
static const CK_OBJECT_CLASS cko_nss_trust = CKO_NSS_TRUST;
static const CK_BBOOL ck_false = CK_FALSE;
static const CK_BBOOL ck_true = CK_TRUE;
static const CK_TRUST ckt_nss_must_verify_trust = CKT_NSS_MUST_VERIFY_TRUST;
static const CK_TRUST ckt_nss_trusted_delegator = CKT_NSS_TRUSTED_DELEGATOR;
static const CK_ATTRIBUTE attributes[] = {
    {CKA_CLASS, (void *)&cko_nss_trust, (PRUint32)sizeof(CK_OBJECT_CLASS)},
    {CKA_TOKEN, (void *)&ck_true, (PRUint32)sizeof(CK_BBOOL)},
    {CKA_PRIVATE, (void *)&ck_false, (PRUint32)sizeof(CK_BBOOL)},
    {CKA_MODIFIABLE, (void *)&ck_false, (PRUint32)sizeof(CK_BBOOL)},
    {CKA_LABEL,
     (void *)"Symantec Class 2 Public Primary Certification Authority - G4",
     (PRUint32)61},
    {CKA_CERT_SHA1_HASH,
     (void *)"\147\044\220\056\110\001\260\042\226\100\020\106\264\261\147\054"
             "\251\165\375\053",
     (PRUint32)20},
    {CKA_CERT_MD5_HASH,
     (void *)"\160\325\060\361\332\224\227\324\327\164\337\276\355\150\336\226",
     (PRUint32)16},
    {CKA_ISSUER,
     (void *)"\060\201\224\061\013\060\011\006\003\125\004\006\023\002\125\123"
             "\061\035\060\033\006\003\125\004\012\023\024\123\171\155\141\156"
             "\164\145\143\040\103\157\162\160\157\162\141\164\151\157\156\061"
             "\037\060\035\006\003\125\004\013\023\026\123\171\155\141\156\164"
             "\145\143\040\124\162\165\163\164\040\116\145\164\167\157\162\153"
             "\061\105\060\103\006\003\125\004\003\023\074\123\171\155\141\156"
             "\164\145\143\040\103\154\141\163\163\040\062\040\120\165\142\154"
             "\151\143\040\120\162\151\155\141\162\171\040\103\145\162\164\151"
             "\146\151\143\141\164\151\157\156\040\101\165\164\150\157\162\151"
             "\164\171\040\055\040\107\064",
     (PRUint32)151},
    {CKA_SERIAL_NUMBER,
     (void *)"\002\020\064\027\145\022\100\073\267\126\200\055\200\313\171\125"
             "\246\036",
     (PRUint32)18},
    {CKA_TRUST_SERVER_AUTH, (void *)&ckt_nss_must_verify_trust,
     (PRUint32)sizeof(CK_TRUST)},
    {CKA_TRUST_EMAIL_PROTECTION, (void *)&ckt_nss_trusted_delegator,
     (PRUint32)sizeof(CK_TRUST)},
    {CKA_TRUST_CODE_SIGNING, (void *)&ckt_nss_must_verify_trust,
     (PRUint32)sizeof(CK_TRUST)},
    {CKA_TRUST_STEP_UP_APPROVED, (void *)&ck_false,
     (PRUint32)sizeof(CK_BBOOL)}};

TEST_F(SoftokenTest, CreateObjectNonEmptyPassword) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  EXPECT_EQ(SECSuccess, PK11_Logout(slot.get()));
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  EXPECT_EQ(nullptr, obj);
}

TEST_F(SoftokenTest, CreateObjectChangePassword) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, nullptr));
  EXPECT_EQ(SECSuccess, PK11_ChangePW(slot.get(), "", "password"));
  EXPECT_EQ(SECSuccess, PK11_Logout(slot.get()));
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  EXPECT_EQ(nullptr, obj);
}

TEST_F(SoftokenTest, CreateObjectChangeToEmptyPassword) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  EXPECT_EQ(SECSuccess, PK11_ChangePW(slot.get(), "password", ""));
  // PK11_Logout returnes an error and SEC_ERROR_TOKEN_NOT_LOGGED_IN if the user
  // is not "logged in".
  EXPECT_EQ(SECFailure, PK11_Logout(slot.get()));
  EXPECT_EQ(SEC_ERROR_TOKEN_NOT_LOGGED_IN, PORT_GetError());
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  // Because there's no password we can't logout and the operation should have
  // succeeded.
  EXPECT_NE(nullptr, obj);
}

class SoftokenNoDBTest : public ::testing::Test {};

TEST_F(SoftokenNoDBTest, NeedUserInitNoDB) {
  ASSERT_EQ(SECSuccess, NSS_NoDB_Init("."));
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(PR_FALSE, PK11_NeedUserInit(slot.get()));

  // When shutting down in here we have to release the slot first.
  slot = nullptr;
  ASSERT_EQ(SECSuccess, NSS_Shutdown());
}

}  // namespace nss_test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
