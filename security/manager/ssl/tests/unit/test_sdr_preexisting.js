// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that the SDR implementation is able to decrypt strings encrypted using
// a preexisting NSS key database. Creating the database is straight-forward:
// simply run Firefox (or xpcshell) and encrypt something using
// nsISecretDecoderRing (e.g. by saving a password or directly using the
// interface). The resulting key3.db file (in the profile directory) now
// contains the private key used to encrypt the data.
// "Upgrading" a key3.db with certutil to use on Android appears not to work.
// Because the keys have to be the same for this test to work the way it does,
// the key from key3.db must be extracted and added to a new key4.db. This can
// be done with NSS' PK11_* APIs like so (although note that the following code
// is not guaranteed to compile or work, but is more of a guideline for how to
// do this in the future if necessary):
//
// #include <stdio.h>
//
// #include "nss.h"
// #include "pk11pub.h"
// #include "prerror.h"
// #include "secerr.h"
//
// void printPRError(const char* message) {
//   fprintf(stderr, "%s: %s\n", message, PR_ErrorToString(PR_GetError(), 0));
// }
//
// int main(int argc, char* argv[]) {
//   if (NSS_Initialize(".", "", "", "", NSS_INIT_NOMODDB | NSS_INIT_NOROOTINIT)
//         != SECSuccess) {
//     printPRError("NSS_Initialize failed");
//     return 1;
//   }
//
//   PK11SlotInfo* slot = PK11_GetInternalKeySlot();
//   if (!slot) {
//     printPRError("PK11_GetInternalKeySlot failed");
//     return 1;
//   }
//
//   // Create a key to wrap the SDR key to export it.
//   unsigned char wrappingKeyIDBytes[] = { 0 };
//   SECItem wrappingKeyID = {
//     siBuffer,
//     wrappingKeyIDBytes,
//     sizeof(wrappingKeyIDBytes)
//   };
//   PK11SymKey* wrappingKey = PK11_TokenKeyGen(slot, CKM_DES3_CBC, 0, 0,
//                                              &wrappingKeyID, PR_FALSE, NULL);
//   if (!wrappingKey) {
//     printPRError("PK11_TokenKeyGen failed");
//     return 1;
//   }
//
//   // This is the magic identifier NSS uses for the SDR key.
//   unsigned char sdrKeyIDBytes[] = {
//     0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
//   };
//   SECItem sdrKeyID = { siBuffer, sdrKeyIDBytes, sizeof(sdrKeyIDBytes) };
//   PK11SymKey* sdrKey = PK11_FindFixedKey(slot, CKM_DES3_CBC, &sdrKeyID,
//                                          NULL);
//   if (!sdrKey) {
//     printPRError("PK11_FindFixedKey failed");
//     return 1;
//   }
//
//   // Wrap the SDR key.
//   unsigned char wrappedKeyBuf[1024];
//   SECItem wrapped = { siBuffer, wrappedKeyBuf, sizeof(wrappedKeyBuf) };
//   if (PK11_WrapSymKey(CKM_DES3_ECB, NULL, wrappingKey, sdrKey, &wrapped)
//         != SECSuccess) {
//     printPRError("PK11_WrapSymKey failed");
//     return 1;
//   }
//
//   // Unwrap the SDR key (NSS considers the SDR key "sensitive" and so won't
//   // just export it as raw key material - we have to export it and then
//   // re-import it as non-sensitive to get that data.
//   PK11SymKey* unwrapped = PK11_UnwrapSymKey(wrappingKey, CKM_DES3_ECB, NULL,
//                                             &wrapped, CKM_DES3_CBC,
//                                             CKA_ENCRYPT, 0);
//   if (!unwrapped) {
//     printPRError("PK11_UnwrapSymKey failed");
//     return 1;
//   }
//   if (PK11_ExtractKeyValue(unwrapped) != SECSuccess) {
//     printPRError("PK11_ExtractKeyValue failed");
//     return 1;
//   }
//   SECItem* keyData = PK11_GetKeyData(unwrapped);
//   if (!keyData) {
//     printPRError("PK11_GetKeyData failed");
//     return 1;
//   }
//   for (int i = 0; i < keyData->len; i++) {
//     printf("0x%02hhx, ", keyData->data[i]);
//   }
//   printf("\n");
//
//   PK11_FreeSymKey(unwrapped);
//   PK11_FreeSymKey(sdrKey);
//   PK11_FreeSymKey(wrappingKey);
//   PK11_FreeSlot(slot);
//
//   if (NSS_Shutdown() != SECSuccess) {
//     printPRError("NSS_Shutdown failed");
//     return 1;
//   }
//   return 0;
// }
//
// The output of compiling and running the above should be the bytes of the SDR
// key. Given that, create a key4.db with an empty password using
// `certutil -N -d sql:.` and then compile and run the following:
//
// #include <stdio.h>
//
// #include "nss.h"
// #include "pk11pub.h"
// #include "prerror.h"
// #include "secerr.h"
// #include "secmod.h"
//
// void printPRError(const char* message) {
//   fprintf(stderr, "%s: %s\n", message, PR_ErrorToString(PR_GetError(), 0));
// }
//
// int main(int argc, char* argv[]) {
//   if (NSS_Initialize("sql:.", "", "", "",
//                      NSS_INIT_NOMODDB | NSS_INIT_NOROOTINIT) != SECSuccess) {
//     printPRError("NSS_Initialize failed");
//     return 1;
//   }
//
//   PK11SlotInfo* slot = PK11_GetInternalKeySlot();
//   if (!slot) {
//     printPRError("PK11_GetInternalKeySlot failed");
//     return 1;
//   }
//
//   // These are the bytes of the SDR key from the previous step:
//   unsigned char keyBytes[] = {
//     0x70, 0xab, 0xea, 0x1f, 0x8f, 0xe3, 0x4a, 0x7a, 0xb5, 0xb0, 0x43, 0xe5,
//     0x51, 0x83, 0x86, 0xe5, 0xb3, 0x43, 0xa8, 0x1f, 0xc1, 0x57, 0x86, 0x46
//   };
//   SECItem keyItem = { siBuffer, keyBytes, sizeof(keyBytes) };
//   PK11SymKey* key = PK11_ImportSymKey(slot, CKM_DES3_CBC, PK11_OriginUnwrap,
//                                       CKA_ENCRYPT, &keyItem, NULL);
//   if (!key) {
//     printPRError("PK11_ImportSymKey failed");
//     return 1;
//   }
//
//   PK11_FreeSymKey(key);
//   PK11_FreeSlot(slot);
//
//   if (NSS_Shutdown() != SECSuccess) {
//     printPRError("NSS_Shutdown failed");
//     return 1;
//   }
//   return 0;
// }
//
// This should create a key4.db file with the SDR key. (Note however that this
// does not set the magic key ID for the SDR key. Currently this is not a
// problem because the NSS implementation that backs the SDR simply tries all
// applicable keys it has when decrypting, so this still works.)

