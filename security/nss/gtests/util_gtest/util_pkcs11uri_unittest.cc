/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include <memory>
#include "pkcs11uri.h"

#include "gtest/gtest.h"
#include "scoped_ptrs.h"

namespace nss_test {

class PK11URITest : public ::testing::Test {
 public:
  bool TestCreate(const PK11URIAttribute *pattrs, size_t num_pattrs,
                  const PK11URIAttribute *qattrs, size_t num_qattrs) {
    ScopedPK11URI tmp(
        PK11URI_CreateURI(pattrs, num_pattrs, qattrs, num_qattrs));
    return tmp != nullptr;
  }

  void TestCreateRetrieve(const PK11URIAttribute *pattrs, size_t num_pattrs,
                          const PK11URIAttribute *qattrs, size_t num_qattrs) {
    ScopedPK11URI tmp(
        PK11URI_CreateURI(pattrs, num_pattrs, qattrs, num_qattrs));
    ASSERT_TRUE(tmp);

    size_t i;
    for (i = 0; i < num_pattrs; i++) {
      const char *value = PK11URI_GetPathAttribute(tmp.get(), pattrs[i].name);
      EXPECT_TRUE(value);
      if (value) {
        EXPECT_EQ(std::string(value), std::string(pattrs[i].value));
      }
    }
    for (i = 0; i < num_qattrs; i++) {
      const char *value = PK11URI_GetQueryAttribute(tmp.get(), qattrs[i].name);
      EXPECT_TRUE(value);
      if (value) {
        EXPECT_EQ(std::string(value), std::string(qattrs[i].value));
      }
    }
  }

  void TestCreateFormat(const PK11URIAttribute *pattrs, size_t num_pattrs,
                        const PK11URIAttribute *qattrs, size_t num_qattrs,
                        const std::string &formatted) {
    ScopedPK11URI tmp(
        PK11URI_CreateURI(pattrs, num_pattrs, qattrs, num_qattrs));
    ASSERT_TRUE(tmp);
    char *out = PK11URI_FormatURI(nullptr, tmp.get());
    EXPECT_TRUE(out);
    if (out) {
      EXPECT_EQ(std::string(out), formatted);
    }
    PORT_Free(out);
  }

  bool TestParse(const std::string &str) {
    ScopedPK11URI tmp(PK11URI_ParseURI(str.c_str()));
    return tmp != nullptr;
  }

  void TestParseRetrieve(const std::string &str, const PK11URIAttribute *pattrs,
                         size_t num_pattrs, const PK11URIAttribute *qattrs,
                         size_t num_qattrs) {
    ScopedPK11URI tmp(PK11URI_ParseURI(str.c_str()));
    ASSERT_TRUE(tmp);

    size_t i;
    for (i = 0; i < num_pattrs; i++) {
      const char *value = PK11URI_GetPathAttribute(tmp.get(), pattrs[i].name);
      EXPECT_TRUE(value);
      if (value) {
        EXPECT_EQ(std::string(value), std::string(pattrs[i].value));
      }
    }
    for (i = 0; i < num_qattrs; i++) {
      const char *value = PK11URI_GetQueryAttribute(tmp.get(), qattrs[i].name);
      EXPECT_TRUE(value);
      if (value) {
        EXPECT_EQ(std::string(value), std::string(qattrs[i].value));
      }
    }
  }

  void TestParseFormat(const std::string &str, const std::string &formatted) {
    ScopedPK11URI tmp(PK11URI_ParseURI(str.c_str()));
    ASSERT_TRUE(tmp);
    char *out = PK11URI_FormatURI(nullptr, tmp.get());
    EXPECT_TRUE(out);
    if (out) {
      EXPECT_EQ(std::string(out), formatted);
      PORT_Free(out);
    }
  }

