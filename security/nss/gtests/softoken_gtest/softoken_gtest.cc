#include "cert.h"
#include "certdb.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"
#include "secmod.h"
#include "secerr.h"

#include "nss_scoped_ptrs.h"
#include "util.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "databuffer.h"
#include <fstream>
#include <chrono>
using namespace std::chrono;

#include "softoken_dh_vectors.h"

namespace nss_test {
class SoftokenTest : public ::testing::Test {
 protected:
  SoftokenTest() : mNSSDBDir("SoftokenTest.d-") {}
  SoftokenTest(const std::string &prefix) : mNSSDBDir(prefix) {}

  virtual void SetUp() {
    std::string nssInitArg("sql:");
    nssInitArg.append(mNSSDBDir.GetUTF8Path());
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

TEST_F(SoftokenTest, GetInvalidAttribute) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  ASSERT_NE(nullptr, obj);
  SECItem out = {siBuffer, nullptr, 0};
  SECStatus rv = PK11_ReadRawAttribute(PK11_TypeGeneric, obj.get(),
                                       CKA_ALLOWED_MECHANISMS, &out);
  EXPECT_EQ(SECFailure, rv);
  // CKR_ATTRIBUTE_TYPE_INVALID maps to SEC_ERROR_BAD_DATA.
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
}

TEST_F(SoftokenTest, GetValidAttributes) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  ASSERT_NE(nullptr, obj);

  CK_ATTRIBUTE template_attrs[] = {
      {CKA_LABEL, NULL, 0},
      {CKA_CERT_SHA1_HASH, NULL, 0},
      {CKA_ISSUER, NULL, 0},
  };
  SECStatus rv =
      PK11_ReadRawAttributes(nullptr, PK11_TypeGeneric, obj.get(),
                             template_attrs, PR_ARRAY_SIZE(template_attrs));
  EXPECT_EQ(SECSuccess, rv);
  ASSERT_EQ(attributes[4].ulValueLen, template_attrs[0].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[4].pValue, template_attrs[0].pValue,
                      template_attrs[0].ulValueLen));
  ASSERT_EQ(attributes[5].ulValueLen, template_attrs[1].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[5].pValue, template_attrs[1].pValue,
                      template_attrs[1].ulValueLen));
  ASSERT_EQ(attributes[7].ulValueLen, template_attrs[2].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[7].pValue, template_attrs[2].pValue,
                      template_attrs[2].ulValueLen));
  for (unsigned int i = 0; i < PR_ARRAY_SIZE(template_attrs); i++) {
    PORT_Free(template_attrs[i].pValue);
  }
}

TEST_F(SoftokenTest, GetOnlyInvalidAttributes) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  ASSERT_NE(nullptr, obj);

  // Provide buffers of sufficient size, so that token
  // will write the data. This is annoying, but PK11_GetAttributes
  // won't allocate in the cases below when a single attribute
  // is missing. So, just put them all on the stack.
  unsigned char buf1[100];
  unsigned char buf2[100];
  CK_ATTRIBUTE template_attrs[] = {{0xffffffffUL, buf1, sizeof(buf1)},
                                   {0xfffffffeUL, buf2, sizeof(buf2)}};
  SECStatus rv =
      PK11_ReadRawAttributes(nullptr, PK11_TypeGeneric, obj.get(),
                             template_attrs, PR_ARRAY_SIZE(template_attrs));
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());

  // MSVC rewards -1UL with a C4146 warning...
  ASSERT_EQ(0UL, template_attrs[0].ulValueLen + 1);
  ASSERT_EQ(0UL, template_attrs[1].ulValueLen + 1);
}

TEST_F(SoftokenTest, GetAttributesInvalidInterspersed1) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  ASSERT_NE(nullptr, obj);

  unsigned char buf1[100];
  unsigned char buf2[100];
  unsigned char buf3[200];
  CK_ATTRIBUTE template_attrs[] = {{0xffffffff, buf1, sizeof(buf1)},
                                   {CKA_CERT_SHA1_HASH, buf2, sizeof(buf2)},
                                   {CKA_ISSUER, buf3, sizeof(buf3)}};
  SECStatus rv =
      PK11_ReadRawAttributes(nullptr, PK11_TypeGeneric, obj.get(),
                             template_attrs, PR_ARRAY_SIZE(template_attrs));
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
  ASSERT_EQ(0UL, template_attrs[0].ulValueLen + 1);
  ASSERT_EQ(attributes[5].ulValueLen, template_attrs[1].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[5].pValue, template_attrs[1].pValue,
                      template_attrs[1].ulValueLen));
  ASSERT_EQ(attributes[7].ulValueLen, template_attrs[2].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[7].pValue, template_attrs[2].pValue,
                      template_attrs[2].ulValueLen));
}

