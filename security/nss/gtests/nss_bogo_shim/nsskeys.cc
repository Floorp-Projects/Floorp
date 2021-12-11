/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsskeys.h"

#include <cstring>

#include <fstream>
#include <iostream>
#include <string>

#include "cert.h"
#include "keyhi.h"
#include "nspr.h"
#include "nss.h"
#include "nssb64.h"
#include "pk11pub.h"

const std::string kPEMBegin = "-----BEGIN ";
const std::string kPEMEnd = "-----END ";

// Read a PEM file, base64 decode it, and return the result.
static bool ReadPEMFile(const std::string& filename, SECItem* item) {
  std::ifstream in(filename);
  if (in.bad()) return false;

  char buf[1024];
  in.getline(buf, sizeof(buf));
  if (in.bad()) return false;

  if (strncmp(buf, kPEMBegin.c_str(), kPEMBegin.size())) return false;

  std::string value = "";
  for (;;) {
    in.getline(buf, sizeof(buf));
    if (in.bad()) return false;

    if (!strncmp(buf, kPEMEnd.c_str(), kPEMEnd.size())) break;

    value += buf;
  }

  // Now we have a base64-encoded block.
  if (!NSSBase64_DecodeBuffer(nullptr, item, value.c_str(), value.size()))
    return false;

  return true;
}

SECKEYPrivateKey* ReadPrivateKey(const std::string& file) {
  SECItem item = {siBuffer, nullptr, 0};

  if (!ReadPEMFile(file, &item)) return nullptr;
  SECKEYPrivateKey* privkey = NULL;
  PK11SlotInfo* slot = PK11_GetInternalSlot();
  SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
      slot, &item, nullptr, nullptr, PR_FALSE, PR_FALSE,
      KU_KEY_ENCIPHERMENT | KU_DATA_ENCIPHERMENT | KU_DIGITAL_SIGNATURE,
      &privkey, nullptr);
  PK11_FreeSlot(slot);
  SECITEM_FreeItem(&item, PR_FALSE);
  if (rv != SECSuccess) {
    std::cerr << "Couldn't import key " << PORT_ErrorToString(PORT_GetError())
              << "\n";
    return nullptr;
  }

  return privkey;
}

CERTCertificate* ReadCertificate(const std::string& file) {
  SECItem item = {siBuffer, nullptr, 0};

  if (!ReadPEMFile(file, &item)) return nullptr;

  CERTCertificate* cert = CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &item, NULL, PR_FALSE, PR_TRUE);
  SECITEM_FreeItem(&item, PR_FALSE);
  return cert;
}
