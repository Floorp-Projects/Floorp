/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"

#include "nsHttp.h"
#include "nsHttpDigestAuth.h"
#include "nsIHttpAuthenticableChannel.h"
#include "nsISupportsPrimitives.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsEscape.h"
#include "nsNetCID.h"
#include "nsCRT.h"
#include "nsICryptoHash.h"
#include "nsComponentManagerUtils.h"

constexpr uint16_t DigestLength(uint16_t aAlgorithm) {
  if (aAlgorithm & (ALGO_SHA256 | ALGO_SHA256_SESS)) {
    return SHA256_DIGEST_LENGTH;
  }
  return MD5_DIGEST_LENGTH;
}

namespace mozilla {
namespace net {

StaticRefPtr<nsHttpDigestAuth> nsHttpDigestAuth::gSingleton;

already_AddRefed<nsIHttpAuthenticator> nsHttpDigestAuth::GetOrCreate() {
  nsCOMPtr<nsIHttpAuthenticator> authenticator;
  if (gSingleton) {
    authenticator = gSingleton;
  } else {
    gSingleton = new nsHttpDigestAuth();
    ClearOnShutdown(&gSingleton);
    authenticator = gSingleton;
  }

  return authenticator.forget();
}

//-----------------------------------------------------------------------------
// nsHttpDigestAuth::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsHttpDigestAuth, nsIHttpAuthenticator)

//-----------------------------------------------------------------------------
// nsHttpDigestAuth <protected>
//-----------------------------------------------------------------------------

nsresult nsHttpDigestAuth::DigestHash(const char* buf, uint32_t len,
                                      uint16_t algorithm) {
  nsresult rv;

  // Cache a reference to the nsICryptoHash instance since we'll be calling
  // this function frequently.
  if (!mVerifier) {
    mVerifier = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      LOG(("nsHttpDigestAuth: no crypto hash!\n"));
      return rv;
    }
  }

  uint32_t dlen;
  if (algorithm & (ALGO_SHA256 | ALGO_SHA256_SESS)) {
    rv = mVerifier->Init(nsICryptoHash::SHA256);
    dlen = SHA256_DIGEST_LENGTH;
  } else {
    rv = mVerifier->Init(nsICryptoHash::MD5);
    dlen = MD5_DIGEST_LENGTH;
  }
  if (NS_FAILED(rv)) return rv;

  rv = mVerifier->Update((unsigned char*)buf, len);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString hashString;
  rv = mVerifier->Finish(false, hashString);
  if (NS_FAILED(rv)) return rv;

  NS_ENSURE_STATE(hashString.Length() == dlen);
  memcpy(mHashBuf, hashString.get(), hashString.Length());

  return rv;
}

nsresult nsHttpDigestAuth::GetMethodAndPath(
    nsIHttpAuthenticableChannel* authChannel, bool isProxyAuth,
    nsCString& httpMethod, nsCString& path) {
  nsresult rv, rv2;
  nsCOMPtr<nsIURI> uri;
  rv = authChannel->GetURI(getter_AddRefs(uri));
  if (NS_SUCCEEDED(rv)) {
    bool proxyMethodIsConnect;
    rv = authChannel->GetProxyMethodIsConnect(&proxyMethodIsConnect);
    if (NS_SUCCEEDED(rv)) {
      if (proxyMethodIsConnect && isProxyAuth) {
        httpMethod.AssignLiteral("CONNECT");
        //
        // generate hostname:port string. (unfortunately uri->GetHostPort
        // leaves out the port if it matches the default value, so we can't
        // just call it.)
        //
        int32_t port;
        rv = uri->GetAsciiHost(path);
        rv2 = uri->GetPort(&port);
        if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv2)) {
          path.Append(':');
          path.AppendInt(port < 0 ? NS_HTTPS_DEFAULT_PORT : port);
        }
      } else {
        rv = authChannel->GetRequestMethod(httpMethod);
        rv2 = uri->GetPathQueryRef(path);
        if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv2)) {
          //
          // strip any fragment identifier from the URL path.
          //
          int32_t ref = path.RFindChar('#');
          if (ref != kNotFound) path.Truncate(ref);
          //
          // make sure we escape any UTF-8 characters in the URI path.  the
          // digest auth uri attribute needs to match the request-URI.
          //
          // XXX we should really ask the HTTP channel for this string
          // instead of regenerating it here.
          //
          nsAutoCString buf;
          rv = NS_EscapeURL(path, esc_OnlyNonASCII | esc_Spaces, buf,
                            mozilla::fallible);
          if (NS_SUCCEEDED(rv)) {
            path = buf;
          }
        }
      }
    }
  }
  return rv;
}