TEST_F(SoftokenTest, GetAttributesInvalidInterspersed2) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  ASSERT_NE(nullptr, obj);

  unsigned char buf1[100];
  unsigned char buf2[100];
  unsigned char buf3[100];
  CK_ATTRIBUTE template_attrs[] = {{CKA_LABEL, buf1, sizeof(buf1)},
                                   {CKA_CERT_SHA1_HASH, buf2, sizeof(buf2)},
                                   {0xffffffffUL, buf3, sizeof(buf3)}};
  SECStatus rv =
      PK11_ReadRawAttributes(nullptr, PK11_TypeGeneric, obj.get(),
                             template_attrs, PR_ARRAY_SIZE(template_attrs));
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
  ASSERT_EQ(attributes[4].ulValueLen, template_attrs[0].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[4].pValue, template_attrs[0].pValue,
                      template_attrs[0].ulValueLen));
  ASSERT_EQ(attributes[5].ulValueLen, template_attrs[1].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[5].pValue, template_attrs[1].pValue,
                      template_attrs[1].ulValueLen));
  ASSERT_EQ(0UL, template_attrs[2].ulValueLen + 1);
}

TEST_F(SoftokenTest, GetAttributesInvalidInterspersed3) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, "password"));
  ScopedPK11GenericObject obj(PK11_CreateGenericObject(
      slot.get(), attributes, PR_ARRAY_SIZE(attributes), true));
  ASSERT_NE(nullptr, obj);

  unsigned char buf1[100];
  unsigned char buf2[100];
  unsigned char buf3[100];
  unsigned char buf4[100];
  unsigned char buf5[100];
  unsigned char buf6[200];
  CK_ATTRIBUTE template_attrs[6] = {{CKA_LABEL, buf1, sizeof(buf1)},
                                    {0xffffffffUL, buf2, sizeof(buf2)},
                                    {0xfffffffeUL, buf3, sizeof(buf3)},
                                    {CKA_CERT_SHA1_HASH, buf4, sizeof(buf4)},
                                    {0xfffffffdUL, buf5, sizeof(buf5)},
                                    {CKA_ISSUER, buf6, sizeof(buf6)}};
  SECStatus rv =
      PK11_ReadRawAttributes(nullptr, PK11_TypeGeneric, obj.get(),
                             template_attrs, PR_ARRAY_SIZE(template_attrs));
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());

  ASSERT_EQ(attributes[4].ulValueLen, template_attrs[0].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[4].pValue, template_attrs[0].pValue,
                      template_attrs[0].ulValueLen));
  ASSERT_EQ(0UL, template_attrs[1].ulValueLen + 1);
  ASSERT_EQ(0UL, template_attrs[2].ulValueLen + 1);
  ASSERT_EQ(attributes[5].ulValueLen, template_attrs[3].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[5].pValue, template_attrs[3].pValue,
                      template_attrs[3].ulValueLen));
  ASSERT_EQ(0UL, template_attrs[4].ulValueLen + 1);
  ASSERT_EQ(attributes[7].ulValueLen, template_attrs[5].ulValueLen);
  EXPECT_EQ(0, memcmp(attributes[7].pValue, template_attrs[5].pValue,
                      template_attrs[5].ulValueLen));
}

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

// The size limit for a password is 500 characters as defined in pkcs11i.h
TEST_F(SoftokenTest, CreateObjectChangeToBigPassword) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, nullptr));
  EXPECT_EQ(
      SECSuccess,
      PK11_ChangePW(slot.get(), "",
                    "rUIFIFr2bxKnbJbitsfkyqttpk6vCJzlYMNxcxXcaN37gSZKbLk763X7iR"
                    "yeVNWZHQ02lSF69HYjzTyPW3318ZD0DBFMMbALZ8ZPZP73CIo5uIQlaowV"
                    "IbP8eOhRYtGUqoLGlcIFNEYogV8Q3GN58VeBMs0KxrIOvPQ9s8SnYYkqvt"
                    "zzgntmAvCgvk64x6eQf0okHwegd5wi6m0WVJytEepWXkP9J629FSa5kNT8"
                    "FvL3jvslkiImzTNuTvl32fQDXXMSc8vVk5Q3mH7trMZM0VDdwHWYERjHbz"
                    "kGxFgp0VhediHx7p9kkz6H6ac4et9sW4UkTnN7xhYc1Zr17wRSk2heQtcX"
                    "oZJGwuzhiKm8A8wkuVxms6zO56P4JORIk8oaUW6lyNTLo2kWWnTA"));
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

