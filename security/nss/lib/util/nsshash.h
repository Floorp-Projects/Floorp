/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSSHASH_H_
#define _NSSHASH_H_

#include "hasht.h"
#include "utilrename.h"

SEC_BEGIN_PROTOS

extern HASH_HashType HASH_GetHashTypeByOidTag(SECOidTag hashOid);
extern SECOidTag HASH_GetHashOidTagByHashType(HASH_HashType type);
extern SECOidTag HASH_GetHashOidTagByHMACOidTag(SECOidTag hmacOid);
extern SECOidTag HASH_GetHMACOidTagByHashOidTag(SECOidTag hashOid);

SEC_END_PROTOS

#endif /* _NSSHASHT_H_ */
