/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "digesttool.h"
#include "argparse.h"
#include "scoped_ptrs.h"
#include "util.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <hasht.h>  // contains supported digest types
#include <nss.h>
#include <pk11pub.h>
#include <prio.h>

static SECOidData* HashTypeToOID(HASH_HashType hashtype) {
  SECOidTag hashtag;

  if (hashtype <= HASH_AlgNULL || hashtype >= HASH_AlgTOTAL) {
    return nullptr;
  }

  switch (hashtype) {
    case HASH_AlgMD5:
      hashtag = SEC_OID_MD5;
      break;
    case HASH_AlgSHA1:
      hashtag = SEC_OID_SHA1;
      break;
    case HASH_AlgSHA224:
      hashtag = SEC_OID_SHA224;
      break;
    case HASH_AlgSHA256:
      hashtag = SEC_OID_SHA256;
      break;
    case HASH_AlgSHA384:
      hashtag = SEC_OID_SHA384;
      break;
    case HASH_AlgSHA512:
      hashtag = SEC_OID_SHA512;
      break;
    default:
      return nullptr;
  }

  return SECOID_FindOIDByTag(hashtag);
}

static SECOidData* HashNameToOID(const std::string& hashName) {
  for (size_t htype = HASH_AlgNULL + 1; htype < HASH_AlgTOTAL; htype++) {
    SECOidData* hashOID = HashTypeToOID(static_cast<HASH_HashType>(htype));
    if (hashOID && std::string(hashOID->desc) == hashName) {
      return hashOID;
    }
  }

  return nullptr;
}

static bool Digest(const ArgParser& parser, SECOidData* hashOID);
static bool ComputeDigest(std::istream& is, ScopedPK11Context& hashCtx);

bool DigestTool::Run(const std::vector<std::string>& arguments) {
  ArgParser parser(arguments);

  if (parser.GetPositionalArgumentCount() != 1) {
    Usage();
    return false;
  }

  // no need for a db for the digest tool
  SECStatus rv = NSS_NoDB_Init(".");
  if (rv != SECSuccess) {
    std::cerr << "NSS init failed!" << std::endl;
    return false;
  }

  std::string hashName = parser.GetPositionalArgument(0);
  std::transform(hashName.begin(), hashName.end(), hashName.begin(), ::toupper);
  SECOidData* hashOID = HashNameToOID(hashName);
  if (hashOID == nullptr) {
    std::cerr << "Error: Unknown digest type "
              << parser.GetPositionalArgument(0) << "." << std::endl;
    return false;
  }

  bool ret = Digest(parser, hashOID);

  // shutdown nss
  if (NSS_Shutdown() != SECSuccess) {
    std::cerr << "NSS Shutdown failed!" << std::endl;
    return false;
  }

  return ret;
}

void DigestTool::Usage() {
  std::cerr << "Usage: nss digest md5|sha-1|sha-224|sha-256|sha-384|sha-512 "
               "[--infile <path>]"
            << std::endl;
}

static bool Digest(const ArgParser& parser, SECOidData* hashOID) {
  std::string inputFile;
  if (parser.Has("--infile")) {
    inputFile = parser.Get("--infile");
  }

  ScopedPK11Context hashCtx(PK11_CreateDigestContext(hashOID->offset));
  if (hashCtx == nullptr) {
    std::cerr << "Creating digest context failed." << std::endl;
    return false;
  }
  PK11_DigestBegin(hashCtx.get());

  std::ifstream fis;
  std::istream& is = GetStreamFromFileOrStdin(inputFile, fis);
  if (!is.good() || !ComputeDigest(is, hashCtx)) {
    return false;
  }

  unsigned char digest[HASH_LENGTH_MAX];
  unsigned int len;
  SECStatus rv = PK11_DigestFinal(hashCtx.get(), digest, &len, HASH_LENGTH_MAX);
  if (rv != SECSuccess || len == 0) {
    std::cerr << "Calculating final hash value failed." << std::endl;
    return false;
  }

  // human readable output
  for (size_t i = 0; i < len; i++) {
    std::cout << std::setw(2) << std::setfill('0') << std::hex
              << static_cast<int>(digest[i]);
  }
  std::cout << std::endl;

  return true;
}

static bool ComputeDigest(std::istream& is, ScopedPK11Context& hashCtx) {
  while (is) {
    unsigned char buf[4096];
    is.read(reinterpret_cast<char*>(buf), sizeof(buf));
    if (is.fail() && !is.eof()) {
      std::cerr << "Error reading from input stream." << std::endl;
      return false;
    }
    SECStatus rv = PK11_DigestOp(hashCtx.get(), buf, is.gcount());
    if (rv != SECSuccess) {
      std::cerr << "PK11_DigestOp failed." << std::endl;
      return false;
    }
  }

  return true;
}
