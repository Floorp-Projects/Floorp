/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSCRLMANAGER_H_
#define _NSCRLMANAGER_H_

#include "nsICRLManager.h"

class nsCRLManager : public nsICRLManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRLMANAGER
  
  nsCRLManager();
  virtual ~nsCRLManager();
};

#endif
