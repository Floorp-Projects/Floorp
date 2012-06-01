/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include <string.h>

PR_IMPLEMENT(PRIntn)
PL_strcmp(const char *a, const char *b)
{
    if( ((const char *)0 == a) || (const char *)0 == b ) 
        return (PRIntn)(a-b);

    return (PRIntn)strcmp(a, b);
}

PR_IMPLEMENT(PRIntn)
PL_strncmp(const char *a, const char *b, PRUint32 max)
{
    if( ((const char *)0 == a) || (const char *)0 == b ) 
        return (PRIntn)(a-b);

    return (PRIntn)strncmp(a, b, (size_t)max);
}
