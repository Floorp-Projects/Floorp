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

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsBaseClipboard.h"
#include <windows.h>

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;
struct IDataObject;

/**
 * Native Win32 Clipboard wrapper
 */

class nsClipboard : public nsBaseClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  //nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  

  // nsIClipboard  
  NS_IMETHOD ForceDataToClipboard();

  // Internal Native Routines
  static nsresult CreateNativeDataObject(nsITransferable * aTransferable, 
                                         IDataObject ** aDataObj);

  static nsresult GetDataFromDataObject(IDataObject     * aDataObject, 
                                        nsIWidget       * aWindow,
                                        nsITransferable * aTransferable);

  static nsresult GetNativeDataOffClipboard(nsIWidget * aWindow, UINT aFormat, void ** aData, PRUint32 * aLen);

  static nsresult GetNativeDataOffClipboard(IDataObject * aDataObject, UINT aFormat, void ** aData, PRUint32 * aLen);

  static nsresult GetGlobalData(HGLOBAL aHGBL, void ** aData, PRUint32 * aLen);

  static UINT     GetFormat(const nsString & aMimeStr);


protected:
  NS_IMETHOD SetNativeClipboardData();
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable);

  nsIWidget         * mWindow;
  IDataObject       * mDataObj;

};

#endif // nsClipboard_h__
