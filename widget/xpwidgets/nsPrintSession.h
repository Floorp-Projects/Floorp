/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSession_h__
#define nsPrintSession_h__

#include "nsIPrintSession.h" 
#include "nsWeakReference.h"
#include "gfxCore.h"

//*****************************************************************************
//***    nsPrintSession
//*****************************************************************************

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_DEFAULT

class nsPrintSession : public nsIPrintSession,
                       public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTSESSION

  nsPrintSession();
  virtual ~nsPrintSession();
  
  virtual nsresult Init();
};

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

#endif // nsPrintSession_h__