//-----------------------------------------------------------------------------
// nsHttpDigestAuth::nsIHttpAuthenticator
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpDigestAuth::ChallengeReceived(nsIHttpAuthenticableChannel* authChannel,
                                    const char* challenge, bool isProxyAuth,
                                    nsISupports** sessionState,
                                    nsISupports** continuationState,
                                    bool* result) {
  nsAutoCString realm, domain, nonce, opaque;
  bool stale;
  uint16_t algorithm, qop;

  nsresult rv = ParseChallenge(challenge, realm, domain, nonce, opaque, &stale,
                               &algorithm, &qop);

  if (!(algorithm &
        (ALGO_MD5 | ALGO_MD5_SESS | ALGO_SHA256 | ALGO_SHA256_SESS))) {
    // they asked for an algorithm that we do not support yet (like SHA-512/256)
    NS_WARNING("unsupported algorithm requested by Digest authentication");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (NS_FAILED(rv)) return rv;

  // if the challenge has the "stale" flag set, then the user identity is not
  // necessarily invalid.  by returning FALSE here we can suppress username
  // and password prompting that usually accompanies a 401/407 challenge.
  *result = !stale;

  // clear any existing nonce_count since we have a new challenge.
  NS_IF_RELEASE(*sessionState);
  return NS_OK;
}

NS_IMETHODIMP
nsHttpDigestAuth::GenerateCredentialsAsync(
    nsIHttpAuthenticableChannel* authChannel,
    nsIHttpAuthenticatorCallback* aCallback, const char* challenge,
    bool isProxyAuth, const char16_t* domain, const char16_t* username,
    const char16_t* password, nsISupports* sessionState,
    nsISupports* continuationState, nsICancelable** aCancellable) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpDigestAuth::GenerateCredentials(
    nsIHttpAuthenticableChannel* authChannel, const char* challenge,
    bool isProxyAuth, const char16_t* userdomain, const char16_t* username,
    const char16_t* password, nsISupports** sessionState,
    nsISupports** continuationState, uint32_t* aFlags, char** creds)

{
  LOG(("nsHttpDigestAuth::GenerateCredentials [challenge=%s]\n", challenge));

  NS_ENSURE_ARG_POINTER(creds);

  *aFlags = 0;

  bool isDigestAuth = !nsCRT::strncasecmp(challenge, "digest ", 7);
  NS_ENSURE_TRUE(isDigestAuth, NS_ERROR_UNEXPECTED);

  // IIS implementation requires extra quotes
  bool requireExtraQuotes = false;
  {
    nsAutoCString serverVal;
    Unused << authChannel->GetServerResponseHeader(serverVal);
    if (!serverVal.IsEmpty()) {
      requireExtraQuotes =
          !nsCRT::strncasecmp(serverVal.get(), "Microsoft-IIS", 13);
    }
  }

  nsresult rv;
  nsAutoCString httpMethod;
  nsAutoCString path;
  rv = GetMethodAndPath(authChannel, isProxyAuth, httpMethod, path);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString realm, domain, nonce, opaque;
  bool stale;
  uint16_t algorithm, qop;

  rv = ParseChallenge(challenge, realm, domain, nonce, opaque, &stale,
                      &algorithm, &qop);
  if (NS_FAILED(rv)) {
    LOG(
        ("nsHttpDigestAuth::GenerateCredentials [ParseChallenge failed "
         "rv=%" PRIx32 "]\n",
         static_cast<uint32_t>(rv)));
    return rv;
  }

  const uint32_t dhexlen = 2 * DigestLength(algorithm) + 1;
  char ha1_digest[dhexlen];
  char ha2_digest[dhexlen];
  char response_digest[dhexlen];
  char upload_data_digest[dhexlen];

  if (qop & QOP_AUTH_INT) {
    // we do not support auth-int "quality of protection" currently
    qop &= ~QOP_AUTH_INT;

    NS_WARNING(
        "no support for Digest authentication with data integrity quality of "
        "protection");

    /* TODO: to support auth-int, we need to get an MD5 digest of
     * TODO: the data uploaded with this request.
     * TODO: however, i am not sure how to read in the file in without
     * TODO: disturbing the channel''s use of it. do i need to copy it
     * TODO: somehow?
     */
#if 0
    if (http_channel != nullptr)
    {
      nsIInputStream * upload;
      nsCOMPtr<nsIUploadChannel> uc = do_QueryInterface(http_channel);
      NS_ENSURE_TRUE(uc, NS_ERROR_UNEXPECTED);
      uc->GetUploadStream(&upload);
      if (upload) {
        char * upload_buffer;
        int upload_buffer_length = 0;
        //TODO: read input stream into buffer
        const char * digest = (const char*)
        nsNetwerkMD5Digest(upload_buffer, upload_buffer_length);
        ExpandToHex(digest, upload_data_digest);
        NS_RELEASE(upload);
      }
    }
#endif
  }

  if (!(algorithm &
        (ALGO_MD5 | ALGO_MD5_SESS | ALGO_SHA256 | ALGO_SHA256_SESS))) {
    // they asked only for algorithms that we do not support
    NS_WARNING("unsupported algorithm requested by Digest authentication");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  //
  // the following are for increasing security.  see RFC 2617 for more
  // information.
  //
  // nonce_count allows the server to keep track of auth challenges (to help
  // prevent spoofing). we increase this count every time.
  //
  char nonce_count[NONCE_COUNT_LENGTH + 1] = "00000001";  // in hex
  if (*sessionState) {
    nsCOMPtr<nsISupportsPRUint32> v(do_QueryInterface(*sessionState));
    if (v) {
      uint32_t nc;
      v->GetData(&nc);
      SprintfLiteral(nonce_count, "%08x", ++nc);
      v->SetData(nc);
    }
  } else {
    nsCOMPtr<nsISupportsPRUint32> v(
        do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID));
    if (v) {
      v->SetData(1);
      v.forget(sessionState);
    }
  }
  LOG(("   nonce_count=%s\n", nonce_count));

  //
  // this lets the client verify the server response (via a server
  // returned Authentication-Info header). also used for session info.
  //
  nsAutoCString cnonce;
  static const char hexChar[] = "0123456789abcdef";
  for (int i = 0; i < 16; ++i) {
    cnonce.Append(hexChar[(int)(15.0 * rand() / (RAND_MAX + 1.0))]);
  }
  LOG(("   cnonce=%s\n", cnonce.get()));

  //
  // calculate credentials
  //

  NS_ConvertUTF16toUTF8 cUser(username), cPass(password);
  rv = CalculateHA1(cUser, cPass, realm, algorithm, nonce, cnonce, ha1_digest);
  if (NS_FAILED(rv)) return rv;

  rv = CalculateHA2(httpMethod, path, algorithm, qop, upload_data_digest,
                    ha2_digest);
  if (NS_FAILED(rv)) return rv;

  rv = CalculateResponse(ha1_digest, ha2_digest, algorithm, nonce, qop,
                         nonce_count, cnonce, response_digest);
  if (NS_FAILED(rv)) return rv;

  //
  // Values that need to match the quoted-string production from RFC 2616:
  //
  //    username
  //    realm
  //    nonce
  //    opaque
  //    cnonce
  //

  nsAutoCString authString;

  authString.AssignLiteral("Digest username=");
  rv = AppendQuotedString(cUser, authString);
  NS_ENSURE_SUCCESS(rv, rv);

  authString.AppendLiteral(", realm=");
  rv = AppendQuotedString(realm, authString);
  NS_ENSURE_SUCCESS(rv, rv);

  authString.AppendLiteral(", nonce=");
  rv = AppendQuotedString(nonce, authString);
  NS_ENSURE_SUCCESS(rv, rv);

  authString.AppendLiteral(", uri=\"");
  authString += path;
  if (algorithm & ALGO_SPECIFIED) {
    authString.AppendLiteral("\", algorithm=");
    if (algorithm & ALGO_MD5_SESS) {
      authString.AppendLiteral("MD5-sess");
    } else if (algorithm & ALGO_SHA256) {
      authString.AppendLiteral("SHA-256");
    } else if (algorithm & ALGO_SHA256_SESS) {
      authString.AppendLiteral("SHA-256-sess");
    } else {
      authString.AppendLiteral("MD5");
    }
  } else {
    authString += '\"';
  }
  authString.AppendLiteral(", response=\"");
  authString += response_digest;
  authString += '\"';

  if (!opaque.IsEmpty()) {
    authString.AppendLiteral(", opaque=");
    rv = AppendQuotedString(opaque, authString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (qop) {
    authString.AppendLiteral(", qop=");
    if (requireExtraQuotes) authString += '\"';
    authString.AppendLiteral("auth");
    if (qop & QOP_AUTH_INT) authString.AppendLiteral("-int");
    if (requireExtraQuotes) authString += '\"';
    authString.AppendLiteral(", nc=");
    authString += nonce_count;

    authString.AppendLiteral(", cnonce=");
    rv = AppendQuotedString(cnonce, authString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *creds = ToNewCString(authString);
  return NS_OK;
}

NS_IMETHODIMP
nsHttpDigestAuth::GetAuthFlags(uint32_t* flags) {
  *flags = REQUEST_BASED | REUSABLE_CHALLENGE | IDENTITY_ENCRYPTED;
  //
  // NOTE: digest auth credentials must be uniquely computed for each request,
  //       so we do not set the REUSABLE_CREDENTIALS flag.
  //
  return NS_OK;
}

nsresult nsHttpDigestAuth::CalculateResponse(
    const char* ha1_digest, const char* ha2_digest, uint16_t algorithm,
    const nsCString& nonce, uint16_t qop, const char* nonce_count,
    const nsCString& cnonce, char* result) {
  const uint32_t dhexlen = 2 * DigestLength(algorithm);
  uint32_t len = 2 * dhexlen + nonce.Length() + 2;

  if (qop & QOP_AUTH || qop & QOP_AUTH_INT) {
    len += cnonce.Length() + NONCE_COUNT_LENGTH + 3;
    if (qop & QOP_AUTH_INT) {
      len += 8;  // length of "auth-int"
    } else {
      len += 4;  // length of "auth"
    }
  }

  nsAutoCString contents;
  contents.SetCapacity(len);

  contents.Append(ha1_digest, dhexlen);
  contents.Append(':');
  contents.Append(nonce);
  contents.Append(':');

  if (qop & QOP_AUTH || qop & QOP_AUTH_INT) {
    contents.Append(nonce_count, NONCE_COUNT_LENGTH);
    contents.Append(':');
    contents.Append(cnonce);
    contents.Append(':');
    if (qop & QOP_AUTH_INT) {
      contents.AppendLiteral("auth-int:");
    } else {
      contents.AppendLiteral("auth:");
    }
  }

  contents.Append(ha2_digest, dhexlen);

  nsresult rv = DigestHash(contents.get(), contents.Length(), algorithm);
  if (NS_SUCCEEDED(rv)) rv = ExpandToHex(mHashBuf, result, algorithm);
  return rv;
}

nsresult nsHttpDigestAuth::ExpandToHex(const char* digest, char* result,
                                       uint16_t algorithm) {
  int16_t index, value;
  const int16_t dlen = DigestLength(algorithm);

  for (index = 0; index < dlen; index++) {
    value = (digest[index] >> 4) & 0xf;
    if (value < 10) {
      result[index * 2] = value + '0';
    } else {
      result[index * 2] = value - 10 + 'a';
    }

    value = digest[index] & 0xf;
    if (value < 10) {
      result[(index * 2) + 1] = value + '0';
    } else {
      result[(index * 2) + 1] = value - 10 + 'a';
    }
  }

  result[2 * dlen] = 0;
  return NS_OK;
}

nsresult nsHttpDigestAuth::CalculateHA1(const nsCString& username,
                                        const nsCString& password,
                                        const nsCString& realm,
                                        uint16_t algorithm,
                                        const nsCString& nonce,
                                        const nsCString& cnonce, char* result) {
  const int16_t dhexlen = 2 * DigestLength(algorithm);
  int16_t len = username.Length() + password.Length() + realm.Length() + 2;
  if (algorithm & (ALGO_MD5_SESS | ALGO_SHA256_SESS)) {
    int16_t exlen = dhexlen + nonce.Length() + cnonce.Length() + 2;
    if (exlen > len) len = exlen;
  }

  nsAutoCString contents;
  contents.SetCapacity(len);

  contents.Append(username);
  contents.Append(':');
  contents.Append(realm);
  contents.Append(':');
  contents.Append(password);

  nsresult rv;
  rv = DigestHash(contents.get(), contents.Length(), algorithm);
  if (NS_FAILED(rv)) return rv;

  if (algorithm & (ALGO_MD5_SESS | ALGO_SHA256_SESS)) {
    char part1[dhexlen + 1];
    rv = ExpandToHex(mHashBuf, part1, algorithm);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    contents.Assign(part1, dhexlen);
    contents.Append(':');
    contents.Append(nonce);
    contents.Append(':');
    contents.Append(cnonce);

    rv = DigestHash(contents.get(), contents.Length(), algorithm);
    if (NS_FAILED(rv)) return rv;
  }

  return ExpandToHex(mHashBuf, result, algorithm);
}

nsresult nsHttpDigestAuth::CalculateHA2(const nsCString& method,
                                        const nsCString& path,
                                        uint16_t algorithm, uint16_t qop,
                                        const char* bodyDigest, char* result) {
  uint16_t methodLen = method.Length();
  uint32_t pathLen = path.Length();
  uint32_t len = methodLen + pathLen + 1;
  const uint32_t dhexlen = 2 * DigestLength(algorithm);

  if (qop & QOP_AUTH_INT) {
    len += dhexlen + 1;
  }

  nsAutoCString contents;
  contents.SetCapacity(len);

  contents.Assign(method);
  contents.Append(':');
  contents.Append(path);

  if (qop & QOP_AUTH_INT) {
    contents.Append(':');
    contents.Append(bodyDigest, dhexlen);
  }

  nsresult rv = DigestHash(contents.get(), contents.Length(), algorithm);
  if (NS_SUCCEEDED(rv)) rv = ExpandToHex(mHashBuf, result, algorithm);
  return rv;
}

nsresult nsHttpDigestAuth::ParseChallenge(const char* challenge,
                                          nsACString& realm, nsACString& domain,
                                          nsACString& nonce, nsACString& opaque,
                                          bool* stale, uint16_t* algorithm,
                                          uint16_t* qop) {
  // put an absurd, but maximum, length cap on the challenge so
  // that calculations are 32 bit safe
  if (strlen(challenge) > 16000000) {
    return NS_ERROR_INVALID_ARG;
  }

  const char* p = challenge + 6;  // first 6 characters are "Digest"

  *stale = false;
  *algorithm = ALGO_MD5;  // default is MD5
  *qop = 0;

  for (;;) {
    while (*p && (*p == ',' || nsCRT::IsAsciiSpace(*p))) ++p;
    if (!*p) break;

    // name
    int32_t nameStart = (p - challenge);
    while (*p && !nsCRT::IsAsciiSpace(*p) && *p != '=') ++p;
    if (!*p) return NS_ERROR_INVALID_ARG;
    int32_t nameLength = (p - challenge) - nameStart;

    while (*p && (nsCRT::IsAsciiSpace(*p) || *p == '=')) ++p;
    if (!*p) return NS_ERROR_INVALID_ARG;

    bool quoted = false;
    if (*p == '"') {
      ++p;
      quoted = true;
    }

    // value
    int32_t valueStart = (p - challenge);
    int32_t valueLength = 0;
    if (quoted) {
      while (*p && *p != '"') ++p;
      if (*p != '"') return NS_ERROR_INVALID_ARG;
      valueLength = (p - challenge) - valueStart;
      ++p;
    } else {
      while (*p && !nsCRT::IsAsciiSpace(*p) && *p != ',') ++p;
      valueLength = (p - challenge) - valueStart;
    }

    // extract information
    if (nameLength == 5 &&
        nsCRT::strncasecmp(challenge + nameStart, "realm", 5) == 0) {
      realm.Assign(challenge + valueStart, valueLength);
    } else if (nameLength == 6 &&
               nsCRT::strncasecmp(challenge + nameStart, "domain", 6) == 0) {
      domain.Assign(challenge + valueStart, valueLength);
    } else if (nameLength == 5 &&
               nsCRT::strncasecmp(challenge + nameStart, "nonce", 5) == 0) {
      nonce.Assign(challenge + valueStart, valueLength);
    } else if (nameLength == 6 &&
               nsCRT::strncasecmp(challenge + nameStart, "opaque", 6) == 0) {
      opaque.Assign(challenge + valueStart, valueLength);
    } else if (nameLength == 5 &&
               nsCRT::strncasecmp(challenge + nameStart, "stale", 5) == 0) {
      if (nsCRT::strncasecmp(challenge + valueStart, "true", 4) == 0) {
        *stale = true;
      } else {
        *stale = false;
      }
    } else if (nameLength == 9 &&
               nsCRT::strncasecmp(challenge + nameStart, "algorithm", 9) == 0) {
      // we want to clear the default, so we use = not |= here
      *algorithm = ALGO_SPECIFIED;
      if (valueLength == 3 &&
          nsCRT::strncasecmp(challenge + valueStart, "MD5", 3) == 0) {
        *algorithm |= ALGO_MD5;
      } else if (valueLength == 8 && nsCRT::strncasecmp(challenge + valueStart,
                                                        "MD5-sess", 8) == 0) {
        *algorithm |= ALGO_MD5_SESS;
      } else if (valueLength == 7 && nsCRT::strncasecmp(challenge + valueStart,
                                                        "SHA-256", 7) == 0) {
        *algorithm |= ALGO_SHA256;
      } else if (valueLength == 12 &&
                 nsCRT::strncasecmp(challenge + valueStart, "SHA-256-sess",
                                    12) == 0) {
        *algorithm |= ALGO_SHA256_SESS;
      }
    } else if (nameLength == 3 &&
               nsCRT::strncasecmp(challenge + nameStart, "qop", 3) == 0) {
      int32_t ipos = valueStart;
      while (ipos < valueStart + valueLength) {
        while (
            ipos < valueStart + valueLength &&
            (nsCRT::IsAsciiSpace(challenge[ipos]) || challenge[ipos] == ',')) {
          ipos++;
        }
        int32_t algostart = ipos;
        while (ipos < valueStart + valueLength &&
               !nsCRT::IsAsciiSpace(challenge[ipos]) &&
               challenge[ipos] != ',') {
          ipos++;
        }
        if ((ipos - algostart) == 4 &&
            nsCRT::strncasecmp(challenge + algostart, "auth", 4) == 0) {
          *qop |= QOP_AUTH;
        } else if ((ipos - algostart) == 8 &&
                   nsCRT::strncasecmp(challenge + algostart, "auth-int", 8) ==
                       0) {
          *qop |= QOP_AUTH_INT;
        }
      }
    }
  }
  return NS_OK;
}

nsresult nsHttpDigestAuth::AppendQuotedString(const nsACString& value,
                                              nsACString& aHeaderLine) {
  nsAutoCString quoted;
  nsACString::const_iterator s, e;
  value.BeginReading(s);
  value.EndReading(e);

  //
  // Encode string according to RFC 2616 quoted-string production
  //
  quoted.Append('"');
  for (; s != e; ++s) {
    //
    // CTL = <any US-ASCII control character (octets 0 - 31) and DEL (127)>
    //
    if (*s <= 31 || *s == 127) {
      return NS_ERROR_FAILURE;
    }

    // Escape two syntactically significant characters
    if (*s == '"' || *s == '\\') {
      quoted.Append('\\');
    }

    quoted.Append(*s);
  }
  // FIXME: bug 41489
  // We should RFC2047-encode non-Latin-1 values according to spec
  quoted.Append('"');
  aHeaderLine.Append(quoted);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla

// vim: ts=2 sw=2
