/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_NSSVERSION_H_
#define _NS_NSSVERSION_H_

#include "nsINSSVersion.h"

class nsNSSVersion : public nsINSSVersion
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINSSVERSION
  
  nsNSSVersion();

private:
  ~nsNSSVersion();
};

#define NS_NSSVERSION_CID \
  { 0x23ad3531, 0x11d2, 0x4e8e, { 0x80, 0x5a, 0x6a, 0x75, 0x2e, 0x91, 0x68, 0x1a } }

#endif
