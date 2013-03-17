/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIOUtil_h__
#define nsIOUtil_h__

#define NS_IOUTIL_CID                                                \
{ 0xeb833911, 0x4f49, 0x4623,                                        \
    { 0x84, 0x5f, 0xe5, 0x8a, 0x8e, 0x6d, 0xe4, 0xc2 } }


#include "nsIIOUtil.h"
#include "mozilla/Attributes.h"

class nsIOUtil MOZ_FINAL : public nsIIOUtil
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIOUTIL
};

#endif /* nsIOUtil_h__ */
