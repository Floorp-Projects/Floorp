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

#ifndef nsTransferable_h__
#define nsTransferable_h__

#include "nsIFormatConverter.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"


class nsVoidArray;
class nsString;
class nsDataObj;
class nsVoidArray;


/**
 * XP Transferable wrapper
 */

class nsTransferable : public nsITransferable
{
public:

  nsTransferable();
  virtual ~nsTransferable();

    // nsISupports
  NS_DECL_ISUPPORTS
  
    // nsITransferable
  NS_IMETHOD FlavorsTransferableCanExport(nsISupportsArray **_retval) ;
  NS_IMETHOD GetTransferData(const char *aFlavor, nsISupports **aData, PRUint32 *aDataLen);
  NS_IMETHOD GetAnyTransferData(char **aFlavor, nsISupports **aData, PRUint32 *aDataLen);
  NS_IMETHOD IsLargeDataSet(PRBool *_retval);
  NS_IMETHOD FlavorsTransferableCanImport(nsISupportsArray **_retval) ;
  NS_IMETHOD SetTransferData(const char *aFlavor, nsISupports *aData, PRUint32 aDataLen);
  NS_IMETHOD AddDataFlavor(const char *aDataFlavor) ;
  NS_IMETHOD RemoveDataFlavor(const char *aDataFlavor) ;
  NS_IMETHOD GetConverter(nsIFormatConverter * *aConverter);
  NS_IMETHOD SetConverter(nsIFormatConverter * aConverter);

protected:

    // get flavors w/out converter
  NS_IMETHOD GetTransferDataFlavors(nsISupportsArray** aDataFlavorList);
 
  nsVoidArray * mDataArray;
  nsCOMPtr<nsIFormatConverter> mFormatConv;

};

#endif // nsTransferable_h__
