/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef _KEYTLOW_H_
#define _KEYTLOW_H_ 1

#include "blapit.h"

typedef enum { 
    nullKey = 0, 
    rsaKey = 1, 
    dsaKey = 2, 
    fortezzaKey = 3,
    dhKey = 4, 
    keaKey = 5
} KeyType;

struct FortezzaPublicKeyStr {
    int      KEAversion;
    int      DSSversion;
    unsigned char    KMID[8];
    SECItem clearance;
    SECItem KEApriviledge;
    SECItem DSSpriviledge;
    SECItem KEAKey;
    SECItem DSSKey;
    PQGParams params;
    PQGParams keaParams;
};
typedef struct FortezzaPublicKeyStr FortezzaPublicKey;

struct FortezzaPrivateKeyStr {
     int certificate;
     unsigned char serial[8];
     int socket;
};
typedef struct FortezzaPrivateKeyStr FortezzaPrivateKey;

/*
** An RSA public key object.
*/
struct SECKEYLowPublicKeyStr {
    PLArenaPool *arena;
    KeyType keyType ;
    union {
        RSAPublicKey rsa;
	DSAPublicKey dsa;
	DHPublicKey  dh;
    } u;
};
typedef struct SECKEYLowPublicKeyStr SECKEYLowPublicKey;

/*
** Low Level private key object
** This is only used by the raw Crypto engines (crypto), keydb (keydb),
** and PKCS #11. Everyone else uses the high level key structure.
*/
struct SECKEYLowPrivateKeyStr {
    PLArenaPool *arena;
    KeyType keyType;
    union {
        RSAPrivateKey rsa;
	DSAPrivateKey dsa;
	DHPrivateKey  dh;
        FortezzaPrivateKey fortezza; /* includes DSA and KEA private
                                        keys used with fortezza      */
    } u;
};
typedef struct SECKEYLowPrivateKeyStr SECKEYLowPrivateKey;

#endif	/* _KEYTLOW_H_ */
