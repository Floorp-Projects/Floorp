/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Henry Sobotka <sobotka@axess.com>: OS/2 adaptation
 */

#include "nsFontRetrieverService.h"

#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsFontSizeIterator.h"
#include "nslog.h"

NS_IMPL_LOG(nsFontRetrieverServiceLog)
#define PRINTF NS_LOG_PRINTF(nsFontRetrieverServiceLog)
#define FLUSH  NS_LOG_FLUSH(nsFontRetrieverServiceLog)
 
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
NS_IMETHODIMP nsFontRetrieverService::LoadFontList()
{
  if (nsnull == mFontList) {
    mFontList = new nsVoidArray();
    if (nsnull == mFontList) {
      return NS_ERROR_FAILURE;
    }
  }
    
  HPS hps = WinGetScreenPS(HWND_DESKTOP);
  LONG lWant = 0, lFonts;

  // Get number of fonts
  lFonts = GpiQueryFonts(hps, QF_PUBLIC | QF_PRIVATE,
                         0, &lWant, 0, 0);
  PFONTMETRICS fMetrics = new FONTMETRICS[lFonts];
   
  // Get fontmetrics
  GpiQueryFonts(hps, QF_PUBLIC | QF_PRIVATE, 0, &lFonts,
                sizeof(FONTMETRICS), fMetrics);

  // Load fonts
  for (LONG i = 0; i < lFonts; i++) {
    FontInfo * font   = new FontInfo();
    font->mName.AssignWithConversion(fMetrics[i].szFamilyname);
    font->mIsScalable = PR_FALSE;
    font->mSizes      = nsnull;
#ifdef DEBUG_sobotka
    PRINTF("Added FONT %s\n", font->mName.ToNewCString());
#endif

    mFontList->AppendElement(font);
  }

  // Clean up
  delete[] fMetrics;
  WinReleasePS(hps);

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
