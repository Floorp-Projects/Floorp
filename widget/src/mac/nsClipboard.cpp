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

#include "nsCOMPtr.h"
#include "nsClipboard.h"

#include "nsISupportsArray.h"
#include "nsIClipboardOwner.h"
#include "nsIDataFlavor.h"
#include "nsIFormatConverter.h"

#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"

#include <Scrap.h>


// interface definitions
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);

NS_IMPL_ADDREF_INHERITED(nsClipboard, nsBaseClipboard)
NS_IMPL_RELEASE_INHERITED(nsClipboard, nsBaseClipboard)

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{

}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
}



/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsClipboard::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  static NS_DEFINE_IID(kIClipboard, NS_ICLIPBOARD_IID);
  if (aIID.Equals(kIClipboard)) {
    *aInstancePtr = (void*) ((nsIClipboard*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


//
// MapMimeTypeToMacOSType
//
// Given a mime type, map this into the appropriate MacOS clipboard type. For
// types that we don't know about intrinsicly, use a hash to get a unique 4
// character code.
//
ResType
nsClipboard :: MapMimeTypeToMacOSType ( const nsString & aMimeStr )
{
  ResType format = 0;

  if (aMimeStr.Equals(kTextMime) )
    format = 'TEXT';
  else if ( aMimeStr.Equals(kXIFMime) )
    format = 'XIF ';
  else if ( aMimeStr.Equals(kHTMLMime) )
    format = 'HTML';
  else
    format = '????';   // XXX for now...
    
  /*
   else if (aMimeStr.Equals(kUnicodeMime)) {
    format = CF_UNICODETEXT;
  } else if (aMimeStr.Equals(kJPEGImageMime)) {
    format = CF_BITMAP;
  } else {
    char * str = aMimeStr.ToNewCString();
    format = ::RegisterClipboardFormat(str);
    delete[] str;
  }
  */
  
  return format;
  
} // MapMimeTypeToMacOSType


//
// SetNativeClipboardData
//
// Take data off the transferrable and put it on the clipboard in as many formats
// as are registered.
//
// NOTE: This code could all live in ForceDataToClipboard() and this could be a NOOP. 
// If speed and large data sizes are an issue, we should move that code there and only
// do it on an app switch.
//
NS_IMETHODIMP
nsClipboard :: SetNativeClipboardData()
{
  mIgnoreEmptyNotification = PR_TRUE;

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }
 
  
  ::ZeroScrap();
  
  // Get the flavor list, and on to the end of it, append the list of flavors we
  // can also get to through a converter. This is so that we can just walk the list
  // in one go and add the flavors to the clipboard.
  nsCOMPtr<nsISupportsArray> flavorList;
  mTransferable->GetTransferDataFlavors(getter_AddRefs(flavorList));
  nsCOMPtr<nsIFormatConverter> converter;
  mTransferable->GetConverter(getter_AddRefs(converter));
  if ( converter ) {
    nsCOMPtr<nsISupportsArray> convertedList;
    converter->GetOutputDataFlavors(getter_AddRefs(convertedList));
    if ( convertedList ) {
//      flavorList->AppendElements(convertedList);
      for (int i=0;i<convertedList->Count();i++) {
  	    nsCOMPtr<nsISupports> temp = getter_AddRefs(convertedList->ElementAt(i));
        flavorList->AppendElement(temp);    // this addref's for us
      }
    
    }

  }

  // For each flavor present in the transferable, put it on the clipboard. Luckily,
  // GetTransferData() handles conversions for us, so we really don't need to know
  // if the transferable relies on a converter to do the work or not.
  for ( int i = 0; i < flavorList->Count(); ++i ) {
  	nsCOMPtr<nsISupports> temp = getter_AddRefs(flavorList->ElementAt(i));
    nsCOMPtr<nsIDataFlavor> currentFlavor ( do_QueryInterface(temp) );
    if ( currentFlavor ) {
      // find MacOS flavor
      nsString mimeType;
      currentFlavor->GetMimeType(mimeType);
      ResType macOSFlavor = MapMimeTypeToMacOSType(mimeType);
    
      // get data. This takes converters into account. We don't own the data
      // so make sure not to delete it.
      void* data = nsnull;
      PRUint32 dataSize = 0;
      mTransferable->GetTransferData ( currentFlavor, &data, &dataSize );
      
      // stash on clipboard
      ::PutScrap ( dataSize, macOSFlavor, data );
    }
  } // foreach flavor in transferable

  return NS_OK;
  
} // SetNativeClipboardData


/**
  * 
  *
  */
NS_IMETHODIMP
nsClipboard :: GetNativeClipboardData(nsITransferable * aTransferable)
{
  // make sure we have a good transferable
  if (nsnull == aTransferable) {
    return NS_ERROR_FAILURE;
  }

  //XXX DO THE WORK

  return NS_OK;
}
