/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_error.c
 *
 * PL error functions
 *
 */

#include "pkix_pl_common.h"

#undef PKIX_ERRORENTRY

#define PKIX_ERRORENTRY(name,desc,plerr) plerr

const PKIX_Int32 PKIX_PLErrorIndex[] =
{
#include "pkix_errorstrings.h"
};

int
PKIX_PL_GetPLErrorCode()
{
    return PORT_GetError();
}
