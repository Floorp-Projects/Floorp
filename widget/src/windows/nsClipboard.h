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

  // nsIClipboard  
  NS_IMETHOD ForceDataToClipboard ( PRInt32 aWhichClipboard );
  NS_IMETHOD HasDataMatchingFlavors(nsISupportsArray *aFlavorList, PRInt32 aWhichClipboard, PRBool *_retval); 

  // Internal Native Routines
  static nsresult CreateNativeDataObject(nsITransferable * aTransferable, 
                                         IDataObject ** aDataObj);
  static nsresult SetupNativeDataObject(nsITransferable * aTransferable, 
                                        IDataObject * aDataObj);
  static nsresult GetDataFromDataObject(IDataObject     * aDataObject,
                                        UINT              anIndex,
                                        nsIWidget       * aWindow,
                                        nsITransferable * aTransferable);
  static nsresult GetNativeDataOffClipboard(nsIWidget * aWindow, UINT aIndex, UINT aFormat, void ** aData, PRUint32 * aLen);
  static nsresult GetNativeDataOffClipboard(IDataObject * aDataObject, UINT aIndex, UINT aFormat, void ** aData, PRUint32 * aLen);
  static nsresult GetGlobalData(HGLOBAL aHGBL, void ** aData, PRUint32 * aLen);
  static UINT     GetFormat(const char* aMimeStr);

protected:
  NS_IMETHOD SetNativeClipboardData ( PRInt32 aWhichClipboard );
  NS_IMETHOD GetNativeClipboardData ( nsITransferable * aTransferable, PRInt32 aWhichClipboard );

  static PRBool IsInternetShortcut ( const char* inFileName ) ;
  static PRBool FindURLFromLocalFile ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen ) ;
  static PRBool FindUnicodeFromPlainText ( IDataObject* inDataObject, UINT inIndex, void** outData, PRUint32* outDataLen ) ;
  static void ResolveShortcut ( const char* inFileName, char** outURL ) ;

  nsIWidget         * mWindow;

};

#define SET_FORMATETC(fe, cf, td, asp, li, med)   \
	 {\
	 (fe).cfFormat=cf;\
	 (fe).ptd=td;\
	 (fe).dwAspect=asp;\
	 (fe).lindex=li;\
	 (fe).tymed=med;\
	 }


#endif // nsClipboard_h__
