/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsDataFlavor_h__
#define nsDataFlavor_h__

#include "nsIDataFlavor.h"

class nsISupportsArray;
class nsIDataFlavor;

/**
 * DataFlavor wrapper
 */

class nsDataFlavor : public nsIDataFlavor
{

public:
  nsDataFlavor();
  virtual ~nsDataFlavor();

  //nsISupports
  NS_DECL_ISUPPORTS
  
  //nsIDataFlavor
  NS_IMETHOD Init(const nsString & aMimeType, 
                  const nsString & aHumanPresentableName);
  NS_IMETHOD GetMimeType(nsString & aMimeStr) const;
  NS_IMETHOD GetHumanPresentableName(nsString & aReadableStr) const;
  NS_IMETHOD Equals(const nsIDataFlavor * aDataFlavor);

protected:
  nsString mMimeType;
  nsString mHumanPresentableName;

};

#endif // nsDataFlavor_h__