function run_test() {
  const isAndroid = AppConstants.platform == "android";
  const keyDBName = isAndroid ? "key4.db" : "key3.db";
  let profile = do_get_profile();
  let keyDBFile = do_get_file(`test_sdr_preexisting/${keyDBName}`);
  keyDBFile.copyTo(profile, keyDBName);

  let sdr = Cc["@mozilla.org/security/sdr;1"]
              .getService(Ci.nsISecretDecoderRing);

  let testcases = [
    // a full padding block
    { ciphertext: "MDoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECGeDHwVfyFqzBBAYvqMq/kDMsrARVNdC1C8d",
      plaintext: "password" },
    // 7 bytes of padding
    { ciphertext: "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECCAzLDVmYG2/BAh3IoIsMmT8dQ==",
      plaintext: "a" },
    // 6 bytes of padding
    { ciphertext: "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECPN8zlZzn8FdBAiu2acpT8UHsg==",
      plaintext: "bb" },
    // 1 byte of padding
    { ciphertext: "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECD5px1eMKkJQBAgUPp35GlrDvQ==",
      plaintext: "!seven!" },
    // 2 bytes of padding
    { ciphertext: "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECMh0hLtKDyUdBAixw9UZsMt+vA==",
      plaintext: "sixsix" },
    // long plaintext requiring more than two blocks
    { ciphertext: "MFoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECDRX1qi+/FX1BDATFIcIneQjvBuq3wdFxzllJt2VtUD69ACdOKAXH3eA87oHDvuHqOeCDwRy4UzoG5s=",
      plaintext: "thisismuchlongerandsotakesupmultipleblocks" },
    // this differs from the previous ciphertext by one bit and demonstrates
    // that this implementation does not enforce message integrity
    { ciphertext: "MFoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECDRX1qi+/FX1BDAbFIcIneQjvBuq3wdFxzllJt2VtUD69ACdOKAXH3eA87oHDvuHqOeCDwRy4UzoG5s=",
      plaintext: "nnLbuwLRkhlongerandsotakesupmultipleblocks" },
  ];

  for (let testcase of testcases) {
    let decrypted = sdr.decryptString(testcase.ciphertext);
    equal(decrypted, testcase.plaintext,
          "decrypted ciphertext should match expected plaintext");
  }
}
