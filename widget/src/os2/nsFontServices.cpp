/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsWidgetDefs.h"
#include "nsFontServices.h"
#include "nsIFontNameIterator.h"
#include "nsIFontSizeIterator.h"
#include "nsString.h"

#include <stdlib.h>

// FONTMETRICS are obtained and stored in a datastructure which is optimized
// towards the enumeration-style purpose for which they are required in this
// toolkit (primarily to populate combobox widgets, I guess)

class FaceInstance
{
   PFONTMETRICS  mFontMetrics;
   FaceInstance *mNext;

 public:
   FaceInstance( PFONTMETRICS aMetrics) : mFontMetrics(aMetrics), mNext(0)
   {}

  ~FaceInstance()
   {
      delete mNext;
   }

   PRBool IsScalable()
   {
      return mFontMetrics->fsDefn & FM_DEFN_OUTLINE;
   }

   double GetPointSize()
   {
      return ((double)mFontMetrics->sNominalPointSize) / 10.0;
   }

   void Append( FaceInstance *aInstance)
   { 
      if( IsScalable() || GetPointSize() == aInstance->GetPointSize())
         delete aInstance;
      else
      {
         if( !mNext) mNext = aInstance;
         else mNext->Append( aInstance);
      }
   }

   FaceInstance *GetNext() { return mNext; }
};


class FontFamily
{
   char         *mFamName;
   FaceInstance *mInstances;
   FontFamily   *mNext;

 public:
   FontFamily( PFONTMETRICS aMetrics) : mFamName(0), mInstances(0), mNext(0)
   {
      mFamName = strdup( aMetrics->szFamilyname);
      mInstances = new FaceInstance( aMetrics);
   }

  ~FontFamily()
   {
      if( mFamName) free( mFamName);
      delete mInstances;
      delete mNext;
   }

   void Append( FontFamily *aFamily)
   {
      if( !mNext) mNext = aFamily;
      else mNext->Append( aFamily);
   }

   FontFamily *GetNext() { return mNext; }

   FaceInstance *GetInstances()  { return mInstances; }
   const char   *GetName() const { return mFamName; }
   // XXX maybe have a method here to do the system atom-table thing
   //     ("good grief", I know...)
};

class FontLister
{
   FontFamily   *mFamilies;
   FontFamily   *mCache;
   PFONTMETRICS  mMetrics;

 public:
   // XXX ought to take a DC or PS or something...
   FontLister() : mFamilies(0), mCache(0), mMetrics(0)
   {
      HPS hps = WinGetScreenPS( HWND_DESKTOP);

      // Get fontmetrics for the display
      LONG lWant = 0, lFonts;
      lFonts = GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE,
                              0, &lWant, 0, 0);
      mMetrics = new FONTMETRICS [ lFonts ];
   
      GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE, 0, &lFonts,
                     sizeof( FONTMETRICS), mMetrics);

      // now stick 'em in our datastructures
      for( LONG l = 0; l < lFonts; l++)
      {
         FontFamily *pFamily = FindFamily( mMetrics[l].szFamilyname);
         if( pFamily)
         {
            FaceInstance *pInstance = new FaceInstance( mMetrics + l);
            pFamily->GetInstances()->Append( pInstance);
         }
         else
         {
            pFamily = new FontFamily( mMetrics + l);
            if( !mFamilies) mFamilies = pFamily;
            else mFamilies->Append( pFamily);
         }
      }

      WinReleasePS( hps);
   }

  ~FontLister()
   {
      delete mFamilies;
      delete [] mMetrics;
   }

   FontFamily *FindFamily( const char *aName)
   {
      if( !mCache || stricmp( mCache->GetName(), aName))
      {
         FontFamily *pFamily = mFamilies;
         while( pFamily)
         {
            if( !stricmp( pFamily->GetName(), aName))
               break;
            pFamily = pFamily->GetNext();
         }
         mCache = pFamily;
      }
      return mCache;
   }

   FontFamily *GetFamilies() { return mFamilies; }
};

// Now declare a couple of iterators for font size and name.
// Could maybe share code somehow (base template class?)

// Font-size iterator (xpcom)
class nsFontSizeIterator : public nsIFontSizeIterator
{
   FaceInstance *mFirst;
   FaceInstance *mCurrent;

 public:
   nsFontSizeIterator( FaceInstance *aFace)
      : mFirst(aFace), mCurrent(aFace)
   {
      NS_INIT_REFCNT();
   }

