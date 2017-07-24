/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tools_codecoverage_nscodecoverage_h
#define tools_codecoverage_nscodecoverage_h

#include "nsICodeCoverage.h"

#define NS_CODECOVERAGE_CID \
{ 0x93576af0, 0xa62f, 0x4c88, \
{ 0xbc, 0x12, 0xf1, 0x85, 0x5d, 0x4e, 0x01, 0x73 } }

class nsCodeCoverage final : nsICodeCoverage {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICODECOVERAGE

  nsCodeCoverage();

private:
  ~nsCodeCoverage();
};

#endif // tools_codecoverage_nscodecoverage_h
