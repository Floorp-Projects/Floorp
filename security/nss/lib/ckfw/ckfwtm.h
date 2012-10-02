/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CKFWTM_H
#define CKFWTM_H

#ifdef DEBUG
static const char CKFWTM_CVS_ID[] = "@(#) $RCSfile: ckfwtm.h,v $ $Revision: 1.4 $ $Date: 2012/04/25 14:49:28 $";
#endif /* DEBUG */

/*
 * ckfwtm.h
 *
 * This file declares the module-private types of the NSS Cryptoki Framework.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

struct nssCKFWHashStr;
typedef struct nssCKFWHashStr nssCKFWHash;

typedef void (PR_CALLBACK *nssCKFWHashIterator)(const void *key, void *value, void *closure);

#endif /* CKFWTM_H */
