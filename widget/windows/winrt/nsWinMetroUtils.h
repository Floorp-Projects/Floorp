/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsIWinMetroUtils.h"
#include "nsString.h"

namespace mozilla {
namespace widget {

class nsWinMetroUtils : public nsIWinMetroUtils
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINMETROUTILS

  nsWinMetroUtils();
  virtual ~nsWinMetroUtils();
};

} // widget
} // mozilla