// We should be able to read CRLF, LF and CR.
// During the Initialization of the NSS Database, is called a function to load
// PKCS11 modules defined in pkcs11.txt. This file is read to get the
// specifications, parse them and load the modules. Here we are ensuring that
// the parsing will work correctly, independent of the breaking line format of
// pkcs11.txt file, which could vary depending where it was created.
// If the parsing is not well interpreted, the database cannot initialize.
TEST_F(SoftokenTest, CreateObjectReadBreakLine) {
  const std::string path = mNSSDBDir.GetPath();
  const std::string dbname_in = path + "/pkcs11.txt";
  const std::string dbname_out_cr = path + "/pkcs11_cr.txt";
  const std::string dbname_out_crlf = path + "/pkcs11_crlf.txt";
  const std::string dbname_out_lf = path + "/pkcs11_lf.txt";

  std::ifstream in(dbname_in);
  ASSERT_TRUE(in);
  std::ofstream out_cr(dbname_out_cr);
  ASSERT_TRUE(out_cr);
  std::ofstream out_crlf(dbname_out_crlf);
  ASSERT_TRUE(out_crlf);
  std::ofstream out_lf(dbname_out_lf);
  ASSERT_TRUE(out_lf);

  // Database should be correctly initialized by Setup()
  ASSERT_TRUE(NSS_IsInitialized());
  ASSERT_EQ(SECSuccess, NSS_Shutdown());

  // Prepare the file formats with CR, CRLF and LF
  for (std::string line; getline(in, line);) {
    out_cr << line << "\r";
    out_crlf << line << "\r\n";
    out_lf << line << "\n";
  }
  in.close();
  out_cr.close();
  out_crlf.close();
  out_lf.close();

  // Change the pkcs11.txt to CR format.
  ASSERT_TRUE(!remove(dbname_in.c_str()));
  ASSERT_TRUE(!rename(dbname_out_cr.c_str(), dbname_in.c_str()));

  // Try to initialize with CR format.
  std::string nssInitArg("sql:");
  nssInitArg.append(mNSSDBDir.GetUTF8Path());
  ASSERT_EQ(SECSuccess, NSS_Initialize(nssInitArg.c_str(), "", "", SECMOD_DB,
                                       NSS_INIT_NOROOTINIT));
  ASSERT_TRUE(NSS_IsInitialized());
  ASSERT_EQ(SECSuccess, NSS_Shutdown());

  // Change the pkcs11.txt to CRLF format.
  ASSERT_TRUE(!remove(dbname_in.c_str()));
  ASSERT_TRUE(!rename(dbname_out_crlf.c_str(), dbname_in.c_str()));

  // Try to initialize with CRLF format.
  ASSERT_EQ(SECSuccess, NSS_Initialize(nssInitArg.c_str(), "", "", SECMOD_DB,
                                       NSS_INIT_NOROOTINIT));
  ASSERT_TRUE(NSS_IsInitialized());
  ASSERT_EQ(SECSuccess, NSS_Shutdown());

  // Change the pkcs11.txt to LF format.
  ASSERT_TRUE(!remove(dbname_in.c_str()));
  ASSERT_TRUE(!rename(dbname_out_lf.c_str(), dbname_in.c_str()));

  // Try to initialize with LF format.
  ASSERT_EQ(SECSuccess, NSS_Initialize(nssInitArg.c_str(), "", "", SECMOD_DB,
                                       NSS_INIT_NOROOTINIT));
  ASSERT_TRUE(NSS_IsInitialized());
}

class SoftokenNonAsciiTest : public SoftokenTest {
 protected:
  SoftokenNonAsciiTest() : SoftokenTest("SoftokenTest.\xF7-") {}
};

TEST_F(SoftokenNonAsciiTest, NonAsciiPathWorking) {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, nullptr));
  EXPECT_EQ(SECSuccess, PK11_ResetToken(slot.get(), nullptr));
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, nullptr));
}

