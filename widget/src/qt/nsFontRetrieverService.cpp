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

#include "nsFontRetrieverService.h"
#include "nsIWidget.h"
#include <ctype.h>

#define FAKE_QSTRINGLIST_H /* Avoid pulling in Corel's qstringlist.h */
#include "qfontdatabase.h"
#include "qstring.h"
#include "qstringlist.h"
#include "qvaluelist.h"

#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsFontSizeIterator.h"
 
NS_IMPL_ISUPPORTS2(nsFontRetrieverService, nsIFontRetrieverService, nsIFontNameIterator)

//----------------------------------------------------------
nsFontRetrieverService::nsFontRetrieverService()
{
printf("JCG: nsFontRetrieverService CTOR.\n");
  NS_INIT_REFCNT();

  mFontList     = nsnull;
  mSizeIter     = nsnull;
  mNameIterInx  = 0;
}

//----------------------------------------------------------
nsFontRetrieverService::~nsFontRetrieverService()
{
printf("JCG: nsFontRetrieverService DTOR.\n");
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
NS_IMETHODIMP nsFontRetrieverService::CreateFontSizeIterator( const nsString & aFontName, 
                                                              nsIFontSizeIterator** aIterator )
{
  // save value in case someone externally is using it
  PRInt32 saveIterInx = mNameIterInx;

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
char *tmp;
printf("JCG: nsFontRetrieverService::Get. FontName: %s\n",(tmp = aFontName->ToNewCString()));
nsMemory::Free((void*)tmp);

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

//------------------------------
static FontInfo * GetFontInfo(nsVoidArray * aFontList, char * aName)
{
  nsAutoString name;
  name.AssignWithConversion(aName);
  PRInt32 i;
  PRInt32 cnt = aFontList->Count();
  for (i=0;i<cnt;i++) {
    FontInfo * fontInfo = (FontInfo *)aFontList->ElementAt(i);
    if (fontInfo->mName.Equals(name)) {
      return fontInfo;
    }
  }

  FontInfo * fontInfo   = new FontInfo();
  fontInfo->mName.AssignWithConversion(aName);
  //printf("Adding [%s]\n", aName);fflush(stdout); 
  fontInfo->mIsScalable = PR_FALSE; // X fonts aren't scalable right??
  fontInfo->mSizes      = nsnull;
  aFontList->AppendElement(fontInfo);
  return fontInfo;
}

//------------------------------
static void AddSizeToFontInfo(FontInfo * aFontInfo, PRInt32 aSize)
{
  nsVoidArray * sizes;
  if (nsnull == aFontInfo->mSizes) {
    sizes = new nsVoidArray();
    aFontInfo->mSizes = sizes;
  } else {
    sizes = aFontInfo->mSizes;
  }
  PRInt32 i;
  PRInt32 cnt = sizes->Count();
  for (i=0;i<cnt;i++) {
    PRInt32 size = (int)sizes->ElementAt(i);
    if (size == aSize) {
      return;
    }
  }
  sizes->AppendElement((void *)aSize);
}

//---------------------------------------------------
// XXX - Hack - Parts of this will need to be reworked
//
// This method does brute force parcing for 4 different formats:
//
// 1) The format -*-*-*-*-*-* etc.
//    -misc-fixed-medium-r-normal--13-120-75-75-c-80-iso8859-8
//
// 2) Name-size format 
//    lucidasans-10
//
// 3) Name-style-size
//    lucidasans-bold-10
//
// 4) Name only (implicit size)
//    6x13
//
//--------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::LoadFontList()
{
  QStringList qFamilies,qStyles,qCharsets;
  typedef QValueList<int> QFSList;
  QFSList qSizes;
  QString qCurFam;
  QFontDatabase qFontDB;

  if (nsnull == mFontList) { 
    mFontList = new nsVoidArray(); 
    if (nsnull == mFontList) { 
      return NS_ERROR_FAILURE; 
    } 
  } 

  /* Get list of all fonts */
  qFamilies = qFontDB.families(FALSE);
  qFamilies.sort();
  for (QStringList::Iterator famIt = qFamilies.begin(); famIt != qFamilies.end(); ++famIt) {
    qCharsets = qFontDB.charSets(*famIt,FALSE);
    qCharsets.sort();
    for (QStringList::Iterator csIt = qCharsets.begin(); csIt != qCharsets.end(); ++csIt) {
      qStyles = qFontDB.styles(*famIt,*csIt);
      qStyles.sort();
      for (QStringList::Iterator stIt = qStyles.begin(); stIt != qStyles.end(); ++stIt) {
        qSizes = qFontDB.pointSizes(*famIt,*stIt,*csIt);
        for (QFSList::Iterator szIt = qSizes.begin(); szIt != qSizes.end(); ++szIt) {
          FontInfo * font = nsnull;

          if (qCurFam.compare(*famIt) || nsnull == font) {
             font = GetFontInfo(mFontList, (char*)(*famIt).latin1());
             qCurFam = *famIt;
          }
          if (nsnull == font->mSizes)
             font->mSizes = new nsVoidArray();
          AddSizeToFontInfo(font, *szIt);
          font->mIsScalable = qFontDB.isScalable(*famIt,*stIt,*csIt);
        }
      }
    }
  }
  return NS_OK;
}


//---------------------------------------------------------- 
NS_IMETHODIMP nsFontRetrieverService::IsFontScalable(const nsString & aFontName,
                                                     PRBool* aResult ) 
{ 
  // save value in case someone externally is using it
  PRInt32 saveIterInx = mNameIterInx;
   
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