   virtual ~nsFontSizeIterator() {}

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIFontSizeIterator
   NS_IMETHOD Reset()
   {
      mCurrent = mFirst;
      return NS_OK;
   }

   NS_IMETHOD Get( double *aFontSize)
   {
      nsresult rc = NS_ERROR_FAILURE;
      if( mCurrent)
      {
         *aFontSize = mCurrent->GetPointSize();
         rc = NS_OK;
      }
      return rc;
   }

   NS_IMETHOD Advance()
   {
      nsresult rc = NS_ERROR_FAILURE;
      if( mCurrent)
      {
         mCurrent = mCurrent->GetNext();
         if( mCurrent) rc = NS_OK;
      }
      return rc;
   }
};

NS_IMPL_ISUPPORTS(nsFontSizeIterator, NS_GET_IID(nsIFontSizeIterator))

// Font-name iterator (xpcom)
class nsFontNameIterator : public nsIFontNameIterator
{
   FontFamily *mFirst;
   FontFamily *mCurrent;

 public:
   nsFontNameIterator( FontFamily *aFamily)
      : mFirst(aFamily), mCurrent(aFamily)
   {
      NS_INIT_REFCNT();
   }

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIFontNameIterator
   NS_IMETHOD Reset()
   {
      mCurrent = mFirst;
      return NS_OK;
   }

   NS_IMETHOD Get( nsString *aString)
   {
      nsresult rc = NS_ERROR_FAILURE;
      if( mCurrent)
      {
         aString->AssignWithConversion( mCurrent->GetName());
         rc = NS_OK;
      }
      return rc;
   }

   NS_IMETHOD Advance()
   {
      nsresult rc = NS_ERROR_FAILURE;
      if( mCurrent)
      {
         mCurrent = mCurrent->GetNext();
         if( mCurrent) rc = NS_OK;
      }
      return rc;
   }
};

NS_IMPL_ISUPPORTS(nsFontNameIterator, NS_GET_IID(nsIFontNameIterator))

// font retriever service
NS_IMPL_ISUPPORTS(nsFontRetrieverService, NS_GET_IID(nsIFontRetrieverService))

nsFontRetrieverService::nsFontRetrieverService()
{
   NS_INIT_REFCNT();
   mFontLister = new FontLister;
}

nsFontRetrieverService::~nsFontRetrieverService()
{
   delete mFontLister;
}

nsresult
nsFontRetrieverService::CreateFontNameIterator( nsIFontNameIterator **aIterator)
{
   if( !aIterator)
      return NS_ERROR_NULL_POINTER;

   *aIterator = new nsFontNameIterator( mFontLister->GetFamilies());
   NS_ADDREF(*aIterator);

   return NS_OK;
}

nsresult
nsFontRetrieverService::CreateFontSizeIterator( const nsString &aFontName,
                                                nsIFontSizeIterator **aIterator)
{
   if( !aIterator)
      return NS_ERROR_NULL_POINTER;

   char buffer[100];
   gModuleData.ConvertFromUcs( aFontName, buffer, 100);

   FontFamily *pFamily = mFontLister->FindFamily( buffer);
   if( !pFamily)
      return NS_ERROR_FAILURE;

   *aIterator = new nsFontSizeIterator( pFamily->GetInstances());
   NS_ADDREF(*aIterator);

   return NS_OK;
}

nsresult
nsFontRetrieverService::IsFontScalable( const nsString &aFontName,
                                        PRBool *aResult)
{
   if( !aResult)
      return NS_ERROR_NULL_POINTER;

   char buffer[100];
   gModuleData.ConvertFromUcs( aFontName, buffer, 100);

   FontFamily *pFamily = mFontLister->FindFamily( buffer);
   if( !pFamily)
      return NS_ERROR_FAILURE;

   *aResult = pFamily->GetInstances()->IsScalable();

   return NS_OK;
}

// singleton behaviour -- don't trust xpcom so do it ourselves
nsresult NS_GetFontService( nsISupports **aResult)
{
   if( !aResult)
      return NS_ERROR_NULL_POINTER;

   if( !gModuleData.fontService)
   {
      gModuleData.fontService = new nsFontRetrieverService;
      NS_ADDREF(gModuleData.fontService);
   }

   *aResult = gModuleData.fontService;
   return NS_OK;
}
