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
 *
 * keydbt.h - private data structures for the private key library
 *
 * $Id: keydbt.h,v 1.3 2001/09/20 21:05:49 relyea%netscape.com Exp $
 */

#ifndef _KEYDBT_H_
#define _KEYDBT_H_

#include "prtypes.h"
#include "plarena.h"
#include "secitem.h"
#include "secasn1t.h"
#include "secmodt.h"
#include "pkcs11t.h"


/*
 * a key in/for the data base
 */
struct SECKEYDBKeyStr {
    PLArenaPool *arena;
    int version;
    char *nickname;
    SECItem salt;
    SECItem derPK;
};
typedef struct SECKEYDBKeyStr SECKEYDBKey;

typedef struct SECKEYKeyDBHandleStr SECKEYKeyDBHandle;

#define PRIVATE_KEY_DB_FILE_VERSION 3

#define SEC_PRIVATE_KEY_VERSION			0	/* what we *create* */

/*
** Typedef for callback to get a password "key".
*/
typedef SECItem * (* SECKEYLowGetPasswordKey)(void *arg,
					   SECKEYKeyDBHandle *handle);

extern const SEC_ASN1Template SECKEY_LowPQGParamsTemplate[];
extern const SEC_ASN1Template SECKEY_LowRSAPrivateKeyTemplate[];
extern const SEC_ASN1Template SECKEY_LowDSAPrivateKeyTemplate[];
extern const SEC_ASN1Template SECKEY_LowDSAPrivateKeyExportTemplate[];
extern const SEC_ASN1Template SECKEY_LowDHPrivateKeyTemplate[];
extern const SEC_ASN1Template SECKEY_LowDHPrivateKeyExportTemplate[];

extern const SEC_ASN1Template SECKEY_EncryptedPrivateKeyInfoTemplate[];
extern const SEC_ASN1Template SECKEY_AttributeTemplate[];

/* These functions simply return the address of the above-declared templates.
** This is necessary for Windows DLLs.  Sigh.
*/
extern SEC_ASN1TemplateChooser NSS_Get_SECKEY_EncryptedPrivateKeyInfoTemplate;

#endif /* _KEYDBT_H_ */
