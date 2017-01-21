/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <vector>

#include "FuzzerInternal.h"
#include "hasht.h"
#include "pk11pub.h"
#include "secoidt.h"
#include "shared.h"

extern const uint16_t DEFAULT_MAX_LENGTH = 4096U;

const std::vector<SECOidTag> algos = {SEC_OID_MD5, SEC_OID_SHA1, SEC_OID_SHA256,
                                      SEC_OID_SHA384, SEC_OID_SHA512};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  uint8_t hashOut[HASH_LENGTH_MAX];

  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());
  assert(db != nullptr);

  // simple hashing.
  for (auto algo : algos) {
    assert(PK11_HashBuf(algo, hashOut, data, size) == SECSuccess);
  }

  // hashing with context.
  for (auto algo : algos) {
    unsigned int len = 0;
    PK11Context *context = PK11_CreateDigestContext(algo);
    assert(context != nullptr);
    assert(PK11_DigestBegin(context) == SECSuccess);
    assert(PK11_DigestFinal(context, hashOut, &len, HASH_LENGTH_MAX) ==
           SECSuccess);
    PK11_DestroyContext(context, PR_TRUE);
  }

  return 0;
}
