/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
  NS_IMETHOD GetPredefinedDataFlavor(nsString & aStr, 
                        nsIDataFlavor ** aDataFlavor);

protected:
  nsString mMimeType;
  nsString mHumanPresentableName;

};

#endif // nsDataFlavor_h__
