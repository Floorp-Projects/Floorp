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
#ifndef _KEYTHI_H_
#define _KEYTHI_H_ 1

#include "keytlow.h"
#include "keytboth.h"
#include "plarena.h"
#include "pkcs11t.h"
#include "secmodt.h"
#include "prclist.h"

/*
** A Generic  public key object.
*/
struct SECKEYPublicKeyStr {
    PLArenaPool *arena;
    KeyType keyType;
    PK11SlotInfo *pkcs11Slot;
    CK_OBJECT_HANDLE pkcs11ID;
    union {
        RSAPublicKey rsa;
	DSAPublicKey dsa;
	DHPublicKey  dh;
        KEAPublicKey kea;
        FortezzaPublicKey fortezza;
    } u;
};
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;

/*
** A generic key structure
*/ 
struct SECKEYPrivateKeyStr {
    PLArenaPool *arena;
    KeyType keyType;
    PK11SlotInfo *pkcs11Slot;	/* pkcs11 slot this key lives in */
    CK_OBJECT_HANDLE pkcs11ID;  /* ID of pkcs11 object */
    PRBool pkcs11IsTemp;	/* temp pkcs11 object, delete it when done */
    void *wincx;		/* context for errors and pw prompts */
};
typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;

/* Despite the name, this struct isn't used by any pkcs5 code.
** It's used by pkcs7 and pkcs12 code.
*/
typedef struct {
    SECItem *pwitem;
    PK11SymKey *key;
    PK11SlotInfo *slot;
    void *wincx;
} SEC_PKCS5KeyAndPassword;

typedef struct {
    PRCList links;
    SECKEYPrivateKey *key;
} SECKEYPrivateKeyListNode;

typedef struct {
    PRCList list;
    PRArenaPool *arena;
} SECKEYPrivateKeyList;

#endif /* _KEYTHI_H_ */
