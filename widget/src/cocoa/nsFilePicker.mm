/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla browser.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Steve Dagley <sdagley@netscape.com>
 *   David Haas <haasd@cae.wisc.edu>
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Arrgh.  Bitten by bug 154232 here.  
// Need to undef DARWIN before including Cocoa & (by default) CoreFoundation
// so we can use CFURLGetFSRef.

#undef DARWIN
#include <Cocoa/Cocoa.h>
#define DARWIN  

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsIComponentManager.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsArrayEnumerator.h"

#include "nsFilePicker.h"


NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
: mAllFilesDisplayed(PR_TRUE)
, mMode(0)
, mSelectedType(0)
{
}


//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
  // string arrays clean themselves up
}


void
nsFilePicker::InitNative(nsIWidget *aParent, const nsAString& aTitle,
                         PRInt16 aMode)
{
  mTitle = aTitle;
  mMode = aMode;
}


//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::Show(PRInt16 *retval)
{
  NS_ENSURE_ARG_POINTER(retval);

  *retval = returnCancel;

  PRInt16 userClicksOK = returnCancel;
//
// Random questions from DHH:
//
// Why do we pass mTitle, mDefault to the functions?  Can GetLocalFile. PutLocalFile,
// and GetLocalFolder get called someplace else?  It generates a bunch of warnings
// as it is right now.
//
// I think we could easily combine GetLocalFile and GetLocalFolder together, just
// setting panel pick options based on mMode.  I didn't do it here b/c I wanted to 
// make this look as much like Carbon nsFilePicker as possible.  
//  
//  

  mFiles.Clear();
  nsCOMPtr<nsILocalFile> theFile;

  switch (mMode)
  {
    case modeOpen:
      userClicksOK = GetLocalFiles(mTitle, PR_FALSE, mFiles);
      break;
    
    case modeOpenMultiple:
      userClicksOK = GetLocalFiles(mTitle, PR_TRUE, mFiles);
      break;
      
    case modeSave:
      userClicksOK = PutLocalFile(mTitle, mDefault, getter_AddRefs(theFile));
      break;
      
    case modeGetFolder:
      userClicksOK = GetLocalFolder(mTitle, getter_AddRefs(theFile));
      break;
    
    default:
      NS_ASSERTION(0, "Unknown file picker mode");
      break;
  }

  if (theFile)
    mFiles.AppendObject(theFile);
  
  *retval = userClicksOK;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// GetLocalFile
//
// Use OpenPanel to do a GetFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the selected file is in the FSSpec.
//
//-------------------------------------------------------------------------
PRInt16
nsFilePicker::GetLocalFiles(const nsString& inTitle, PRBool inAllowMultiple, nsCOMArray<nsILocalFile>& outFiles)
{
  PRInt16 retVal = (PRInt16)returnCancel;
  NSOpenPanel *thePanel = [NSOpenPanel openPanel];

  // Get filters
  NSArray *filters = GenerateFilterList();

  // Set the options for how the get file dialog will appear
  SetDialogTitle(inTitle, thePanel);
  [thePanel setAllowsMultipleSelection:inAllowMultiple];
  [thePanel setCanSelectHiddenExtension:YES];
  [thePanel setCanChooseDirectories:NO];
  [thePanel setCanChooseFiles:YES];
  [thePanel setResolvesAliases:YES];        //this is default - probably doesn't need to be set
  // handle how we deal with packages. 
  if (filters)
    [thePanel setTreatsFilePackagesAsDirectories:NO];       

  // set up default directory
  NSString *theDir = PanelDefaultDirectory();
  int result = [thePanel runModalForDirectory:theDir file:nil types:filters];  
  
  if (result == NSFileHandlingPanelCancelButton)
    return retVal;
  
  // append each chosen file to our list
  for (unsigned int i = 0; i < [[thePanel URLs] count]; i ++)
  {
    NSURL *theURL = [[thePanel URLs] objectAtIndex:i];
    if (theURL)
    {
      nsCOMPtr<nsILocalFile> localFile;
      NS_NewLocalFile(nsString(), PR_TRUE, getter_AddRefs(localFile));
      nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
      if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)theURL)))
        outFiles.AppendObject(localFile);
    }
  }

  if (outFiles.Count() > 0)
    retVal = returnOK;

  return retVal;
} // GetFile