 protected:
};

const PK11URIAttribute pattrs[] = {
    {"token", "aaa"}, {"manufacturer", "bbb"}, {"vendor", "ccc"}};

const PK11URIAttribute qattrs[] = {{"pin-source", "|grep foo /etc/passwd"},
                                   {"pin-value", "secret"},
                                   {"vendor", "ddd"}};

const PK11URIAttribute pattrs_invalid[] = {{"token", "aaa"},
                                           {"manufacturer", "bbb"},
                                           {"vendor", "ccc"},
                                           {"$%*&", "invalid"},
                                           {"", "empty"}};

const PK11URIAttribute qattrs_invalid[] = {
    {"pin-source", "|grep foo /etc/passwd"},
    {"pin-value", "secret"},
    {"vendor", "ddd"},
    {"$%*&", "invalid"},
    {"", "empty"}};

TEST_F(PK11URITest, CreateTest) {
  EXPECT_TRUE(
      TestCreate(pattrs, PR_ARRAY_SIZE(pattrs), qattrs, PR_ARRAY_SIZE(qattrs)));
  EXPECT_FALSE(TestCreate(pattrs_invalid, PR_ARRAY_SIZE(pattrs_invalid), qattrs,
                          PR_ARRAY_SIZE(qattrs)));
  EXPECT_FALSE(TestCreate(pattrs, PR_ARRAY_SIZE(pattrs), qattrs_invalid,
                          PR_ARRAY_SIZE(qattrs_invalid)));
  EXPECT_FALSE(TestCreate(pattrs_invalid, PR_ARRAY_SIZE(pattrs_invalid),
                          qattrs_invalid, PR_ARRAY_SIZE(qattrs_invalid)));
}

TEST_F(PK11URITest, CreateRetrieveTest) {
  TestCreateRetrieve(pattrs, PR_ARRAY_SIZE(pattrs), qattrs,
                     PR_ARRAY_SIZE(qattrs));
}

TEST_F(PK11URITest, CreateFormatTest) {
  TestCreateFormat(pattrs, PR_ARRAY_SIZE(pattrs), qattrs, PR_ARRAY_SIZE(qattrs),
                   "pkcs11:token=aaa;manufacturer=bbb;vendor=ccc?pin-source=|"
                   "grep%20foo%20/etc/passwd&pin-value=secret&vendor=ddd");
}

TEST_F(PK11URITest, ParseTest) {
  EXPECT_FALSE(TestParse("pkcs11:token=aaa;token=bbb"));
  EXPECT_FALSE(TestParse("pkcs11:dup=aaa;dup=bbb"));
  EXPECT_FALSE(TestParse("pkcs11:?pin-value=aaa&pin-value=bbb"));
  EXPECT_FALSE(TestParse("pkcs11:=empty"));
  EXPECT_FALSE(TestParse("pkcs11:token=%2;manufacturer=aaa"));
}

TEST_F(PK11URITest, ParseRetrieveTest) {
  TestParseRetrieve(
      "pkcs11:token=aaa;manufacturer=bbb;vendor=ccc?pin-source=|"
      "grep%20foo%20/etc/passwd&pin-value=secret&vendor=ddd",
      pattrs, PR_ARRAY_SIZE(pattrs), qattrs, PR_ARRAY_SIZE(qattrs));
}

TEST_F(PK11URITest, ParseFormatTest) {
  TestParseFormat("pkcs11:", "pkcs11:");
  TestParseFormat("pkcs11:token=aaa", "pkcs11:token=aaa");
  TestParseFormat("pkcs11:token=aaa;manufacturer=bbb",
                  "pkcs11:token=aaa;manufacturer=bbb");
  TestParseFormat("pkcs11:manufacturer=bbb;token=aaa",
                  "pkcs11:token=aaa;manufacturer=bbb");
  TestParseFormat("pkcs11:manufacturer=bbb;token=aaa;vendor2=ddd;vendor1=ccc",
                  "pkcs11:token=aaa;manufacturer=bbb;vendor1=ccc;vendor2=ddd");
  TestParseFormat("pkcs11:?pin-value=secret", "pkcs11:?pin-value=secret");
  TestParseFormat("pkcs11:?dup=aaa&dup=bbb", "pkcs11:?dup=aaa&dup=bbb");
  TestParseFormat(
      "pkcs11:?pin-source=|grep%20foo%20/etc/passwd&pin-value=secret",
      "pkcs11:?pin-source=|grep%20foo%20/etc/passwd&pin-value=secret");
  TestParseFormat("pkcs11:token=aaa?pin-value=secret",
                  "pkcs11:token=aaa?pin-value=secret");
}

}  // namespace nss_test
