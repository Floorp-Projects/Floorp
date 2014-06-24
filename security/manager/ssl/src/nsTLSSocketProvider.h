/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSTLSSOCKETPROVIDER_H_
#define _NSTLSSOCKETPROVIDER_H_

#include "nsISocketProvider.h"


#define NS_STARTTLSSOCKETPROVIDER_CID \
{ /* b9507aec-1dd1-11b2-8cd5-c48ee0c50307 */         \
     0xb9507aec,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0x8c, 0xd5, 0xc4, 0x8e, 0xe0, 0xc5, 0x03, 0x07} \
}

class nsTLSSocketProvider : public nsISocketProvider
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISOCKETPROVIDER

  // nsTLSSocketProvider methods:
  nsTLSSocketProvider();

protected:
  virtual ~nsTLSSocketProvider();
};

#endif /* _NSTLSSOCKETPROVIDER_H_ */
