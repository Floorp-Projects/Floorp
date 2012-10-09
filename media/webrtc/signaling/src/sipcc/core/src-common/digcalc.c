/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "cpr_string.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "md5.h"
#include "digcalc.h"
#include "phone_debug.h"


void
CvtHex (IN HASH Bin, OUT HASHHEX Hex)
{
    unsigned short i;
    unsigned char j;

    for (i = 0; i < HASHLEN; i++) {
        j = (unsigned char) ((Bin[i] >> 4) & 0xf);
        if (j <= 9)
            Hex[i * 2] = (j + '0');
        else
            Hex[i * 2] = (j + 'a' - 10);
        j = Bin[i] & 0xf;
        if (j <= 9)
            Hex[i * 2 + 1] = (j + '0');
        else
            Hex[i * 2 + 1] = (j + 'a' - 10);
    };
    Hex[HASHHEXLEN] = '\0';
}

/* calculate H(A1) as per spec */
void
DigestCalcHA1 (IN char *pszAlg,
               IN char *pszUserName,
               IN char *pszRealm,
               IN char *pszPassword,
               IN char *pszNonce,
               IN char *pszCNonce,
               OUT HASHHEX SessionKey)
{
    MD5_CTX Md5Ctx;
    HASH HA1;
    HASHHEX HA1Hex;

    MD5Init(&Md5Ctx);
    MD5Update(&Md5Ctx, (unsigned char *) pszUserName, strlen(pszUserName));
    MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
    MD5Update(&Md5Ctx, (unsigned char *) pszRealm, strlen(pszRealm));
    MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
    MD5Update(&Md5Ctx, (unsigned char *) pszPassword, strlen(pszPassword));
    MD5Final((unsigned char *) HA1, &Md5Ctx);
    if (cpr_strcasecmp(pszAlg, "md5-sess") == 0) {
        MD5Init(&Md5Ctx);
        CvtHex(HA1, HA1Hex);
        MD5Update(&Md5Ctx, (unsigned char *) HA1Hex, HASHHEXLEN);
        MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
        MD5Update(&Md5Ctx, (unsigned char *) pszNonce, strlen(pszNonce));
        MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
        MD5Update(&Md5Ctx, (unsigned char *) pszCNonce, strlen(pszCNonce));
        MD5Final((unsigned char *) HA1, &Md5Ctx);
    };

    CvtHex(HA1, SessionKey);
}

/* calculate request-digest/response-digest as per HTTP Digest spec */
void
DigestCalcResponse (IN HASHHEX HA1,       /* H(A1) */
                    IN char *pszNonce,    /* nonce from server */
                    IN char *pszNonceCount,/* 8 hex digits */
                    IN char *pszCNonce,   /* client nonce */
                    IN char *pszQop,      /* qop-value: "", "auth", "auth-int"*/
                    IN char *pszMethod,   /* method from the request */
                    IN char *pszDigestUri,/* requested URL */
                    IN HASHHEX HEntity,   /* H(entity body) if qop="auth-int" */
                    OUT HASHHEX Response) /* request-digest or response-digest */
{
    MD5_CTX Md5Ctx;
    HASH HA2;
    HASH RespHash;
    HASHHEX HA2Hex;
    static const char fname[] = "DigestCalcResponse";

    // calculate H(A2)
    MD5Init(&Md5Ctx);
    MD5Update(&Md5Ctx, (unsigned char *) pszMethod, strlen(pszMethod));
    MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
    MD5Update(&Md5Ctx, (unsigned char *) pszDigestUri, strlen(pszDigestUri));
    
/*  Commented as causes problems on Windows ToDo
 * qop is not used but this should be fixed    
    if (cpr_strcasecmp(pszQop, "auth-int") == 0) {
        MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
        MD5Update(&Md5Ctx, (unsigned char *) HEntity, HASHHEXLEN);
    };
*/     
    MD5Final((unsigned char *) HA2, &Md5Ctx);

    CvtHex(HA2, HA2Hex);

    // calculate response
    MD5Init(&Md5Ctx);
    MD5Update(&Md5Ctx, (unsigned char *) HA1, HASHHEXLEN);
    MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
    MD5Update(&Md5Ctx, (unsigned char *) pszNonce, strlen(pszNonce));
    MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
    if (pszQop /* && *pszQop */) {
        MD5Update(&Md5Ctx, (unsigned char *) pszNonceCount,
                  strlen(pszNonceCount));
        MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
        MD5Update(&Md5Ctx, (unsigned char *) pszCNonce, strlen(pszCNonce));
        MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
        MD5Update(&Md5Ctx, (unsigned char *) pszQop, strlen(pszQop));
        MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
    };
    MD5Update(&Md5Ctx, (unsigned char *) HA2Hex, HASHHEXLEN);
    MD5Final((unsigned char *) RespHash, &Md5Ctx);

    CvtHex(RespHash, Response);

    AUTH_DEBUG(DEB_F_PREFIX"HA1=     %s\n", DEB_F_PREFIX_ARGS(SIP_REQ_DIGEST, fname), HA1);
    AUTH_DEBUG(DEB_F_PREFIX"HEntity= %s\n", DEB_F_PREFIX_ARGS(SIP_REQ_DIGEST, fname), HEntity);
    AUTH_DEBUG(DEB_F_PREFIX"HA2=     %s\n", DEB_F_PREFIX_ARGS(SIP_REQ_DIGEST, fname), HA2Hex);
    AUTH_DEBUG(DEB_F_PREFIX"Digest=  %s\n", DEB_F_PREFIX_ARGS(SIP_REQ_DIGEST, fname), Response);
}


void
DigestString (char *string, HASHHEX response)
{
    MD5_CTX Md5Ctx;
    HASH hash;
    unsigned int len = strlen(string);

    MD5Init(&Md5Ctx);
    MD5Update(&Md5Ctx, (unsigned char *)string, len);
    MD5Final((unsigned char *)hash, &Md5Ctx);

    CvtHex(hash, response);
}

