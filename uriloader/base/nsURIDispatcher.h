/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* In some regards this class is a temporary class. As the new web shell
   re-architecture begins to fall into place, the URIDispatcher will also
   be the doc loader and this implementation will be grated onto the doc
   loader. 

   But in the current world, the uri dispatcher will be a stand alone
   class implementation.

*/

#ifndef nsURIDispatcher_h__
#define nsURIDispatcher_h__

#include "nsIURIDispatcher.h"
#include "nsCOMPtr.h"

class nsVoidArray;

class nsURIDispatcher : public nsIURIDispatcher
{
public:
  NS_DECL_NSIURIDISPATCHER
  NS_DECL_ISUPPORTS

  nsURIDispatcher();
  virtual ~nsURIDispatcher();

protected:
  // we shouldn't need to have an owning ref count on registered
  // content listeners because they are supposed to unregister themselves
  // when they go away.
  nsVoidArray * m_listeners;

};

#endif /* nsURIDispatcher_h__ */

