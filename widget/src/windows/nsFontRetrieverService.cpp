/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsFontRetrieverService.h"
#include <windows.h>

#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsFontSizeIterator.h"
 
NS_IMPL_ADDREF(nsFontRetrieverService)
NS_IMPL_RELEASE(nsFontRetrieverService)


//----------------------------------------------------------
nsFontRetrieverService::nsFontRetrieverService()
{
  NS_INIT_REFCNT();

  mFontList     = nsnull;
  mSizeIter     = nsnull;
  mNameIterInx  = 0;

}

//----------------------------------------------------------
nsFontRetrieverService::~nsFontRetrieverService()
{
  if (nsnull != mFontList) {
    for (PRInt32 i=0;i<mFontList->Count();i++) {
      FontInfo * font = (FontInfo *)mFontList->ElementAt(i);
      if (font->mSizes) {
        delete font->mSizes;
      }
      delete font;
    }
    delete mFontList;
  }
  NS_IF_RELEASE(mSizeIter);
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsFontRetrieverService::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(NS_GET_IID(nsIFontRetrieverService))) {
    *aInstancePtr = (void*) ((nsIFontRetrieverService*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIFontNameIterator))) {
    *aInstancePtr = (void*) ((nsIFontNameIterator*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


//----------------------------------------------------------
//-- nsIFontRetrieverService
//----------------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::CreateFontNameIterator( nsIFontNameIterator** aIterator )
{
  if (nsnull == aIterator) {
    return NS_ERROR_FAILURE;
  }

  if (nsnull == mFontList) {
    LoadFontList();
  }
  *aIterator = this;
  NS_ADDREF_THIS();
  return NS_OK;
}

//----------------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::CreateFontSizeIterator( const nsString &aFontName, 
                                                              nsIFontSizeIterator** aIterator )
{
  // cache current value in case someone else externally is using it
  PRInt32 saveIterInx  = mNameIterInx;

  PRBool found = PR_FALSE;
  Reset();
  do {
    nsAutoString name;
    Get(&name);
    if (name.Equals(aFontName)) {
      found = PR_TRUE;
      break;
    }
  } while (Advance() == NS_OK);

  if (found) {
    if (nsnull == mSizeIter) {
      mSizeIter = new nsFontSizeIterator();
    }
    NS_ASSERTION( nsnull != mSizeIter, "nsFontSizeIterator instance pointer is null");

    *aIterator = (nsIFontSizeIterator *)mSizeIter;
    NS_ADDREF(mSizeIter);

    FontInfo * fontInfo = (FontInfo *)mFontList->ElementAt(mNameIterInx);
    mSizeIter->SetFontInfo(fontInfo);
    mNameIterInx = saveIterInx;
    return NS_OK;
  }
  mNameIterInx = saveIterInx;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------
//-- nsIFontNameIterator
//----------------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::Reset()
{
  mNameIterInx = 0;
  return NS_OK;
}

//----------------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::Get( nsString* aFontName )
{
  if (mNameIterInx < mFontList->Count()) {
    FontInfo * fontInfo = (FontInfo *)mFontList->ElementAt(mNameIterInx);
    *aFontName = fontInfo->mName;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::Advance()
{
  if (mNameIterInx < mFontList->Count()-1) {
    mNameIterInx++;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------
static int CALLBACK MyEnumFontFamProc(ENUMLOGFONT FAR *lpelf, 
                                      NEWTEXTMETRIC FAR *lpntm, 
                                      unsigned long fontType, 
                                      LPARAM lParam)
{
  FontInfo * font = (FontInfo *)lParam;

  ////printf("  sizes-> %d\n", lpelf->elfLogFont.lfHeight);
  LOGFONT  ourLogFont;    // Local copy of the LOGFONT structure.
  memcpy(&ourLogFont,  (const void *)&lpelf->elfLogFont, sizeof LOGFONT); // Make the copy
  //printf("Size: %d", ourLogFont.lfHeight);

  if (fontType & TRUETYPE_FONTTYPE) {
    //printf("TRUETYPE_FONTTYPE;");
    font->mIsScalable = PR_TRUE;
    return 1;
  } else if (fontType & RASTER_FONTTYPE) {
    //printf("RASTER_FONTTYPE;");
    if (nsnull == font->mSizes ) {
      font->mSizes = new nsVoidArray();
    }
  } else if (fontType & DEVICE_FONTTYPE) {
    //printf("DEVICE_FONTTYPE;");
    //if (nsnull == font->mSizes ) {
    //  font->mSizes = new nsVoidArray();
    //}
  } else {
    //printf("VECTOR_FONTTYPE;");
    font->mIsScalable = PR_TRUE;
  }

  if (nsnull != font->mSizes) {
    font->mSizes->AppendElement((void *)ourLogFont.lfHeight);
  }

#if 0
   if (ourLogFont.lfCharSet != ANSI_CHARSET)
      //printf("FNT_NONANSI;");
   else
      //printf("FNT_ANSI;");

   //
   // We also need to set the family type (we use it for filtering)
   //
   switch (ourLogFont.lfPitchAndFamily & 0xF0)
      {
      case FF_SWISS:
         //printf("FNT_SANSSERIF;");
         break;

      case FF_ROMAN:
         //printf("FNT_SERIF;");
         break;

      case FF_MODERN:
         //printf("FNT_MODERN;");
         break;

      case FF_SCRIPT:
         //printf("FNT_SCRIPT;");
         break;

      default:
         //printf("FNT_OTHER;"); 
         break;
      }
#endif
  //printf("\n");

  return 1;
}

//--------------------------------------------------------------------- 
// 
// int CALLBACK EnumerateMyFonts 
// 
// This function is called as a result of the call to EnumFontFamiliesEx 
// in CFViewerView::OnInitialUpdate(), once for every type face in the 
// system. 
// 
//--------------------------------------------------------------------- 
static int CALLBACK EnumerateMyFonts(ENUMLOGFONTEX    *lpLogFontEx, 
                                     NEWTEXTMETRICEX  *lpMetricsEx, 
                                     int               fontType, 
                                     LPARAM            lParam) 
{ 
  nsVoidArray * fontList = (nsVoidArray *)lParam;

  /*printf("%s [%s][%s][%s]", lpLogFontEx->elfLogFont.lfFaceName, 
                 lpLogFontEx->elfFullName, 
                 lpLogFontEx->elfStyle, 
                 lpLogFontEx->elfScript 
                 ); */
  LOGFONT  ourLogFont;    // Local copy of the LOGFONT structure.
  memcpy(&ourLogFont,  (const void *)&lpLogFontEx->elfLogFont, sizeof LOGFONT); // Make the copy
  //printf("Size: %d\n", ourLogFont.lfHeight);

  FontInfo * font   = new FontInfo();
  font->mName.AssignWithConversion((char *)lpLogFontEx->elfLogFont.lfFaceName); // XXX I18N ?
  font->mIsScalable = PR_FALSE;
  font->mSizes      = nsnull;


  fontList->AppendElement(font);

   HDC myDC = ::GetDC(NULL);       // Get the old style DC handle 
  ::EnumFontFamilies(myDC, lpLogFontEx->elfLogFont.lfFaceName, (FONTENUMPROC)&MyEnumFontFamProc, (LPARAM)font);
  ::ReleaseDC(NULL, myDC); 


  return (1); 
}

//----------------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::LoadFontList()
{
  if (nsnull == mFontList) {
    mFontList = new nsVoidArray();
    if (nsnull == mFontList) {
      return NS_ERROR_FAILURE;
    }
  }
    
  LOGFONT        myLogFont;        // Our font structure 
  HDC            myDC;             // Our device context 


  // Here is a partial list CHAR_SETS
  /* 
  ANSI_CHARSET 
  DEFAULT_CHARSET 
  SYMBOL_CHARSET 
  SHIFTJIS_CHARSET 
  GB2312_CHARSET 
  HANGEUL_CHARSET 
  CHINESEBIG5_CHARSET 
  OEM_CHARSET 
  */ 

  myDC = ::GetDC(NULL);       // Get the old style DC handle 

  // Font search parms (find all) 
  myLogFont.lfCharSet        = ANSI_CHARSET; 
  myLogFont.lfFaceName[0]    = 0; 
  myLogFont.lfPitchAndFamily = 0; 

  // Force the calls to EnumerateMyFonts 
  ::EnumFontFamiliesEx(myDC, &myLogFont, (FONTENUMPROC)&EnumerateMyFonts, (LPARAM)mFontList, (DWORD)0); 

  myLogFont.lfCharSet        = SYMBOL_CHARSET; 
  ::EnumFontFamiliesEx(myDC, &myLogFont, (FONTENUMPROC)&EnumerateMyFonts, (LPARAM)mFontList, (DWORD)0);
  
  ::ReleaseDC(NULL, myDC); 

  return NS_OK;

} 

//----------------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::IsFontScalable( const nsString &aFontName, PRBool* aResult )
{
  // cache current value in case someone else externally is using it
  PRInt32 saveIterInx  = mNameIterInx;

  PRBool found = PR_FALSE;
  Reset();
  do {
    nsAutoString name;
    Get(&name);
    if (name.Equals(aFontName)) {
      found = PR_TRUE;
      break;
    }
  } while (Advance() == NS_OK);

  if (found) {
    FontInfo * fontInfo = (FontInfo *)mFontList->ElementAt(mNameIterInx);
    *aResult = fontInfo->mIsScalable;
    mNameIterInx = saveIterInx;
    return NS_OK;
  }
  mNameIterInx = saveIterInx;

  return NS_ERROR_FAILURE;
}