// This is just any X509 certificate. Its contents don't matter.
static unsigned char certDER[] = {
    0x30, 0x82, 0x01, 0xEF, 0x30, 0x82, 0x01, 0x94, 0xA0, 0x03, 0x02, 0x01,
    0x02, 0x02, 0x14, 0x49, 0xC4, 0xC4, 0x4A, 0xB6, 0x86, 0x07, 0xA3, 0x06,
    0xDC, 0x4D, 0xC8, 0xC3, 0xFE, 0xC7, 0x21, 0x3A, 0x2D, 0xE4, 0xDA, 0x30,
    0x0B, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B,
    0x30, 0x0F, 0x31, 0x0D, 0x30, 0x0B, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C,
    0x04, 0x74, 0x65, 0x73, 0x74, 0x30, 0x22, 0x18, 0x0F, 0x32, 0x30, 0x31,
    0x35, 0x31, 0x31, 0x32, 0x38, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5A,
    0x18, 0x0F, 0x32, 0x30, 0x31, 0x38, 0x30, 0x32, 0x30, 0x35, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x5A, 0x30, 0x0F, 0x31, 0x0D, 0x30, 0x0B, 0x06,
    0x03, 0x55, 0x04, 0x03, 0x0C, 0x04, 0x74, 0x65, 0x73, 0x74, 0x30, 0x82,
    0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D,
    0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00, 0x30, 0x82,
    0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xBA, 0x88, 0x51, 0xA8, 0x44,
    0x8E, 0x16, 0xD6, 0x41, 0xFD, 0x6E, 0xB6, 0x88, 0x06, 0x36, 0x10, 0x3D,
    0x3C, 0x13, 0xD9, 0xEA, 0xE4, 0x35, 0x4A, 0xB4, 0xEC, 0xF5, 0x68, 0x57,
    0x6C, 0x24, 0x7B, 0xC1, 0xC7, 0x25, 0xA8, 0xE0, 0xD8, 0x1F, 0xBD, 0xB1,
    0x9C, 0x06, 0x9B, 0x6E, 0x1A, 0x86, 0xF2, 0x6B, 0xE2, 0xAF, 0x5A, 0x75,
    0x6B, 0x6A, 0x64, 0x71, 0x08, 0x7A, 0xA5, 0x5A, 0xA7, 0x45, 0x87, 0xF7,
    0x1C, 0xD5, 0x24, 0x9C, 0x02, 0x7E, 0xCD, 0x43, 0xFC, 0x1E, 0x69, 0xD0,
    0x38, 0x20, 0x29, 0x93, 0xAB, 0x20, 0xC3, 0x49, 0xE4, 0xDB, 0xB9, 0x4C,
    0xC2, 0x6B, 0x6C, 0x0E, 0xED, 0x15, 0x82, 0x0F, 0xF1, 0x7E, 0xAD, 0x69,
    0x1A, 0xB1, 0xD3, 0x02, 0x3A, 0x8B, 0x2A, 0x41, 0xEE, 0xA7, 0x70, 0xE0,
    0x0F, 0x0D, 0x8D, 0xFD, 0x66, 0x0B, 0x2B, 0xB0, 0x24, 0x92, 0xA4, 0x7D,
    0xB9, 0x88, 0x61, 0x79, 0x90, 0xB1, 0x57, 0x90, 0x3D, 0xD2, 0x3B, 0xC5,
    0xE0, 0xB8, 0x48, 0x1F, 0xA8, 0x37, 0xD3, 0x88, 0x43, 0xEF, 0x27, 0x16,
    0xD8, 0x55, 0xB7, 0x66, 0x5A, 0xAA, 0x7E, 0x02, 0x90, 0x2F, 0x3A, 0x7B,
    0x10, 0x80, 0x06, 0x24, 0xCC, 0x1C, 0x6C, 0x97, 0xAD, 0x96, 0x61, 0x5B,
    0xB7, 0xE2, 0x96, 0x12, 0xC0, 0x75, 0x31, 0xA3, 0x0C, 0x91, 0xDD, 0xB4,
    0xCA, 0xF7, 0xFC, 0xAD, 0x1D, 0x25, 0xD3, 0x09, 0xEF, 0xB9, 0x17, 0x0E,
    0xA7, 0x68, 0xE1, 0xB3, 0x7B, 0x2F, 0x22, 0x6F, 0x69, 0xE3, 0xB4, 0x8A,
    0x95, 0x61, 0x1D, 0xEE, 0x26, 0xD6, 0x25, 0x9D, 0xAB, 0x91, 0x08, 0x4E,
    0x36, 0xCB, 0x1C, 0x24, 0x04, 0x2C, 0xBF, 0x16, 0x8B, 0x2F, 0xE5, 0xF1,
    0x8F, 0x99, 0x17, 0x31, 0xB8, 0xB3, 0xFE, 0x49, 0x23, 0xFA, 0x72, 0x51,
    0xC4, 0x31, 0xD5, 0x03, 0xAC, 0xDA, 0x18, 0x0A, 0x35, 0xED, 0x8D, 0x02,
    0x03, 0x01, 0x00, 0x01, 0x30, 0x0B, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
    0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x20,
    0x5C, 0x75, 0x51, 0x9F, 0x13, 0x11, 0x50, 0xCD, 0x5D, 0x8A, 0xDE, 0x20,
    0xA3, 0xBC, 0x06, 0x30, 0x91, 0xFF, 0xB2, 0x73, 0x75, 0x5F, 0x31, 0x64,
    0xEC, 0xFD, 0xCB, 0x42, 0x80, 0x0A, 0x70, 0xE6, 0x02, 0x21, 0x00, 0x82,
    0x12, 0xF7, 0xE5, 0xEA, 0x40, 0x27, 0xFD, 0xF7, 0xC0, 0x0E, 0x25, 0xF3,
    0x3E, 0x34, 0x95, 0x80, 0xB9, 0xA3, 0x38, 0xE0, 0x56, 0x68, 0xDA, 0xE5,
    0xC1, 0xF5, 0x37, 0xC7, 0xB5, 0xCE, 0x0D};

