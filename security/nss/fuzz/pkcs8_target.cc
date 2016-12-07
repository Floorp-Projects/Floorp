/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdint.h>
#include <memory>

#include "keyhi.h"
#include "pk11pub.h"

#include "registry.h"
#include "shared.h"

extern "C" int pkcs8_fuzzing_target(const uint8_t *Data, size_t Size) {
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

REGISTER_FUZZING_TARGET("pkcs8", pkcs8_fuzzing_target, 2048, "PKCS#8 Import",
                        {})
