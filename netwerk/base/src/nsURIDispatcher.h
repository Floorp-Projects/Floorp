/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

#define NS_URIDISPATCHER_CID				              \
{ /* EBBBBFE1-8BE8-11d3-989D-001083010E9B */      \
 0xebbbbfe1, 0x8be8, 0x11d3,                      \
 {0x98, 0x9d, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b}}

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

