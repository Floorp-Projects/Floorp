/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Terry Hayes <thayes@netscape.com>
 */

#ifndef __NS_PK11TOKENDB_H__
#define __NS_PK11TOKENDB_H__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsIPK11TokenDB.h"
#include "nsIPK11Token.h"
#include "nsISupportsArray.h"
#include "nsNSSHelper.h"
#include "pk11func.h"
#include "nsNSSShutDown.h"

class nsPK11Token : public nsIPK11Token,
                    public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPK11TOKEN

  nsPK11Token(PK11SlotInfo *slot);
  virtual ~nsPK11Token();
  /* additional members */

private:
  friend class nsPK11TokenDB;

  nsString mTokenName;
  nsString mTokenLabel, mTokenManID, mTokenHWVersion, mTokenFWVersion;
  nsString mTokenSerialNum;
  PK11SlotInfo *mSlot;
  nsCOMPtr<nsIInterfaceRequestor> mUIContext;
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

class nsPK11TokenDB : public nsIPK11TokenDB
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPK11TOKENDB

  nsPK11TokenDB();
  virtual ~nsPK11TokenDB();
  /* additional members */
};

#define NS_PK11TOKENDB_CID \
{ 0xb084a2ce, 0x1dd1, 0x11b2, \
  { 0xbf, 0x10, 0x83, 0x24, 0xf8, 0xe0, 0x65, 0xcc }}

#endif
