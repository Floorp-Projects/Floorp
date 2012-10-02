/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSDEVT_H
#define NSSDEVT_H

#ifdef DEBUG
static const char NSSDEVT_CVS_ID[] = "@(#) $RCSfile: nssdevt.h,v $ $Revision: 1.6 $ $Date: 2012/04/25 14:49:42 $";
#endif /* DEBUG */

/*
 * nssdevt.h
 *
 * This file contains definitions for the low-level cryptoki devices.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSPKIT_H
#include "nsspkit.h"
#endif /* NSSPKIT_H */

PR_BEGIN_EXTERN_C

/*
 * NSSModule and NSSSlot -- placeholders for the PKCS#11 types
 */

typedef struct NSSModuleStr NSSModule;

typedef struct NSSSlotStr NSSSlot;

typedef struct NSSTokenStr NSSToken;

PR_END_EXTERN_C

#endif /* NSSDEVT_H */
