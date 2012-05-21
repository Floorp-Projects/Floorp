/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPrintOptionsAndroid_h__
#define nsPrintOptionsAndroid_h__

#include "nsPrintOptionsImpl.h"
#include "nsIPrintSettings.h"

//*****************************************************************************
//***    nsPrintOptions
//*****************************************************************************
class nsPrintOptionsAndroid : public nsPrintOptions
{
public:
  nsPrintOptionsAndroid();
  virtual ~nsPrintOptionsAndroid();

  NS_IMETHOD CreatePrintSettings(nsIPrintSettings **_retval);
};

#endif /* nsPrintOptionsAndroid_h__ */
