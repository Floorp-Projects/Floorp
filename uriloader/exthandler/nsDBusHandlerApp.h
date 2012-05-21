/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsDBusHandlerAppImpl_h__
#define __nsDBusHandlerAppImpl_h__

#include "nsString.h"
#include "nsIMIMEInfo.h"

class nsDBusHandlerApp : public nsIDBusHandlerApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP
  NS_DECL_NSIDBUSHANDLERAPP

  nsDBusHandlerApp() { }

  virtual ~nsDBusHandlerApp() { }

protected:
  nsString mName;
  nsString mDetailedDescription;
  nsCString mService;
  nsCString mMethod;
  nsCString mInterface;
  nsCString mObjpath;

};
#endif
