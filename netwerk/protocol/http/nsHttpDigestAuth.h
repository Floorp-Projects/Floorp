/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDigestAuth_h__
#define nsDigestAuth_h__

#include "nsIHttpAuthenticator.h"
#include "nsStringFwd.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

class nsICryptoHash;

namespace mozilla { namespace net {

#define ALGO_SPECIFIED 0x01
#define ALGO_MD5 0x02
#define ALGO_MD5_SESS 0x04
#define QOP_AUTH 0x01
#define QOP_AUTH_INT 0x02

#define DIGEST_LENGTH 16
#define EXPANDED_DIGEST_LENGTH 32
#define NONCE_COUNT_LENGTH 8

//-----------------------------------------------------------------------------
// nsHttpDigestAuth
//-----------------------------------------------------------------------------

class nsHttpDigestAuth MOZ_FINAL : public nsIHttpAuthenticator
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPAUTHENTICATOR

    nsHttpDigestAuth();
    ~nsHttpDigestAuth();

  protected:
    nsresult ExpandToHex(const char * digest, char * result);

    nsresult CalculateResponse(const char * ha1_digest,
                               const char * ha2_digest,
                               const nsAFlatCString & nonce,
                               uint16_t qop,
                               const char * nonce_count,
                               const nsAFlatCString & cnonce,
                               char * result);

    nsresult CalculateHA1(const nsAFlatCString & username,
                          const nsAFlatCString & password,
                          const nsAFlatCString & realm,
                          uint16_t algorithm,
                          const nsAFlatCString & nonce,
                          const nsAFlatCString & cnonce,
                          char * result);

    nsresult CalculateHA2(const nsAFlatCString & http_method,
                          const nsAFlatCString & http_uri_path,
                          uint16_t qop,
                          const char * body_digest,
                          char * result);

    nsresult ParseChallenge(const char * challenge,
                            nsACString & realm,
                            nsACString & domain,
                            nsACString & nonce,
                            nsACString & opaque,
                            bool * stale,
                            uint16_t * algorithm,
                            uint16_t * qop);

    // result is in mHashBuf
    nsresult MD5Hash(const char *buf, uint32_t len);

    nsresult GetMethodAndPath(nsIHttpAuthenticableChannel *,
                              bool, nsCString &, nsCString &);

    // append the quoted version of value to aHeaderLine
    nsresult AppendQuotedString(const nsACString & value,
                                nsACString & aHeaderLine);

  protected:
    nsCOMPtr<nsICryptoHash>        mVerifier;
    char                           mHashBuf[DIGEST_LENGTH];
};

}} // namespace mozilla::net

#endif // nsHttpDigestAuth_h__
