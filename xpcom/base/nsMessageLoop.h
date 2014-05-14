/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMessageLoop.h"

/*
 * nsMessageLoop implements nsIMessageLoop, which wraps Chromium's MessageLoop
 * class and adds a bit of sugar.
 */
class nsMessageLoop : public nsIMessageLoop
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMESSAGELOOP

  virtual ~nsMessageLoop()
  {
  }
};

#define NS_MESSAGE_LOOP_CID \
{0x67b3ac0c, 0xd806, 0x4d48, \
{0x93, 0x9e, 0x6a, 0x81, 0x9e, 0x6c, 0x24, 0x8f}}

extern nsresult
nsMessageLoopConstructor(nsISupports* aOuter,
                         const nsIID& aIID,
                         void** aInstancePtr);
