/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _NSPKIPARAMBLOCK_
#define _NSPKIPARAMBLOCK_
#include "nsCOMPtr.h"
#include "nsIPKIParamBlock.h"
#include "nsIDialogParamBlock.h"
#include "nsISupportsArray.h"

#define NS_PKIPARAMBLOCK_CID \
  { 0x0bec75a8, 0x1dd2, 0x11b2, \
    { 0x86, 0x3a, 0xf6, 0x9f, 0x77, 0xc3, 0x13, 0x71 }}

#define NS_PKIPARAMBLOCK_CONTRACTID "@mozilla.org/security/pkiparamblock;1"

class nsPKIParamBlock : public nsIPKIParamBlock,
                        public nsIDialogParamBlock
{
public:
 
  nsPKIParamBlock();
  nsresult Init();

  NS_DECL_NSIPKIPARAMBLOCK
  NS_DECL_NSIDIALOGPARAMBLOCK
  NS_DECL_THREADSAFE_ISUPPORTS
private:
  virtual ~nsPKIParamBlock();
  nsCOMPtr<nsIDialogParamBlock> mDialogParamBlock;
  nsCOMPtr<nsISupportsArray>    mSupports;
};

#endif //_NSPKIPARAMBLOCK_
