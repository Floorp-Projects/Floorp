/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
      nsFont * font = (nsFont *)mFontList->ElementAt(i);
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

  if (aIID.Equals(nsIFontRetrieverService::GetIID())) {
    *aInstancePtr = (void*) ((nsIFontRetrieverService*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(nsIFontNameIterator::GetIID())) {
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
NS_IMETHODIMP nsFontRetrieverService::CreateFontSizeIterator( const nsString * aFontName, 
                                                             nsIFontSizeIterator** aIterator )
{
  if (nsnull == mSizeIter) {
    mSizeIter = new nsFontSizeIterator(mFontList);
  }
  NS_ASSERTION( nsnull != mSizeIter, "nsFontSizeIterator instance pointer is null");

  *aIterator = (nsIFontSizeIterator *)mSizeIter;
  NS_ADDREF(mSizeIter);

  mSizeIter->SetFontName(*aFontName);
  return NS_OK;
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
    nsFont * font = (nsFont *)mFontList->ElementAt(mNameIterInx);
    *aFontName = font->name;
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

  printf("%s [%s][%s][%s]\n", lpLogFontEx->elfLogFont.lfFaceName, 
                 lpLogFontEx->elfFullName, 
                 lpLogFontEx->elfStyle, 
                 lpLogFontEx->elfScript 
                 ); 

  nsFont * font = new nsFont(lpLogFontEx->elfLogFont.lfFaceName, 0,0,0,0,0);
  fontList->AppendElement(font);

  // 
  // We need to set our TRUETYPE, BITMAP, and NONANSI flags 
  // based on the font type. 
  // 
  /*if (fontType == TRUETYPE_FONTTYPE) 
    faceFlag |= FNT_TRUETYPE; 
  else 
    faceFlag |= FNT_BITMAP; 

  if (ourLogFont.lfCharSet != ANSI_CHARSET) 
    faceFlag |= FNT_NONANSI; 
  else 
    faceFlag |= FNT_ANSI; 
    */ 

  // 
  // We also need to set the family type (we use it for filtering) 
  // 
  /*switch (ourLogFont.lfPitchAndFamily & 0xF0) 
    { 
    case FF_SWISS: 
       faceFlag |= FNT_SANSSERIF; 
       break; 

    case FF_ROMAN: 
       faceFlag |= FNT_SERIF; 
       break; 

    case FF_MODERN: 
       faceFlag |= FNT_MODERN; 
       break; 

    case FF_SCRIPT: 
       faceFlag |= FNT_SCRIPT; 
       break; 

    default: 
       faceFlag |= FNT_OTHER; 
       break; 
    }*/ 

  // 
  // Add the font to our font list. 
  // 
  //pFaceList->NewFont(lpLogFont->lfFaceName, faceFlag, fontType, ourLogFont); // Otherwise, build table. 

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

  myDC = ::GetDC(NULL);       // Get the old style DC handle 

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

  // Font search parms (find all) 
  myLogFont.lfCharSet        = ANSI_CHARSET; 
  myLogFont.lfFaceName[0]    = 0; 
  myLogFont.lfPitchAndFamily = 0; 

  // Force the calls to EnumerateMyFonts 
  ::EnumFontFamiliesEx(myDC, &myLogFont, (FONTENUMPROC)&EnumerateMyFonts, (LPARAM)mFontList, (DWORD)0); 
  ::ReleaseDC(NULL, myDC); 

  return NS_OK;

} 
