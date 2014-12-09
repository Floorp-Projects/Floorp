/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _DIGCALC_H_
#define _DIGCALC_H_

#define HASHLEN 16
typedef char HASH[HASHLEN];
#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN + 1];

#define IN
#define OUT

/* calculate H(A1) as per HTTP Digest spec */
void DigestCalcHA1(
    IN char *pszAlg,
    IN char *pszUserName,
    IN char *pszRealm,
    IN char *pszPassword,
    IN char *pszNonce,
    IN char *pszCNonce,
    OUT HASHHEX SessionKey
    );

/* calculate request-digest/response-digest as per HTTP Digest spec */
void DigestCalcResponse(
    IN HASHHEX HA1,         /* H(A1) */
    IN char *pszNonce,      /* nonce from server */
    IN char *pszNonceCount, /* 8 hex digits */
    IN char *pszCNonce,     /* client nonce */
    IN char *pszQop,        /* qop-value: "", "auth", "auth-int" */
    IN char *pszMethod,     /* method from the request */
    IN char *pszDigestUri,  /* requested URL */
    IN HASHHEX HEntity,     /* H(entity body) if qop="auth-int" */
    OUT HASHHEX Response    /* request-digest or response-digest */
    );

void DigestString(char *string, HASHHEX response);

#endif
