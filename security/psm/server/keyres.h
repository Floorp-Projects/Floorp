/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#ifndef _KEYRES_H_
#define _KEYRES_H_
#include "resource.h"
#include "key.h"
#include "ctrlconn.h"

/*
 * FUNCTION: SSM_GenerateKeyPairs
 * INPUTS:
 *    msg
 *        The message sent by the client requesting the key generation.
 *    destID
 *        A pointer to pre-allocated memory where the funciton can place
 *        a copy of the resource id to associate with the newly generated
 *        key pair.
 * NOTES:
 * This function takes a key pair generation method message and performs the
 * actual key generation.
 *
 * RETURN:
 * PRSuccess if the key generation is successfull.  Any other return value 
 * indicates an error while trying to generate the key pair.
 */
extern SSMStatus SSM_GenerateKeyPair(SECItem *msg, SSMResourceID *destID);

/* Forward declare this structure so that builds will work.
 * Unfortunately, there is a circular dependency here.
 */
typedef struct SSMKeyGenContext SSMKeyGenContext;

/* Now we begin to define the KeyPair class */
typedef struct SSMKeyPair {
    SSMResource super;

    SECKEYPublicKey  *m_PubKey;
    SECKEYPrivateKey *m_PrivKey;
    SSMKeyGenContext *m_KeyGenContext;
    SSMKeyGenType     m_KeyGenType;
} SSMKeyPair;

/*
 * This structure should be passed as the argument when creating
 * a key pair.
 */

typedef struct SSMKeyPairArgStr {
    SSMKeyGenContext *keyGenContext;
} SSMKeyPairArg;

/*
 * FUNCTION: SSMKeyPair_Create
 * INPUTS:
 *    arg
 *        An opaque pointer that gets passed while creation is taking place
 *        within the resource code.
 *    res
 *        A pointer to a pointer where a copy of the pointer to the instatiated
 *        class can be placed.
 * NOTES:
 * This is the constructor "method" the resource builder will call when 
 * instatiating a key pair class.
 *
 * RETURN:
 * PRSuccess if creation was successful.  In this case, a valid pointer
 * to an SSMResource will be placed at *res. 
 * Any other return value indicates failure and the value at *res should
 * be ignored. 
 */
extern SSMStatus SSMKeyPair_Create(void *arg, SSMControlConnection * conn, 
                                  SSMResource **res);

extern SSMStatus SSMKeyPair_Init(SSMKeyPair           *inKeyPair, 
                                SSMControlConnection *conn,
                                SSMResourceType       type,
                                SSMKeyGenContext     *keyGenContext);

extern SSMStatus 
SSMKeyPair_SetAttr(SSMResource *res,
                   SSMAttributeID attrID,
                   SSMAttributeValue *value);

extern SSMStatus SSMKeyPair_Destroy(SSMResource *res, PRBool doFree);

#endif /* _KEYRES_H_ */