struct PasswordPair {
  const char *mInitialPassword;
  const char *mSecondPassword;
};

class SoftokenPasswordChangeTest
    : public SoftokenTest,
      public ::testing::WithParamInterface<PasswordPair> {};

TEST_P(SoftokenPasswordChangeTest, KeepTrustAfterPasswordChange) {
  const PasswordPair &passwords = GetParam();
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);
  // Set a password.
  EXPECT_EQ(SECSuccess,
            PK11_InitPin(slot.get(), nullptr, passwords.mInitialPassword));
  SECItem certDERItem = {siBuffer, certDER, sizeof(certDER)};
  // Import a certificate.
  ScopedCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &certDERItem, nullptr, true, true));
  EXPECT_TRUE(cert);
  SECStatus result =
      PK11_ImportCert(slot.get(), cert.get(), CK_INVALID_HANDLE, "test", false);
  EXPECT_EQ(SECSuccess, result);
  // Set a trust value.
  CERTCertTrust trust = {CERTDB_TRUSTED_CLIENT_CA | CERTDB_NS_TRUSTED_CA |
                             CERTDB_TRUSTED_CA | CERTDB_VALID_CA,
                         0, 0};
  result = CERT_ChangeCertTrust(nullptr, cert.get(), &trust);
  EXPECT_EQ(SECSuccess, result);
  // Release the certificate to ensure we get it from the DB rather than an
  // in-memory cache, below.
  cert = nullptr;
  // Change the password.
  result = PK11_ChangePW(slot.get(), passwords.mInitialPassword,
                         passwords.mSecondPassword);
  EXPECT_EQ(SECSuccess, result);
  // Look up the certificate again.
  ScopedCERTCertificate newCert(
      PK11_FindCertFromDERCertItem(slot.get(), &certDERItem, nullptr));
  EXPECT_TRUE(newCert.get());
  // The trust should be the same as before.
  CERTCertTrust newTrust = {0, 0, 0};
  result = CERT_GetCertTrust(newCert.get(), &newTrust);
  EXPECT_EQ(SECSuccess, result);
  EXPECT_EQ(trust.sslFlags, newTrust.sslFlags);
  EXPECT_EQ(trust.emailFlags, newTrust.emailFlags);
  EXPECT_EQ(trust.objectSigningFlags, newTrust.objectSigningFlags);
}

static const PasswordPair PASSWORD_CHANGE_TESTS[] = {
    {"password", ""},           // non-empty to empty password
    {"", "password"},           // empty to non-empty password
    {"password", "password2"},  // non-empty to non-empty password
};

INSTANTIATE_TEST_SUITE_P(SoftokenPasswordChangeTests,
                         SoftokenPasswordChangeTest,
                         ::testing::ValuesIn(PASSWORD_CHANGE_TESTS));

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

