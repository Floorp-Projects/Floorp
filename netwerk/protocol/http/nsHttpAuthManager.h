/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpAuthManager_h__
#define nsHttpAuthManager_h__

#include "nsIHttpAuthManager.h"

namespace mozilla {
namespace net {

class nsHttpAuthCache;

class nsHttpAuthManager : public nsIHttpAuthManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPAUTHMANAGER

  nsHttpAuthManager();
  nsresult Init();

protected:
  virtual ~nsHttpAuthManager();

  nsHttpAuthCache *mAuthCache;
  nsHttpAuthCache *mPrivateAuthCache;
};

} // namespace net
} // namespace mozilla

#endif // nsHttpAuthManager_h__
