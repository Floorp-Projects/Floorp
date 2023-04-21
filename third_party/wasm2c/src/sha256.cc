/*
 * Copyright 2017 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wabt/sha256.h"

#if HAVE_OPENSSL_SHA_H
#include <openssl/sha.h>
#else
#include "picosha2.h"
#endif

namespace wabt {

/**
 * SHA-256 the "input" sv into the output "digest".
 *
 * Uses OpenSSL's libcrypto or vendored PicoSHA2.
 */
void sha256(std::string_view input, std::string& digest) {
  digest.clear();

#if HAVE_OPENSSL_SHA_H
  digest.resize(SHA256_DIGEST_LENGTH);
  if (!SHA256(reinterpret_cast<const uint8_t*>(input.data()), input.size(),
              reinterpret_cast<uint8_t*>(digest.data()))) {
    // should not be possible to fail here, but check (and abort) just in case
    abort();
  }
#else
  digest.resize(picosha2::k_digest_size);
  picosha2::hash256(input, digest);
#endif
}

}  // namespace wabt