//-------------------------------------------------------------------------
//
// GetLocalFolder
//
// Use OpenPanel to do a GetFolder. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the folder location is in the FSSpec.
//
// Note: Apparently, not used in Chimera.  So it hasn't been tested.  At all.
//       Consider yourself warned.
//-------------------------------------------------------------------------
PRInt16
nsFilePicker::GetLocalFolder(const nsString& inTitle, nsILocalFile** outFile)
{
  NS_ENSURE_ARG(outFile);
  *outFile = nsnull;
  
  PRInt16 retVal = (PRInt16)returnCancel;
  NSOpenPanel *thePanel = [NSOpenPanel openPanel];

  // Set the options for how the get file dialog will appear
  SetDialogTitle(inTitle, thePanel);
  [thePanel setAllowsMultipleSelection:NO];   //this is default -probably doesn't need to be set
  [thePanel setCanSelectHiddenExtension:YES];
  [thePanel setCanChooseDirectories:YES];
  [thePanel setCanChooseFiles:NO];
  [thePanel setResolvesAliases:YES];          //this is default - probably doesn't need to be set
  [thePanel setTreatsFilePackagesAsDirectories:YES]; //sure, we can pick packages.  

  // set up default directory
  NSString *theDir = PanelDefaultDirectory();
  int result = [thePanel runModalForDirectory:theDir file:nil types:nil];  

  if (result == NSFileHandlingPanelCancelButton)
    return retVal;
    
  // get FSRef for folder (we allow just 1, so that's all we get)
  NSURL *theURL = [[thePanel URLs] objectAtIndex:0];
  if (theURL)
  {
    nsCOMPtr<nsILocalFile> localFile;
    NS_NewLocalFile(nsString(), PR_TRUE, getter_AddRefs(localFile));
    nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
    if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)theURL)))
    {
      *outFile = localFile;
      NS_ADDREF(*outFile);
      retVal = returnOK;
    }
  }

  return retVal;
} // GetFolder

//-------------------------------------------------------------------------
//
// PutLocalFile
//
// Use SavePanel to do a PutFile. Returns returnOK if the user presses OK in the dialog. If
// they do so, the folder location is in the FSSpec.
//
// Note: I don't think Chimera is using this at all.  So - it hasn't been tested.
//       If you end up calling it, let me know how it turns out.
//-------------------------------------------------------------------------

PRInt16
nsFilePicker::PutLocalFile(const nsString& inTitle, const nsString& inDefaultName, nsILocalFile** outFile)
{
  NS_ENSURE_ARG(outFile);
  *outFile = nsnull;

  PRInt16 retVal = returnCancel;
  NSSavePanel *thePanel = [NSSavePanel savePanel];
  
  // set up save panel options & whatnot.
  SetDialogTitle(inTitle, thePanel);
  
  // set up default file name
  NSString* defaultFilename = [NSString stringWithCharacters:(const unichar*)inDefaultName.get() length:inDefaultName.Length()];

  // set up default directory
  NSString *theDir = PanelDefaultDirectory();
  
  // load the panel.
  int result = [thePanel runModalForDirectory:theDir file:defaultFilename];
  if (result == NSFileHandlingPanelCancelButton)
    return retVal;

  // Get the NSURL for the directory where the file to be saved
  NSURL *dirURL = [NSURL fileURLWithPath:[thePanel directory]];
  // append the filename
  NSURL *fileURL = [[NSURL alloc] initWithString:[thePanel filename] relativeToURL:dirURL];
  if (fileURL)
  { 
    nsCOMPtr<nsILocalFile> localFile;
    NS_NewLocalFile(nsString(), PR_TRUE, getter_AddRefs(localFile));
    nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
    if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)fileURL)))
    {
      *outFile = localFile;
      NS_ADDREF(*outFile);
      // XXX how can we tell if we're replacing? Need to return returnReplace
      retVal = returnOK;
    }

    [fileURL release];
  }      
  return retVal;    
}

