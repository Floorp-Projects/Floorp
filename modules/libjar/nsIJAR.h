/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Don Bragg <dbragg@netscape.com>
 */


#ifndef nsIJAR_h__
#define nsIJAR_h__


#include "nsJARIIDs.h"
#include "nsIZip.h"
#include "nsISupports.h"
#include "nsIFactory.h"


class nsIJAR : public nsIZip 
{
  public:
    static const nsIID& IID() { static nsIID iid = NS_IJAR_IID; return iid; }

  /* This interface will be filled with other "JAR"-related functions. */
  /* The term "JAR" is in reference to security as opposed to "Zip" 
   * which is a non-secure archive format.                             */
};


#endif // nsIJAR_h__
