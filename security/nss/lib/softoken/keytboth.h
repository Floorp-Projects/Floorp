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
#ifndef _KEYTBOTH_H_
#define _KEYTBOTH_H_ 1

#include "blapit.h"
#include "secoidt.h"

/*
** Attributes
*/
struct SECKEYAttributeStr {
    SECItem attrType;
    SECItem **attrValue;
};
typedef struct SECKEYAttributeStr SECKEYAttribute;

/*
** A PKCS#8 private key info object
*/
struct SECKEYPrivateKeyInfoStr {
    PLArenaPool *arena;
    SECItem version;
    SECAlgorithmID algorithm;
    SECItem privateKey;
    SECKEYAttribute **attributes;
};
typedef struct SECKEYPrivateKeyInfoStr SECKEYPrivateKeyInfo;
#define SEC_PRIVATE_KEY_INFO_VERSION		0	/* what we *create* */

/*
** A PKCS#8 private key info object
*/
struct SECKEYEncryptedPrivateKeyInfoStr {
    PLArenaPool *arena;
    SECAlgorithmID algorithm;
    SECItem encryptedData;
};
typedef struct SECKEYEncryptedPrivateKeyInfoStr SECKEYEncryptedPrivateKeyInfo;


struct DiffPQGParamsStr {
    PQGParams DiffKEAParams;
    PQGParams DiffDSAParams;
};
typedef struct DiffPQGParamsStr DiffPQGParams;

struct PQGDualParamsStr {
    PQGParams CommParams;
    DiffPQGParams DiffParams;
};
typedef struct PQGDualParamsStr PQGDualParams;


struct KEAParamsStr {
    PLArenaPool *arena;
    SECItem hash;
};
typedef struct KEAParamsStr KEAParams;
 
struct KEAPublicKeyStr {
    KEAParams params;
    SECItem publicValue;
};
typedef struct KEAPublicKeyStr KEAPublicKey;



#endif /* _KEYT_H_ */
