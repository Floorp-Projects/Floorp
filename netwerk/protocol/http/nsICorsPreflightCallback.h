/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICorsPreflightCallback_h__
#define nsICorsPreflightCallback_h__

#include "nsISupports.h"
#include "nsID.h"
#include "nsError.h"

#define NS_ICORSPREFLIGHTCALLBACK_IID \
  { 0x3758cfbb, 0x259f, 0x4074, \
      { 0xa8, 0xc0, 0x98, 0xe0, 0x4b, 0x3c, 0xc0, 0xe3 } }

class nsICorsPreflightCallback : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(nsICorsPreflightCallback);
  NS_IMETHOD OnPreflightSucceeded() = 0;
  NS_IMETHOD OnPreflightFailed(nsresult aError) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICorsPreflightCallback, NS_ICORSPREFLIGHTCALLBACK_IID);

#endif
