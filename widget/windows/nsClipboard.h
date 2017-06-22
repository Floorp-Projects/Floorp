/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsBaseClipboard.h"
#include "nsIObserver.h"
#include "nsIURI.h"
#include <windows.h>

class nsITransferable;
class nsIWidget;
class nsIFile;
struct IDataObject;

/**
 * Native Win32 Clipboard wrapper
 */

class nsClipboard : public nsBaseClipboard,
                    public nsIObserver
{
  virtual ~nsClipboard();

public:
  nsClipboard();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIObserver
  NS_DECL_NSIOBSERVER

  static already_AddRefed<nsIClipboard> GetInstance();

  // nsIClipboard
  NS_IMETHOD HasDataMatchingFlavors(const char** aFlavorList, uint32_t aLength,
                                    int32_t aWhichClipboard, bool *_retval) override; 
  NS_IMETHOD EmptyClipboard(int32_t aWhichClipboard) override;

  // Internal Native Routines
  static nsresult CreateNativeDataObject(nsITransferable * aTransferable, 
                                         IDataObject ** aDataObj,
                                         nsIURI       * uri);
  static nsresult SetupNativeDataObject(nsITransferable * aTransferable, 
                                        IDataObject * aDataObj);
  static nsresult GetDataFromDataObject(IDataObject     * aDataObject,
                                        UINT              anIndex,
                                        nsIWidget       * aWindow,
                                        nsITransferable * aTransferable);
  static nsresult GetNativeDataOffClipboard(nsIWidget * aWindow, UINT aIndex, UINT aFormat, void ** aData, uint32_t * aLen);
  static nsresult GetNativeDataOffClipboard(IDataObject * aDataObject, UINT aIndex, UINT aFormat, const char * aMIMEImageFormat, void ** aData, uint32_t * aLen);
  static nsresult GetGlobalData(HGLOBAL aHGBL, void ** aData, uint32_t * aLen);

  // This function returns the internal Windows clipboard format identifier
  // for a given Mime string. The default is to map kHTMLMime ("text/html")
  // to the clipboard format CF_HTML ("HTLM Format"), but it can also be
  // registered as clipboard format "text/html" to support previous versions
  // of Gecko.
  static UINT     GetFormat(const char* aMimeStr, bool aMapHTMLMime = true);

  static UINT     CF_HTML;
  static UINT     CF_CUSTOMTYPES;
  
protected:
  NS_IMETHOD SetNativeClipboardData ( int32_t aWhichClipboard ) override;
  NS_IMETHOD GetNativeClipboardData ( nsITransferable * aTransferable, int32_t aWhichClipboard ) override;
  
  static bool IsInternetShortcut ( const nsAString& inFileName ) ;
  static bool FindURLFromLocalFile ( IDataObject* inDataObject, UINT inIndex, void** outData, uint32_t* outDataLen ) ;
  static bool FindURLFromNativeURL ( IDataObject* inDataObject, UINT inIndex, void** outData, uint32_t* outDataLen ) ;
  static bool FindUnicodeFromPlainText ( IDataObject* inDataObject, UINT inIndex, void** outData, uint32_t* outDataLen ) ;
  static bool FindPlatformHTML ( IDataObject* inDataObject, UINT inIndex, void** outData,
                                 uint32_t* outStartOfData, uint32_t* outDataLen );
  static void ResolveShortcut ( nsIFile* inFileName, nsACString& outURL ) ;

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