SECStatus test_dh_value(const PQGParams *params, const SECItem *pub_key_value,
                        PRBool genFailOK, time_t *time) {
  SECKEYDHParams dh_params;
  dh_params.base = params->base;
  dh_params.prime = params->prime;

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  EXPECT_TRUE(slot);
  if (!slot) return SECFailure;

  /* create a private/public key pair in with the given params */
  SECKEYPublicKey *pub_tmp = nullptr;
  ScopedSECKEYPrivateKey priv_key(
      PK11_GenerateKeyPair(slot.get(), CKM_DH_PKCS_KEY_PAIR_GEN, &dh_params,
                           &pub_tmp, PR_FALSE, PR_TRUE, nullptr));
  if ((genFailOK) && ((priv_key.get() == nullptr) || (pub_tmp == nullptr))) {
    return SECFailure;
  }
  EXPECT_NE(nullptr, priv_key.get()) << "PK11_GenerateKeyPair failed: "
                                     << PORT_ErrorToName(PORT_GetError());
  EXPECT_NE(nullptr, pub_tmp);
  if ((priv_key.get() == nullptr) || (pub_tmp == nullptr)) return SECFailure;
  ScopedSECKEYPublicKey pub_key(pub_tmp);
  ScopedSECKEYPublicKey peer_pub_key_manager(nullptr);
  SECKEYPublicKey *peer_pub_key = pub_key.get();

  /* if a subprime has been given set it on the PKCS #11 key */
  if (params->subPrime.data != nullptr) {
    SECStatus rv;
    EXPECT_EQ(SECSuccess, rv = PK11_WriteRawAttribute(
                              PK11_TypePrivKey, priv_key.get(), CKA_SUBPRIME,
                              (SECItem *)&params->subPrime))
        << "PK11_WriteRawAttribute failed: "
        << PORT_ErrorToString(PORT_GetError());
    if (rv != SECSuccess) {
      return rv;
    }
  }

  /* find if we weren't passed a public value in, use the
   * one we just generated */
  if (pub_key_value && pub_key_value->data) {
    peer_pub_key = SECKEY_CopyPublicKey(pub_key.get());
    EXPECT_NE(nullptr, peer_pub_key);
    if (peer_pub_key == nullptr) {
      return SECFailure;
    }
    peer_pub_key->u.dh.publicValue = *pub_key_value;
    peer_pub_key_manager.reset(peer_pub_key);
  }

  /* now do the derive. time it and return the time if
   * the caller requested it. */
  auto start = high_resolution_clock::now();
  ScopedPK11SymKey derivedKey(PK11_PubDerive(
      priv_key.get(), peer_pub_key, PR_FALSE, nullptr, nullptr,
      CKM_DH_PKCS_DERIVE, CKM_HKDF_DERIVE, CKA_DERIVE, 32, nullptr));
  auto stop = high_resolution_clock::now();
  if (!derivedKey) {
    std::cerr << "PK11_PubDerive failed: "
              << PORT_ErrorToString(PORT_GetError()) << std::endl;
  }

  if (time) {
    auto duration = duration_cast<microseconds>(stop - start);
    *time = duration.count();
  }
  return derivedKey ? SECSuccess : SECFailure;
}

class SoftokenDhTest : public SoftokenTest {
 protected:
  SoftokenDhTest() : SoftokenTest("SoftokenDhTest.d-") {}
#ifdef NSS_USE_TIMING_CODE
  time_t reference_time[CLASS_LAST] = {0};
#endif

  virtual void SetUp() {
    SoftokenTest::SetUp();

#ifdef NSS_USE_TIMING_CODE
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ASSERT_TRUE(slot);

    time_t time;
    for (int i = CLASS_FIRST; i < CLASS_LAST; i++) {
      PQGParams params;
      params.prime.data = (unsigned char *)reference_prime[i];
      params.prime.len = reference_prime_len[i];
      params.base.data = (unsigned char *)g2;
      params.base.len = sizeof(g2);
      params.subPrime.data = nullptr;
      params.subPrime.len = 0;
      ASSERT_EQ(SECSuccess, test_dh_value(&params, nullptr, PR_FALSE, &time));
      reference_time[i] = time / 2 + 3 * time;
    }
#endif
  };
};

const char *param_value(DhParamType param_type) {
  switch (param_type) {
    case TLS_APPROVED:
      return "TLS_APPROVED";
    case IKE_APPROVED:
      return "IKE_APPROVED";
    case SAFE_PRIME:
      return "SAFE_PRIME";
    case SAFE_PRIME_WITH_SUBPRIME:
      return "SAFE_PRIME_WITH_SUBPRIME";
    case KNOWN_SUBPRIME:
      return "KNOWN_SUBPRIME";
    case UNKNOWN_SUBPRIME:
      return "UNKNOWN_SUBPRIME";
    case WRONG_SUBPRIME:
      return "WRONG_SUBPRIME";
    case BAD_PUB_KEY:
      return "BAD_PUB_KEY";
  }
  return "**Invalid**";
}

const char *key_value(DhKeyClass key_class) {
  switch (key_class) {
    case CLASS_1536:
      return "CLASS_1536";
    case CLASS_2048:
      return "CLASS_2048";
    case CLASS_3072:
      return "CLASS_3072";
    case CLASS_4096:
      return "CLASS_4096";
    case CLASS_6144:
      return "CLASS_6144";
    case CLASS_8192:
      return "CLASS_8192";
    case CLASS_LAST:
      break;
  }
  return "**Invalid**";
}

class SoftokenDhValidate : public SoftokenDhTest,
                           public ::testing::WithParamInterface<DhTestVector> {
};

/* test the DH validation process. In non-fips mode, only BAD_PUB_KEY tests
 * should fail */
