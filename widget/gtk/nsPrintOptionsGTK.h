/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintOptionsGTK_h__
#define nsPrintOptionsGTK_h__

#include "nsPrintOptionsImpl.h"  


//*****************************************************************************
//***    nsPrintOptions
//*****************************************************************************
class nsPrintOptionsGTK : public nsPrintOptions
{
public:
  nsPrintOptionsGTK();
  virtual ~nsPrintOptionsGTK();

  virtual nsresult _CreatePrintSettings(nsIPrintSettings **_retval);

};



#endif /* nsPrintOptions_h__ */
