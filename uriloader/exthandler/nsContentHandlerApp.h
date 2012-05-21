/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsContentHandlerAppImpl_h__
#define __nsContentHandlerAppImpl_h__

#include <contentaction/contentaction.h>
#include "nsString.h"
#include "nsIMIMEInfo.h"

class nsContentHandlerApp : public nsIHandlerApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP

  nsContentHandlerApp(nsString aName, nsCString aType, ContentAction::Action& aAction);
  virtual ~nsContentHandlerApp() { }

protected:
  nsString mName;
  nsCString mType;
  nsString mDetailedDescription;

  ContentAction::Action mAction;
};
#endif
