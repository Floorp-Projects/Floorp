/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSCERTPICKER_H_
#define _NSCERTPICKER_H_

#include "nsIUserCertPicker.h"
#include "nsNSSShutDown.h"

#define NS_CERT_PICKER_CID \
  { 0x735959a1, 0xaf01, 0x447e, { 0xb0, 0x2d, 0x56, 0xe9, 0x68, 0xfa, 0x52, 0xb4 } }

class nsCertPicker : public nsIUserCertPicker
                   , public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIUSERCERTPICKER

  nsCertPicker();

  // Nothing to actually release.
  virtual void virtualDestroyNSSReference() override {}

protected:
  virtual ~nsCertPicker();
};

#endif //_NSCERTPICKER_H_
