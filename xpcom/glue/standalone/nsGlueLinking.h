/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGlueLinking_h__
#define nsGlueLinking_h__

#include "nsXPCOMPrivate.h"

#define XPCOM_DEPENDENT_LIBS_LIST "dependentlibs.list"

NS_HIDDEN_(nsresult)
XPCOMGlueLoad(const char *xpcomFile, GetFrozenFunctionsFunc *func);

NS_HIDDEN_(void)
XPCOMGlueUnload();

typedef void (*DependentLibsCallback)(const char *aDependentLib, bool do_preload);

NS_HIDDEN_(void)
XPCOMGlueLoadDependentLibs(const char *xpcomDir, DependentLibsCallback cb);

#endif // nsGlueLinking_h__
