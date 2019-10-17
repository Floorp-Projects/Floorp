#include "cert.h"
#include "certdb.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"

#include "nss_scoped_ptrs.h"
#include "util.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

namespace nss_test {

class SoftokenBuiltinsTest : public ::testing::Test {
 protected:
  SoftokenBuiltinsTest() : nss_db_dir_("SoftokenBuiltinsTest.d-") {}
  SoftokenBuiltinsTest(const std::string &prefix) : nss_db_dir_(prefix) {}

  virtual void SetUp() {
    std::string nss_init_arg("sql:");
    nss_init_arg.append(nss_db_dir_.GetUTF8Path());
    ASSERT_EQ(SECSuccess, NSS_Initialize(nss_init_arg.c_str(), "", "",
                                         SECMOD_DB, NSS_INIT_NOROOTINIT));
  }

  virtual void TearDown() {
    ASSERT_EQ(SECSuccess, NSS_Shutdown());
    const std::string &nss_db_dir_path = nss_db_dir_.GetPath();
    ASSERT_EQ(0, unlink((nss_db_dir_path + "/cert9.db").c_str()));
    ASSERT_EQ(0, unlink((nss_db_dir_path + "/key4.db").c_str()));
    ASSERT_EQ(0, unlink((nss_db_dir_path + "/pkcs11.txt").c_str()));
  }

  virtual void LoadModule() {
    ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
    ASSERT_TRUE(slot);
    EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, nullptr));
    SECStatus result = SECMOD_AddNewModule(
        "Builtins-testlib", DLL_PREFIX "nssckbi-testlib." DLL_SUFFIX, 0, 0);
    ASSERT_EQ(result, SECSuccess);
  }

  ScopedUniqueDirectory nss_db_dir_;
};

// The next tests in this class are used to test the Distrust Fields.
// More details about these fields in lib/ckfw/builtins/README.
TEST_F(SoftokenBuiltinsTest, CheckNoDistrustFields) {
  const char *kCertNickname =
      "Builtin Object Token:Distrust Fields Test - no_distrust";
  LoadModule();

  CERTCertDBHandle *cert_handle = CERT_GetDefaultCertDB();
  ASSERT_TRUE(cert_handle);
  ScopedCERTCertificate cert(
      CERT_FindCertByNickname(cert_handle, kCertNickname));
  ASSERT_TRUE(cert);

  EXPECT_EQ(PR_FALSE,
            PK11_HasAttributeSet(cert->slot, cert->pkcs11ID,
                                 CKA_NSS_SERVER_DISTRUST_AFTER, PR_FALSE));
  EXPECT_EQ(PR_FALSE,
            PK11_HasAttributeSet(cert->slot, cert->pkcs11ID,
                                 CKA_NSS_EMAIL_DISTRUST_AFTER, PR_FALSE));
  ASSERT_FALSE(cert->distrust);
}

TEST_F(SoftokenBuiltinsTest, CheckOkDistrustFields) {
  const char *kCertNickname =
      "Builtin Object Token:Distrust Fields Test - ok_distrust";
  LoadModule();

  CERTCertDBHandle *cert_handle = CERT_GetDefaultCertDB();
  ASSERT_TRUE(cert_handle);
  ScopedCERTCertificate cert(
      CERT_FindCertByNickname(cert_handle, kCertNickname));
  ASSERT_TRUE(cert);

  const char *kExpectedDERValueServer = "200617000000Z";
  const char *kExpectedDERValueEmail = "071014085320Z";
  // When a valid timestamp is encoded, the result length is exactly 13.
  const unsigned int kDistrustFieldSize = 13;

  ASSERT_TRUE(cert->distrust);
  ASSERT_EQ(kDistrustFieldSize, cert->distrust->serverDistrustAfter.len);
  ASSERT_NE(nullptr, cert->distrust->serverDistrustAfter.data);
  EXPECT_TRUE(!memcmp(kExpectedDERValueServer,
                      cert->distrust->serverDistrustAfter.data,
                      kDistrustFieldSize));

  ASSERT_EQ(kDistrustFieldSize, cert->distrust->emailDistrustAfter.len);
  ASSERT_NE(nullptr, cert->distrust->emailDistrustAfter.data);
  EXPECT_TRUE(!memcmp(kExpectedDERValueEmail,
                      cert->distrust->emailDistrustAfter.data,
                      kDistrustFieldSize));
}

TEST_F(SoftokenBuiltinsTest, CheckInvalidDistrustFields) {
  const char *kCertNickname =
      "Builtin Object Token:Distrust Fields Test - err_distrust";
  LoadModule();

  CERTCertDBHandle *cert_handle = CERT_GetDefaultCertDB();
  ASSERT_TRUE(cert_handle);
  ScopedCERTCertificate cert(
      CERT_FindCertByNickname(cert_handle, kCertNickname));
  ASSERT_TRUE(cert);

  // The field should never be set to TRUE in production, we are just
  // testing if this field is readable, even if set to TRUE.
  EXPECT_EQ(PR_TRUE,
            PK11_HasAttributeSet(cert->slot, cert->pkcs11ID,
                                 CKA_NSS_SERVER_DISTRUST_AFTER, PR_FALSE));
  // If something other than CK_BBOOL CK_TRUE, it will be considered FALSE
  // Here, there is an OCTAL value, but with unexpected content (1 digit less).
  EXPECT_EQ(PR_FALSE,
            PK11_HasAttributeSet(cert->slot, cert->pkcs11ID,
                                 CKA_NSS_EMAIL_DISTRUST_AFTER, PR_FALSE));
  ASSERT_FALSE(cert->distrust);
}

}  // namespace nss_test