TEST_P(SoftokenDhValidate, DhVectors) {
  const DhTestVector dhTestValues = GetParam();
  std::string testId = (char *)(dhTestValues.id);
  std::string err = "Test(" + testId + ") failed";
  SECStatus rv;
  time_t time;

  PQGParams params;
  params.prime = dhTestValues.p;
  params.base = dhTestValues.g;
  params.subPrime = dhTestValues.q;

  std::cerr << "Test: " + testId << std::endl
            << "param_type: " << param_value(dhTestValues.param_type)
            << ", key_class: " << key_value(dhTestValues.key_class) << std::endl
            << "p: " << DataBuffer(dhTestValues.p.data, dhTestValues.p.len)
            << std::endl
            << "g: " << DataBuffer(dhTestValues.g.data, dhTestValues.g.len)
            << std::endl
            << "q: " << DataBuffer(dhTestValues.q.data, dhTestValues.q.len)
            << std::endl
            << "pub_key: "
            << DataBuffer(dhTestValues.pub_key.data, dhTestValues.pub_key.len)
            << std::endl;
  rv = test_dh_value(&params, &dhTestValues.pub_key, PR_FALSE, &time);

  switch (dhTestValues.param_type) {
    case TLS_APPROVED:
    case IKE_APPROVED:
    case SAFE_PRIME:
    case UNKNOWN_SUBPRIME:
      EXPECT_EQ(SECSuccess, rv) << err;
#ifdef NSS_USE_TIMING_CODE
      EXPECT_LE(time, reference_time[dhTestValues.key_class]) << err;
#endif
      break;
    case KNOWN_SUBPRIME:
    case SAFE_PRIME_WITH_SUBPRIME:
      EXPECT_EQ(SECSuccess, rv) << err;
#ifdef NSS_USE_TIMING_CODE
      EXPECT_GT(time, reference_time[dhTestValues.key_class]) << err;
#endif
      break;
    case WRONG_SUBPRIME:
    case BAD_PUB_KEY:
      EXPECT_EQ(SECFailure, rv) << err;
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(DhValidateCases, SoftokenDhValidate,
                         ::testing::ValuesIn(DH_TEST_VECTORS));

#ifndef NSS_FIPS_DISABLED

class SoftokenFipsTest : public SoftokenTest {
 protected:
  SoftokenFipsTest() : SoftokenTest("SoftokenFipsTest.d-") {}
  SoftokenFipsTest(const std::string &prefix) : SoftokenTest(prefix) {}

  virtual void SetUp() {
    SoftokenTest::SetUp();

    // Turn on FIPS mode (code borrowed from FipsMode in modutil/pk11.c)
    char *internal_name;
    ASSERT_FALSE(PK11_IsFIPS());
    internal_name = PR_smprintf("%s", SECMOD_GetInternalModule()->commonName);
    ASSERT_EQ(SECSuccess, SECMOD_DeleteInternalModule(internal_name))
        << PORT_ErrorToName(PORT_GetError());
    PR_smprintf_free(internal_name);
    ASSERT_TRUE(PK11_IsFIPS());
  }
};

class SoftokenFipsDhTest : public SoftokenFipsTest {
 protected:
  SoftokenFipsDhTest() : SoftokenFipsTest("SoftokenFipsDhTest.d-") {}
#ifdef NSS_USE_TIMING_CODE
  time_t reference_time[CLASS_LAST] = {0};
#endif

  virtual void SetUp() {
    SoftokenFipsTest::SetUp();

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ASSERT_TRUE(slot);

    ASSERT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, ""));
    ASSERT_EQ(SECSuccess, PK11_Authenticate(slot.get(), PR_FALSE, nullptr));

#ifdef NSS_USE_TIMING_CODE
    time_t time;
    for (int i = CLASS_FIRST; i < CLASS_LAST; i++) {
      PQGParams params;
      params.prime.data = (unsigned char *)reference_prime[i];
      params.prime.len = reference_prime_len[i];
      params.base.data = (unsigned char *)g2;
      params.base.len = sizeof(g2);
      params.subPrime.data = nullptr;
      params.subPrime.len = 0;
      ASSERT_EQ(SECSuccess, test_dh_value(&params, nullptr, PR_FALSE, &time));
      reference_time[i] = time / 2 + 3 * time;
    }
#endif
  };
};

const std::vector<std::string> kFipsPasswordCases[] = {
    // FIPS level1 -> level1 -> level1
    {"", "", ""},
    // FIPS level1 -> level1 -> level2
    {"", "", "strong-_123"},
    // FIXME: this should work: FIPS level1 -> level2 -> level2
    // {"", "strong-_123", "strong-_456"},
    // FIPS level2 -> level2 -> level2
    {"strong-_123", "strong-_456", "strong-_123"}};

const std::vector<std::string> kFipsPasswordBadCases[] = {
    // FIPS level1 -> level2 -> level1
    {"", "strong-_123", ""},
    // FIPS level2 -> level1 -> level1
    {"strong-_123", ""},
    // FIPS level2 -> level2 -> level1
    {"strong-_123", "strong-_456", ""},
    // initialize with a weak password
    {"weak"},
    // FIPS level1 -> weak password
    {"", "weak"},
    // FIPS level2 -> weak password
    {"strong-_123", "weak"}};

