/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"

#include "nsIVersionComparator.h"

class nsVersionComparatorImpl MOZ_FINAL : public nsIVersionComparator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVERSIONCOMPARATOR
};

#define NS_VERSIONCOMPARATOR_CONTRACTID "@mozilla.org/xpcom/version-comparator;1"

// c6e47036-ca94-4be3-963a-9abd8705f7a8
#define NS_VERSIONCOMPARATOR_CID \
{ 0xc6e47036, 0xca94, 0x4be3, \
  { 0x96, 0x3a, 0x9a, 0xbd, 0x87, 0x05, 0xf7, 0xa8 } }
