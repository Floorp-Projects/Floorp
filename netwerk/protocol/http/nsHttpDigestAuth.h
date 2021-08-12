/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDigestAuth_h__
#define nsDigestAuth_h__

#include "nsICryptoHash.h"
#include "nsIHttpAuthenticator.h"
#include "nsStringFwd.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace net {

#define ALGO_SPECIFIED 0x01
#define ALGO_MD5 0x02
#define ALGO_MD5_SESS 0x04
#define ALGO_SHA256 0x08
#define ALGO_SHA256_SESS 0x10
#define QOP_AUTH 0x01
#define QOP_AUTH_INT 0x02

#define NONCE_COUNT_LENGTH 8
#ifndef MD5_DIGEST_LENGTH
#  define MD5_DIGEST_LENGTH 16
#endif
#ifndef SHA256_DIGEST_LENGTH
#  define SHA256_DIGEST_LENGTH 32
#endif

//-----------------------------------------------------------------------------
// nsHttpDigestAuth
//-----------------------------------------------------------------------------

class nsHttpDigestAuth final : public nsIHttpAuthenticator {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPAUTHENTICATOR

  nsHttpDigestAuth() = default;

  static already_AddRefed<nsIHttpAuthenticator> GetOrCreate();

  [[nodiscard]] static nsresult ParseChallenge(
      const nsACString& aChallenge, nsACString& realm, nsACString& domain,
      nsACString& nonce, nsACString& opaque, bool* stale, uint16_t* algorithm,
      uint16_t* qop);

 protected:
  ~nsHttpDigestAuth() = default;

  [[nodiscard]] nsresult ExpandToHex(const char* digest, char* result,
                                     uint16_t algorithm);

  [[nodiscard]] nsresult CalculateResponse(
      const char* ha1_digest, const char* ha2_digest, uint16_t algorithm,
      const nsCString& nonce, uint16_t qop, const char* nonce_count,
      const nsCString& cnonce, char* result);

  [[nodiscard]] nsresult CalculateHA1(const nsCString& username,
                                      const nsCString& password,
                                      const nsCString& realm,
                                      uint16_t algorithm,
                                      const nsCString& nonce,
                                      const nsCString& cnonce, char* result);

  [[nodiscard]] nsresult CalculateHA2(const nsCString& http_method,
                                      const nsCString& http_uri_path,
                                      uint16_t algorithm, uint16_t qop,
                                      const char* body_digest, char* result);

  // result is in mHashBuf
  [[nodiscard]] nsresult DigestHash(const char* buf, uint32_t len,
                                    uint16_t algorithm);

  [[nodiscard]] nsresult GetMethodAndPath(nsIHttpAuthenticableChannel*, bool,
                                          nsCString&, nsCString&);

  // append the quoted version of value to aHeaderLine
  [[nodiscard]] nsresult AppendQuotedString(const nsACString& value,
                                            nsACString& aHeaderLine);

 protected:
  nsCOMPtr<nsICryptoHash> mVerifier;
  char mHashBuf[SHA256_DIGEST_LENGTH]{0};

  static StaticRefPtr<nsHttpDigestAuth> gSingleton;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpDigestAuth_h__
