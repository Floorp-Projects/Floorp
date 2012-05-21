/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlueLinking.h"

#include <stdio.h>

nsresult
XPCOMGlueLoad(const char *xpcomFile, GetFrozenFunctionsFunc *func)
{
    fprintf(stderr, "XPCOM glue dynamic linking is not implemented on this platform!");

    *func = nsnull;

    return NS_ERROR_NOT_IMPLEMENTED;
}

void
XPCOMGlueUnload()
{
}