//-------------------------------------------------------------------------
//
// GenerateFilterList
//
// Take the list of file types (in a nice win32-specific format) and fills up
// an NSArray of them for the Open Panel.  Cocoa's open panel doesn't recognize * 
// as being equal to everything, so make sure there aren't any in mFlatFilter array.
//
//-------------------------------------------------------------------------

NSArray *
nsFilePicker::GenerateFilterList()
{
  NSArray *filterArray = nil;
  if (mFilters.Count() > 0)
  {
    // Set up our filter string
    NSMutableString *giantFilterString = [[NSMutableString alloc] initWithString:@""];
    if (!giantFilterString) //dang.
      return filterArray;   

    // Loop through each of the filter strings
    for (PRInt32 loop = 0; loop < mFilters.Count(); loop++)
    {
      nsString* filterWide = mFilters[loop];
      if (filterWide && filterWide->Length() > 0)
        [giantFilterString appendString:[NSString stringWithCharacters:filterWide->get() length:filterWide->Length()]];
    }
    
    // Now we clean stuff up.  Get rid of white spaces, "*"'s, and the odd period or two.
    NSCharacterSet *aSet = [NSCharacterSet characterSetWithCharactersInString:[NSString stringWithString:@". *"]];
    NSRange aRange = [giantFilterString rangeOfCharacterFromSet:aSet];
    while (aRange.length) {
      [giantFilterString replaceCharactersInRange:aRange withString:@""];
      aRange = [giantFilterString rangeOfCharacterFromSet:aSet];
    }   
    // OK, if string isn't empty we'll make a new filter list
    if ([giantFilterString length] > 0)
      // every time we find a semicolon, we've found a new filter.
      // components SeparatedByString should do that for us.
      filterArray = [[[NSArray alloc] initWithArray:[giantFilterString componentsSeparatedByString:@";"]] autorelease];
    [giantFilterString release];
  }
  return filterArray;
} // GenerateFilterList

//-------------------------------------------------------------------------
//
// SetDialogTitle
// 
// Sets the dialog title to whatever it should be.  If it fails, eh,
// the OS will provide a sensible default.
//
//-------------------------------------------------------------------------

void
nsFilePicker::SetDialogTitle(const nsString& inTitle, id aPanel)
{
  [aPanel setTitle:[NSString stringWithCharacters:(const unichar*)inTitle.get() length:inTitle.Length()]];
} 

//-------------------------------------------------------------------------
//
// PanelDefaultDirectory
//
// Converts path from an nsILocalFile into a NSString path
// If it fails, returns an empty string.
//
//-------------------------------------------------------------------------
NSString *
nsFilePicker::PanelDefaultDirectory()
{
  NSString *directory = nil;
  if (mDisplayDirectory) {
    nsAutoString pathStr;
    mDisplayDirectory->GetPath(pathStr);
    directory = [[[NSString alloc] initWithCharacters:pathStr.get() length:nsCRT::strlen(pathStr.get())] autorelease];
  }
  return directory;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = nsnull;
  
  // just return the first file
  if (mFiles.Count() > 0)
  {
    *aFile = mFiles.ObjectAt(0);
    NS_IF_ADDREF(*aFile);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  NS_ENSURE_ARG_POINTER(aFileURL);
  *aFileURL = nsnull;

  if (mFiles.Count() == 0)
    return NS_OK;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewFileURI(getter_AddRefs(uri), mFiles.ObjectAt(0));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
  NS_ENSURE_TRUE(fileURL, NS_ERROR_FAILURE);
  
  NS_ADDREF(*aFileURL = fileURL);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  return NS_NewArrayEnumerator(aFiles, mFiles);
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const nsAString& aString)
{
  mDefault = aString;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(nsAString& aString)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// The default extension to use for files
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDefaultExtension(nsAString& aExtension)
{
  aExtension.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const nsAString& aExtension)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDisplayDirectory(nsILocalFile *aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDisplayDirectory(nsILocalFile **aDirectory)
{
  *aDirectory = mDisplayDirectory;
  NS_IF_ADDREF(*aDirectory);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Append an entry to the filters array
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
  mFilters.AppendString(aFilter);
  mTitles.AppendString(aTitle);
  
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the filter index - do we still need this?
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  *aFilterIndex = mSelectedType;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the filter index - do we still need this?
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  mSelectedType = aFilterIndex;
  return NS_OK;
}

