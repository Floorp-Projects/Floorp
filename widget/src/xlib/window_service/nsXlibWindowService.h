/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C) 1999
 * Christopher Blizzard.  All Rights Reserved.
 */

#include "nsIXlibWindowService.h"

class nsXlibWindowService : public nsIXlibWindowService
{
 public:
  nsXlibWindowService();
  virtual ~nsXlibWindowService();

  NS_DECL_ISUPPORTS

//   NS_IMETHOD SetCreateCallback(nsXlibWindowCallback *aFunc);
//   NS_IMETHOD DispatchNativeXlibEvent(void *aNativeEvent);

  NS_IMETHOD SetWindowCreateCallback(nsXlibWindowCallback aCallback);
  NS_IMETHOD SetWindowDestroyCallback(nsXlibWindowCallback aCallback);
  NS_IMETHOD GetWindowCreateCallback(nsXlibWindowCallback * aCallbackOut);
  NS_IMETHOD GetWindowDestroyCallback(nsXlibWindowCallback * aCallbackOut);


  NS_IMETHOD SetEventDispatcher(nsXlibEventDispatcher aDispatcher);
  NS_IMETHOD GetEventDispatcher(nsXlibEventDispatcher * aDispatcherOut);

  NS_IMETHOD SetTimeToNextTimeoutFunc(nsXlibTimeToNextTimeoutFunc aFunc);
  NS_IMETHOD GetTimeToNextTimeoutFunc(nsXlibTimeToNextTimeoutFunc * aFuncOut);

  NS_IMETHOD SetProcessTimeoutsProc(nsXlibProcessTimeoutsProc aProc);
  NS_IMETHOD GetProcessTimeoutsProc(nsXlibProcessTimeoutsProc * aProcOut);

private:

  static nsXlibWindowCallback         gsWindowCreateCallback;
  static nsXlibWindowCallback         gsWindowDestroyCallback;
  static nsXlibEventDispatcher        gsEventDispatcher;
  static nsXlibTimeToNextTimeoutFunc  gsTimeToNextTimeoutFunc;
  static nsXlibProcessTimeoutsProc    gsProcessTimeoutsProc;
};
