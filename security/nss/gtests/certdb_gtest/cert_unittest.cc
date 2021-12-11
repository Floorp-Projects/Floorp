/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nss.h"
#include "secerr.h"
#include "pk11pub.h"
#include "nss_scoped_ptrs.h"

namespace nss_test {

class CertTest : public ::testing::Test {};

// Tests CERT_GetCertificateDer for the certs we have.
TEST_F(CertTest, GetCertDer) {
  // Listing all the certs should get us the default trust anchors.
  ScopedCERTCertList certs(PK11_ListCerts(PK11CertListAll, nullptr));
  ASSERT_FALSE(PR_CLIST_IS_EMPTY(&certs->list));

  for (PRCList* cursor = PR_NEXT_LINK(&certs->list); cursor != &certs->list;
       cursor = PR_NEXT_LINK(cursor)) {
    CERTCertListNode* node = (CERTCertListNode*)cursor;
    SECItem der;
    ASSERT_EQ(SECSuccess, CERT_GetCertificateDer(node->cert, &der));
    ASSERT_EQ(0, SECITEM_CompareItem(&der, &node->cert->derCert));
  }
}

TEST_F(CertTest, GetCertDerBad) {
  EXPECT_EQ(SECFailure, CERT_GetCertificateDer(nullptr, nullptr));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  ScopedCERTCertList certs(PK11_ListCerts(PK11CertListAll, nullptr));
  ASSERT_FALSE(PR_CLIST_IS_EMPTY(&certs->list));
  CERTCertListNode* node = (CERTCertListNode*)PR_NEXT_LINK(&certs->list);
  EXPECT_EQ(SECFailure, CERT_GetCertificateDer(node->cert, nullptr));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  SECItem der;
  EXPECT_EQ(SECFailure, CERT_GetCertificateDer(nullptr, &der));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}
}
