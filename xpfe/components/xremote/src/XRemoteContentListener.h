/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 */

#include <nsIURIContentListener.h>
#include <nsIInterfaceRequestor.h>
#include <nsCOMPtr.h>

class XRemoteContentListener : public nsIURIContentListener,
                               public nsIInterfaceRequestor
{
 public:
  XRemoteContentListener();
  virtual ~XRemoteContentListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURICONTENTLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR

 private:

  nsCOMPtr<nsISupports> mLoadCookie;
};
