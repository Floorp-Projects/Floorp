/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPCOMGlue_h__
#define nsXPCOMGlue_h__

#include "nscore.h"

#ifdef XPCOM_GLUE

/**
 * The following functions are only available in the standalone glue.
 */

/**
 * Enabled preloading of dynamically loaded libraries
 */
extern "C" NS_HIDDEN_(void)
XPCOMGlueEnablePreload();

/**
 * Initialize the XPCOM glue by dynamically linking against the XPCOM
 * shared library indicated by xpcomFile.
 */
extern "C" NS_HIDDEN_(nsresult)
XPCOMGlueStartup(const char* xpcomFile);

typedef void (*NSFuncPtr)();

struct nsDynamicFunctionLoad
{
    const char *functionName;
    NSFuncPtr  *function;
};

/**
 * Dynamically load functions from libxul.
 *
 * @throws NS_ERROR_NOT_INITIALIZED if XPCOMGlueStartup() was not called or
 *         if the libxul DLL was not found.
 * @throws NS_ERROR_LOSS_OF_SIGNIFICANT_DATA if only some of the required
 *         functions were found.
 */
extern "C" NS_HIDDEN_(nsresult)
XPCOMGlueLoadXULFunctions(const nsDynamicFunctionLoad *symbols);

#endif // XPCOM_GLUE
#endif // nsXPCOMGlue_h__
