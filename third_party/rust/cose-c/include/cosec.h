/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

enum CoseSignatureType
{
  ES256 = 0,
  ES384 = 1,
  ES512 = 2,
  PS256 = 3
};

extern "C" {
typedef bool (*cose_verify_callback)(const uint8_t* payload,
                                     size_t payload_len,
                                     const uint8_t** cert_chain,
                                     size_t cert_chain_len,
                                     const size_t* certs_len,
                                     const uint8_t* ee_cert,
                                     size_t ee_cert_len,
                                     const uint8_t* signature,
                                     size_t signature_len,
                                     uint8_t algorithm,
                                     void* ctx);
bool
verify_cose_signature_ffi(const uint8_t* payload,
                          size_t payload_len,
                          const uint8_t* signature,
                          size_t signature_len,
                          void* ctx,
                          cose_verify_callback);
}
