/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

/**
 * Finish the XPCOM glue after it is no longer needed.
 */
extern "C" NS_HIDDEN_(nsresult)
XPCOMGlueShutdown();

#endif // XPCOM_GLUE
#endif // nsXPCOMGlue_h__
