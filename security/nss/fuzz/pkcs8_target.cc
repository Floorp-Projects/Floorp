/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <vector>

#include "keyhi.h"
#include "pk11pub.h"

#include "asn1_mutators.h"
#include "shared.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  SECItem data = {siBuffer, (unsigned char *)Data, (unsigned int)Size};

  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());
  assert(db != nullptr);

  PK11SlotInfo *slot = PK11_GetInternalSlot();
  assert(slot != nullptr);

  SECKEYPrivateKey *key = nullptr;
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(slot, &data, nullptr, nullptr,
                                               false, false, KU_ALL, &key,
                                               nullptr) == SECSuccess) {
    SECKEY_DestroyPrivateKey(key);
  }

  PK11_FreeSlot(slot);
  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t max_size, unsigned int seed) {
  return CustomMutate(
      Mutators({ASN1MutatorFlipConstructed, ASN1MutatorChangeType}), data, size,
      max_size, seed);
}