class SoftokenFipsPasswordTest
    : public SoftokenFipsTest,
      public ::testing::WithParamInterface<std::vector<std::string>> {};

class SoftokenFipsBadPasswordTest
    : public SoftokenFipsTest,
      public ::testing::WithParamInterface<std::vector<std::string>> {};

TEST_P(SoftokenFipsPasswordTest, SetPassword) {
  const std::vector<std::string> &passwords = GetParam();
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);

  auto it = passwords.begin();
  auto prev_it = it;
  EXPECT_EQ(SECSuccess, PK11_InitPin(slot.get(), nullptr, (*it).c_str()));
  for (it++; it != passwords.end(); it++, prev_it++) {
    EXPECT_EQ(SECSuccess,
              PK11_ChangePW(slot.get(), (*prev_it).c_str(), (*it).c_str()));
  }
}

TEST_P(SoftokenFipsBadPasswordTest, SetBadPassword) {
  const std::vector<std::string> &passwords = GetParam();
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);

  auto it = passwords.begin();
  auto prev_it = it;
  SECStatus rv = PK11_InitPin(slot.get(), nullptr, (*it).c_str());
  if (it + 1 == passwords.end())
    EXPECT_EQ(SECFailure, rv);
  else
    EXPECT_EQ(SECSuccess, rv);
  for (it++; it != passwords.end(); it++, prev_it++) {
    rv = PK11_ChangePW(slot.get(), (*prev_it).c_str(), (*it).c_str());
    if (it + 1 == passwords.end())
      EXPECT_EQ(SECFailure, rv);
    else
      EXPECT_EQ(SECSuccess, rv);
  }
}

class SoftokenFipsDhValidate
    : public SoftokenFipsDhTest,
      public ::testing::WithParamInterface<DhTestVector> {};

/* test the DH validation process. In fips mode, primes with unknown
 * subprimes, and all sorts of bad public keys should fail */
TEST_P(SoftokenFipsDhValidate, DhVectors) {
  const DhTestVector dhTestValues = GetParam();
  std::string testId = (char *)(dhTestValues.id);
  std::string err = "Test(" + testId + ") failed";
  time_t time;
  PRBool genFailOK = PR_FALSE;
  SECStatus rv;

  PQGParams params;
  params.prime = dhTestValues.p;
  params.base = dhTestValues.g;
  params.subPrime = dhTestValues.q;
  std::cerr << "Test:" + testId << std::endl
            << "param_type: " << param_value(dhTestValues.param_type)
            << ", key_class: " << key_value(dhTestValues.key_class) << std::endl
            << "p: " << DataBuffer(dhTestValues.p.data, dhTestValues.p.len)
            << std::endl
            << "g: " << DataBuffer(dhTestValues.g.data, dhTestValues.g.len)
            << std::endl
            << "q: " << DataBuffer(dhTestValues.q.data, dhTestValues.q.len)
            << std::endl
            << "pub_key: "
            << DataBuffer(dhTestValues.pub_key.data, dhTestValues.pub_key.len)
            << std::endl;

  if ((dhTestValues.param_type != TLS_APPROVED) &&
      (dhTestValues.param_type != IKE_APPROVED)) {
    genFailOK = PR_TRUE;
  }
  rv = test_dh_value(&params, &dhTestValues.pub_key, genFailOK, &time);

  switch (dhTestValues.param_type) {
    case TLS_APPROVED:
    case IKE_APPROVED:
      EXPECT_EQ(SECSuccess, rv) << err;
#ifdef NSS_USE_TIMING_CODE
      EXPECT_LE(time, reference_time[dhTestValues.key_class]) << err;
#endif
      break;
    case SAFE_PRIME:
    case SAFE_PRIME_WITH_SUBPRIME:
    case KNOWN_SUBPRIME:
    case UNKNOWN_SUBPRIME:
    case WRONG_SUBPRIME:
    case BAD_PUB_KEY:
      EXPECT_EQ(SECFailure, rv) << err;
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(FipsPasswordCases, SoftokenFipsPasswordTest,
                         ::testing::ValuesIn(kFipsPasswordCases));

INSTANTIATE_TEST_SUITE_P(BadFipsPasswordCases, SoftokenFipsBadPasswordTest,
                         ::testing::ValuesIn(kFipsPasswordBadCases));

INSTANTIATE_TEST_SUITE_P(FipsDhCases, SoftokenFipsDhValidate,
                         ::testing::ValuesIn(DH_TEST_VECTORS));
#endif

}  // namespace nss_test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
