/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_buildparams.h
 *
 * BuildParams Object Type Definition
 *
 */

#ifndef _PKIX_BUILDPARAMS_H
#define _PKIX_BUILDPARAMS_H

#include "pkix_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PKIX_BuildParamsStruct {
        PKIX_ProcessingParams *procParams;      /* Never NULL */
};

/* see source file for function documentation */

PKIX_Error *pkix_BuildParams_RegisterSelf(void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_BUILDPARAMS_H */
