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
#include <Pt.h>
#include "nsPhWidgetLog.h"

//#include <gdk/gdkx.h>
//#include "X11/Xlib.h"
//#include "X11/Xutil.h"
 
#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsFontSizeIterator.h"
#include "nslog.h"

NS_IMPL_LOG(nsFontRetrieverServiceLog)
#define PRINTF NS_LOG_PRINTF(nsFontRetrieverServiceLog)
#define FLUSH  NS_LOG_FLUSH(nsFontRetrieverServiceLog)
 
NS_IMPL_ISUPPORTS2(nsFontRetrieverService, nsIFontRetrieverService, nsIFontNameIterator)

//----------------------------------------------------------
nsFontRetrieverService::nsFontRetrieverService()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::nsFontRetrieverService this=<%p>\n", this));

  NS_INIT_REFCNT();
  mFontList     = nsnull;
  mSizeIter     = nsnull;
  mNameIterInx  = 0;

}

//----------------------------------------------------------
nsFontRetrieverService::~nsFontRetrieverService()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::~nsFontRetrieverService this=<%p>\n",this));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::CreateFontNameIterator this=<%p>\n",this));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::CreateFontNameIterator 2 this=<%p>\n",this));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::Reset this=<%p>\n",this));

  mNameIterInx = 0;
  return NS_OK;
}

//----------------------------------------------------------
NS_IMETHODIMP nsFontRetrieverService::Get( nsString* aFontName )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::Get this=<%p>\n",this));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::Advance this=<%p>\n",this));

  if (mNameIterInx < mFontList->Count()-1) {
    mNameIterInx++;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//------------------------------
static FontInfo * GetFontInfo(nsVoidArray * aFontList, char * aName)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::GetFontInfo\n"));

  nsAutoString name((const PRUnichar *) aName, strlen(aName) );
  PRInt32 i;
  PRInt32 cnt = aFontList->Count();
  for (i=0;i<cnt;i++) {
    FontInfo * fontInfo = (FontInfo *)aFontList->ElementAt(i);
    if (fontInfo->mName.Equals(name)) {
      return fontInfo;
    }
  }

  FontInfo * fontInfo   = new FontInfo();
  fontInfo->mName       = nsString( (const PRUnichar *) aName, strlen(aName));
  PRINTF("nsFontRetrieverService::GetFontInfo Adding [%s]\n", aName);fflush(stdout); 
  fontInfo->mIsScalable = PR_FALSE; // X fonts aren't scalable right??
  fontInfo->mSizes      = nsnull;
  aFontList->AppendElement(fontInfo);
  return fontInfo;
}

//------------------------------
static void AddSizeToFontInfo(FontInfo * aFontInfo, PRInt32 aSize)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::AddSizeToFontInfo\n"));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::LoadFontList this=<%p>\n",this));

#if 0
  char    * pattern = "*";
  int nnames = 1024;

  int available = nnames+1;
  int i;
  char **fonts;
  XFontStruct *info;

  if (nsnull == mFontList) { 
    mFontList = new nsVoidArray(); 
    if (nsnull == mFontList) { 
      return NS_ERROR_FAILURE; 
    } 
  } 

  /* Get list of fonts matching pattern */
  for (;;) {
    // the following line is VERY slow to return
    fonts = XListFontsWithInfo(GDK_DISPLAY(), pattern, nnames, 
                               &available, &info);
    if (fonts == NULL || available < nnames)
      break;

    XFreeFontInfo(fonts, info, available);
    nnames = available * 2;
  }

  if (fonts == NULL) {
    PRINTF("pattern \"%s\" unmatched\n", pattern);
    return NS_ERROR_FAILURE;
  }

#if 0 // debug
  // print out all the retrieved fonts
  PRINTF("-----------------------------\n");
  for (i=0; i<available; i++) {
    PRINTF("[%s]i\n", fonts[i]);
  }
  PRINTF("-----------------------------\n");
#endif

  // this code assumes all like fonts are grouped together
  // currentName is the current name of the font we are gathering
  // sizes for, when the name changes we create a new FontInfo object
  // but it also takes into account fonts of similar names when it 
  // goes to add then and disregards duplicates
  char buffer[1024];
  char currentName[1024];
  FontInfo * font = nsnull;

  currentName[0]  = 0;
  for (i=0; i<available; i++) {

    // This is kind of lame, but it will have to do for now
    strcpy(buffer, fonts[i]);

    // Start by checking to see if the name begins with a dash
    char * ptr  = buffer;
    if (buffer[0] == '-') { //Format #1
      
      PRInt32 cnt = 0;
      // skip first two '-'
      do {
        if (*ptr == '-') cnt++;
        ptr++;
      } while (cnt < 2);
      
      // find the dash at the end of the name
      char * end = strchr(ptr, '-');
      if (end) {
        *end = 0;
        
        // Check to see if we need to create a new FontInfo obj
        // and set the currentName var to this guys font name
        if (strcmp(currentName, ptr) || NULL == font) {
          font = GetFontInfo(mFontList, ptr);
          strcpy(currentName, ptr);
        }
        if (nsnull == font->mSizes) {
          font->mSizes = new nsVoidArray();
        }
        ptr = end+1; // skip past the dash that was set to zero
        
        cnt = 0;
        // now skip ahead 4 dashes
        do {
          if (*ptr == '-') cnt++;
          ptr++;
        } while (cnt < 4);
        
        // find the dash after the size
        end = strchr(ptr, '-');
        
        if (end) {
	  *end = 0;
          PRInt32 size;
          sscanf(ptr, "%d", &size);
	  AddSizeToFontInfo(font, size);
        }
      }
    } else { // formats 2,3,4
      
      // no leading dash means the start of the 
      // buffer is the start of the name
      // this checks for a dash at the end of the font name
      // which means there is a size at the end
      char * end = strchr(buffer, '-');
      if (end) { // Format 2,3
        *end = 0;
        // Check to see if we need to create a new FontInfo obj
        // and set the currentName var to this guys font name
        if (strcmp(currentName, buffer) || NULL == font) { 
          font = GetFontInfo(mFontList, buffer);
          strcpy(currentName, buffer);
        }
        end++; // advance past the dash
	// check to see if we have a number
	ptr = end;
	if (isalpha(*ptr)) {  // Format 3
	  // skip until next dash
	  end = strchr(ptr, '-');
	  if (end) {
            *end = 0;
	    ptr = end+1;
	  }
	}
        PRInt32 size;
        // yes, it has a dash at the end so it must have the size
        // check to see if the size is terminated by a dash
        // it shouldn't be
        char * end2 = strchr(ptr, '-');
        if (end2) *end2 = 0; // put terminator at the dash
        sscanf(end, "%d", &size);
	AddSizeToFontInfo(font, size);

      } else { // Format #4
        // The font has an implicit size, 
        // so there is nothing to parse for size
        // so we can't really do much here
        // Check to see if we need to create a new FontInfo obj
        // and set the currentName var to this guys font name
        if (strcmp(currentName, buffer) || NULL == font) {
          font = GetFontInfo(mFontList, buffer);
          strcpy(currentName, buffer);
        }
      } 
    }

  }

  XFreeFontInfo(fonts, info, available);
#endif

  return NS_OK;
}


//---------------------------------------------------------- 
NS_IMETHODIMP nsFontRetrieverService::IsFontScalable(const nsString & aFontName,
                                                     PRBool* aResult ) 
{ 
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFontRetrieverService::IsFontScalable this=<%p>\n",this));

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
