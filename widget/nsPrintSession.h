/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSession_h__
#define nsPrintSession_h__

#include "nsIPrintSession.h" 
#include "nsWeakReference.h"

//*****************************************************************************
//***    nsPrintSession
//*****************************************************************************

class nsPrintSession : public nsIPrintSession,
                       public nsSupportsWeakReference
{
  virtual ~nsPrintSession();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTSESSION

  nsPrintSession();
  
  virtual nsresult Init();
};

#endif // nsPrintSession_h__
