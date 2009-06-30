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
 *   Josh Aas <josh@mozilla.com>
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

#import <Cocoa/Cocoa.h>

#include "nsFilePicker.h"
#include "nsObjCExceptions.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsIComponentManager.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsIURL.h"
#include "nsArrayEnumerator.h"
#include "nsIStringBundle.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsCocoaUtils.h"

const float kAccessoryViewPadding = 5;
const int   kSaveTypeControlTag = 1;
const char  kLastTypeIndexPref[] = "filepicker.lastTypeIndex";

static PRBool gCallSecretHiddenFileAPI = PR_FALSE;
const char kShowHiddenFilesPref[] = "filepicker.showHiddenFiles";

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

// We never want to call the secret show hidden files API unless the pref
// has been set. Once the pref has been set we always need to call it even
// if it disappears so that we stop showing hidden files if a user deletes
// the pref. If the secret API was used once and things worked out it should
// continue working for subsequent calls so the user is at no more risk.
static void SetShowHiddenFileState(NSSavePanel* panel)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PRBool show = PR_FALSE;
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    nsresult rv = prefs->GetBoolPref(kShowHiddenFilesPref, &show);
    if (NS_SUCCEEDED(rv))
      gCallSecretHiddenFileAPI = PR_TRUE;
  }

  if (gCallSecretHiddenFileAPI) {
    // invoke a method to get a Cocoa-internal nav view
    SEL navViewSelector = @selector(_navView);
    NSMethodSignature* navViewSignature = [panel methodSignatureForSelector:navViewSelector];
    if (!navViewSignature)
      return;
    NSInvocation* navViewInvocation = [NSInvocation invocationWithMethodSignature:navViewSignature];
    [navViewInvocation setSelector:navViewSelector];
    [navViewInvocation setTarget:panel];
    [navViewInvocation invoke];

    // get the returned nav view
    id navView = nil;
    [navViewInvocation getReturnValue:&navView];

    // invoke the secret show hidden file state method on the nav view
    SEL showHiddenFilesSelector = @selector(setShowsHiddenFiles:);
    NSMethodSignature* showHiddenFilesSignature = [navView methodSignatureForSelector:showHiddenFilesSelector];
    if (!showHiddenFilesSignature)
      return;
    NSInvocation* showHiddenFilesInvocation = [NSInvocation invocationWithMethodSignature:showHiddenFilesSignature];
    [showHiddenFilesInvocation setSelector:showHiddenFilesSelector];
    [showHiddenFilesInvocation setTarget:navView];
    [showHiddenFilesInvocation setArgument:&show atIndex:2];
    [showHiddenFilesInvocation invoke];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsFilePicker::nsFilePicker()
: mMode(0)
, mSelectedTypeIndex(0)
{
}

nsFilePicker::~nsFilePicker()
{
}

void
nsFilePicker::InitNative(nsIWidget *aParent, const nsAString& aTitle,
                         PRInt16 aMode)
{
  mTitle = aTitle;
  mMode = aMode;

  // read in initial type index from prefs
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    int prefIndex;
    if (NS_SUCCEEDED(prefs->GetIntPref(kLastTypeIndexPref, &prefIndex)))
      mSelectedTypeIndex = prefIndex;
  }
}

NSView* nsFilePicker::GetAccessoryView()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSView* accessoryView = [[[NSView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)] autorelease];

  // get the localized string for "Save As:"
  NSString* saveAsLabel = @"Save As:"; // backup in case we can't get a localized string
  nsCOMPtr<nsIStringBundleService> sbs = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = sbs->CreateBundle("chrome://global/locale/filepicker.properties", getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv)) {
    nsXPIDLString label;
    bundle->GetStringFromName(NS_LITERAL_STRING("saveAsLabel").get(), getter_Copies(label));
    if (label)
      saveAsLabel = [NSString stringWithCharacters:label.get() length:label.Length()];
  }

  // set up label text field
  NSTextField* textField = [[[NSTextField alloc] init] autorelease];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setBordered:NO];
  [textField setFont:[NSFont labelFontOfSize:13.0]];
  [textField setStringValue:saveAsLabel];
  [textField setTag:0];
  [textField sizeToFit];

  // set up popup button
  NSPopUpButton* popupButton = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 0, 0) pullsDown:NO] autorelease];
  PRUint32 numMenuItems = mTitles.Length();
  for (PRUint32 i = 0; i < numMenuItems; i++) {
    const nsString& currentTitle = mTitles[i];
    NSString *titleString;
    if (currentTitle.IsEmpty()) {
      const nsString& currentFilter = mFilters[i];
      titleString = [[NSString alloc] initWithCharacters:currentFilter.get()
                                                  length:currentFilter.Length()];
    }
    else {
      titleString = [[NSString alloc] initWithCharacters:currentTitle.get()
                                                  length:currentTitle.Length()];
    }
    [popupButton addItemWithTitle:titleString];
    [titleString release];
  }
  if (mSelectedTypeIndex >= 0 && (PRUint32)mSelectedTypeIndex < numMenuItems)
    [popupButton selectItemAtIndex:mSelectedTypeIndex];
  [popupButton setTag:kSaveTypeControlTag];
  [popupButton sizeToFit]; // we have to do sizeToFit to get the height calculated for us
  // This is just a default width that works well, doesn't truncate the vast majority of
  // things that might end up in the menu.
  [popupButton setFrameSize:NSMakeSize(180, [popupButton frame].size.height)];

  // position everything based on control sizes with kAccessoryViewPadding pix padding
  // on each side kAccessoryViewPadding pix horizontal padding between controls
  float greatestHeight = [textField frame].size.height;
  if ([popupButton frame].size.height > greatestHeight)
    greatestHeight = [popupButton frame].size.height;
  float totalViewHeight = greatestHeight + kAccessoryViewPadding * 2;
  float totalViewWidth  = [textField frame].size.width + [popupButton frame].size.width + kAccessoryViewPadding * 3;
  [accessoryView setFrameSize:NSMakeSize(totalViewWidth, totalViewHeight)];

  float textFieldOriginY = ((greatestHeight - [textField frame].size.height) / 2 + 1) + kAccessoryViewPadding;
  [textField setFrameOrigin:NSMakePoint(kAccessoryViewPadding, textFieldOriginY)];
  
  float popupOriginX = [textField frame].size.width + kAccessoryViewPadding * 2;
  float popupOriginY = ((greatestHeight - [popupButton frame].size.height) / 2) + kAccessoryViewPadding;
  [popupButton setFrameOrigin:NSMakePoint(popupOriginX, popupOriginY)];

  [accessoryView addSubview:textField];
  [accessoryView addSubview:popupButton];
  return accessoryView;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// Display the file dialog
NS_IMETHODIMP nsFilePicker::Show(PRInt16 *retval)
{
  NS_ENSURE_ARG_POINTER(retval);

  *retval = returnCancel;

  PRInt16 userClicksOK = returnCancel;

// Random questions from DHH:
//
// Why do we pass mTitle, mDefault to the functions?  Can GetLocalFile. PutLocalFile,
// and GetLocalFolder get called someplace else?  It generates a bunch of warnings
// as it is right now.
//
// I think we could easily combine GetLocalFile and GetLocalFolder together, just
// setting panel pick options based on mMode.  I didn't do it here b/c I wanted to 
// make this look as much like Carbon nsFilePicker as possible.  

  mFiles.Clear();
  nsCOMPtr<nsILocalFile> theFile;

  switch (mMode)
  {
    case modeOpen:
      userClicksOK = GetLocalFiles(mTitle, mDefault, PR_FALSE, mFiles);
      break;
    
    case modeOpenMultiple:
      userClicksOK = GetLocalFiles(mTitle, mDefault, PR_TRUE, mFiles);
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

// Use OpenPanel to do a GetFile. Returns |returnOK| if the user presses OK in the dialog. 
PRInt16
nsFilePicker::GetLocalFiles(const nsString& inTitle, const nsString& inDefaultName, PRBool inAllowMultiple, nsCOMArray<nsILocalFile>& outFiles)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  PRInt16 retVal = (PRInt16)returnCancel;
  NSOpenPanel *thePanel = [NSOpenPanel openPanel];

  SetShowHiddenFileState(thePanel);

  // Get filters
  // filters may be null, if we should allow all file types.
  NSArray *filters = GenerateFilterList();

  // Set the options for how the get file dialog will appear
  SetDialogTitle(inTitle, thePanel);
  [thePanel setAllowsMultipleSelection:inAllowMultiple];
  [thePanel setCanSelectHiddenExtension:YES];
  [thePanel setCanChooseDirectories:NO];
  [thePanel setCanChooseFiles:YES];
  [thePanel setResolvesAliases:YES];        //this is default - probably doesn't need to be set
  
  // if we show all file types, also "expose" bundles' contents.
  if (!filters)
    [thePanel setTreatsFilePackagesAsDirectories:NO];       

  // set up default file name
  NSString* defaultFilename = [NSString stringWithCharacters:(const unichar*)inDefaultName.get() length:inDefaultName.Length()];

  // set up default directory
  NSString *theDir = PanelDefaultDirectory();
  
  // if this is the "Choose application..." dialog, and no other start
  // dir has been set, then use the Applications folder.
  if (!theDir && filters && [filters count] == 1 && 
      [(NSString *)[filters objectAtIndex:0] isEqualToString:@"app"]) {
    theDir = @"/Applications/";
  }

  nsCocoaUtils::PrepareForNativeAppModalDialog();
  int result = [thePanel runModalForDirectory:theDir file:defaultFilename
                types:filters];
  nsCocoaUtils::CleanUpAfterNativeAppModalDialog();
  
  if (result == NSFileHandlingPanelCancelButton)
    return retVal;
  
  // append each chosen file to our list
  for (unsigned int i = 0; i < [[thePanel URLs] count]; i++) {
    NSURL *theURL = [[thePanel URLs] objectAtIndex:i];
    if (theURL) {
      nsCOMPtr<nsILocalFile> localFile;
      NS_NewLocalFile(EmptyString(), PR_TRUE, getter_AddRefs(localFile));
      nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
      if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)theURL)))
        outFiles.AppendObject(localFile);
    }
  }

  if (outFiles.Count() > 0)
    retVal = returnOK;

  return retVal;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

// Use OpenPanel to do a GetFolder. Returns |returnOK| if the user presses OK in the dialog.
PRInt16
nsFilePicker::GetLocalFolder(const nsString& inTitle, nsILocalFile** outFile)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_ARG(outFile);
  *outFile = nsnull;
  
  PRInt16 retVal = (PRInt16)returnCancel;
  NSOpenPanel *thePanel = [NSOpenPanel openPanel];

  SetShowHiddenFileState(thePanel);

  // Set the options for how the get file dialog will appear
  SetDialogTitle(inTitle, thePanel);
  [thePanel setAllowsMultipleSelection:NO];   //this is default -probably doesn't need to be set
  [thePanel setCanSelectHiddenExtension:YES];
  [thePanel setCanChooseDirectories:YES];
  [thePanel setCanChooseFiles:NO];
  [thePanel setResolvesAliases:YES];          //this is default - probably doesn't need to be set
  [thePanel setCanCreateDirectories:YES];
  
  // packages != folders
  [thePanel setTreatsFilePackagesAsDirectories:NO];

  // set up default directory
  NSString *theDir = PanelDefaultDirectory();
  nsCocoaUtils::PrepareForNativeAppModalDialog();
  int result = [thePanel runModalForDirectory:theDir file:nil types:nil];  
  nsCocoaUtils::CleanUpAfterNativeAppModalDialog();

  if (result == NSFileHandlingPanelCancelButton)
    return retVal;

  // get the path for the folder (we allow just 1, so that's all we get)
  NSURL *theURL = [[thePanel URLs] objectAtIndex:0];
  if (theURL) {
    nsCOMPtr<nsILocalFile> localFile;
    NS_NewLocalFile(EmptyString(), PR_TRUE, getter_AddRefs(localFile));
    nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
    if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)theURL))) {
      *outFile = localFile;
      NS_ADDREF(*outFile);
      retVal = returnOK;
    }
  }

  return retVal;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

// Returns |returnOK| if the user presses OK in the dialog.
PRInt16
nsFilePicker::PutLocalFile(const nsString& inTitle, const nsString& inDefaultName, nsILocalFile** outFile)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_ARG(outFile);
  *outFile = nsnull;

  PRInt16 retVal = returnCancel;
  NSSavePanel *thePanel = [NSSavePanel savePanel];

  SetShowHiddenFileState(thePanel);

  SetDialogTitle(inTitle, thePanel);

  // set up accessory view for file format options
  NSView* accessoryView = GetAccessoryView();
  [thePanel setAccessoryView:accessoryView];

  // set up default file name
  NSString* defaultFilename = [NSString stringWithCharacters:(const unichar*)inDefaultName.get() length:inDefaultName.Length()];

  // set up default directory
  NSString *theDir = PanelDefaultDirectory();

  // load the panel
  nsCocoaUtils::PrepareForNativeAppModalDialog();
  int result = [thePanel runModalForDirectory:theDir file:defaultFilename];
  nsCocoaUtils::CleanUpAfterNativeAppModalDialog();
  if (result == NSFileHandlingPanelCancelButton)
    return retVal;

  // get the save type
  NSPopUpButton* popupButton = [accessoryView viewWithTag:kSaveTypeControlTag];
  if (popupButton) {
    mSelectedTypeIndex = [popupButton indexOfSelectedItem];
    // save out to prefs for initializing other file picker instances
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs)
      prefs->SetIntPref(kLastTypeIndexPref, mSelectedTypeIndex);
  }

  NSURL* fileURL = [thePanel URL];
  if (fileURL) { 
    nsCOMPtr<nsILocalFile> localFile;
    NS_NewLocalFile(EmptyString(), PR_TRUE, getter_AddRefs(localFile));
    nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
    if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)fileURL))) {
      *outFile = localFile;
      NS_ADDREF(*outFile);
      // We tell if we are replacing or not by just looking to see if the file exists.
      // The user could not have hit OK and not meant to replace the file.
      if ([[NSFileManager defaultManager] fileExistsAtPath:[fileURL path]])
        retVal = returnReplace;
      else
        retVal = returnOK;
    }
  }

  return retVal;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

// Take the list of file types (in a nice win32-specific format) and fills up
// an NSArray of them for the Open Panel.  Note: Will return nil if we should allow
// all file types.
NSArray *
nsFilePicker::GenerateFilterList()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSArray *filterArray = nil;
  if (mFilters.Length() > 0) {
    // Set up our filter string
    NSMutableString *giantFilterString = [[[NSMutableString alloc] initWithString:@""] autorelease];

    // Loop through each of the filter strings
    for (PRUint32 loop = 0; loop < mFilters.Length(); loop++) {
      const nsString& filterWide = mFilters[loop];

      // separate individual filters
      if ([giantFilterString length] > 0)
        [giantFilterString appendString:[NSString stringWithString:@";"]];

      // handle special case filters
      if (filterWide.Equals(NS_LITERAL_STRING("*"))) {
        // if we'll allow all files, we won't bother parsing all other
        // file types. just return early.
        return nil;
      }
      else if (filterWide.Equals(NS_LITERAL_STRING("..apps"))) {
        // this magic filter means that we should enable app bundles.
        // translate it into a usable filter, and continue looping through 
        // other filters.
        [giantFilterString appendString:@"*.app"];
        continue;
      }
      
      if (filterWide.Length() > 0)
        [giantFilterString appendString:[NSString stringWithCharacters:filterWide.get() length:filterWide.Length()]];
    }
    
    // Now we clean stuff up.  Get rid of white spaces, "*"'s, and the odd period or two.
    NSCharacterSet *aSet = [NSCharacterSet characterSetWithCharactersInString:[NSString stringWithString:@". *"]];
    NSRange aRange = [giantFilterString rangeOfCharacterFromSet:aSet];
    while (aRange.length) {
      [giantFilterString replaceCharactersInRange:aRange withString:@""];
      aRange = [giantFilterString rangeOfCharacterFromSet:aSet];
    }   
    // OK, if string isn't empty we'll make a new filter list
    if ([giantFilterString length] > 0) {
      // every time we find a semicolon, we've found a new filter.
      // components SeparatedByString should do that for us.
      filterArray = [[[NSArray alloc] initWithArray:[giantFilterString componentsSeparatedByString:@";"]] autorelease];
    }
  }
  return filterArray;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// Sets the dialog title to whatever it should be.  If it fails, eh,
// the OS will provide a sensible default.
void
nsFilePicker::SetDialogTitle(const nsString& inTitle, id aPanel)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [aPanel setTitle:[NSString stringWithCharacters:(const unichar*)inTitle.get() length:inTitle.Length()]];

  NS_OBJC_END_TRY_ABORT_BLOCK;
} 

// Converts path from an nsILocalFile into a NSString path
// If it fails, returns an empty string.
NSString *
nsFilePicker::PanelDefaultDirectory()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSString *directory = nil;
  if (mDisplayDirectory) {
    nsAutoString pathStr;
    mDisplayDirectory->GetPath(pathStr);
    directory = [[[NSString alloc] initWithCharacters:pathStr.get() length:pathStr.Length()] autorelease];
  }
  return directory;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = nsnull;
  
  // just return the first file
  if (mFiles.Count() > 0) {
    *aFile = mFiles.ObjectAt(0);
    NS_IF_ADDREF(*aFile);
  }

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFileURL(nsIURI **aFileURL)
{
  NS_ENSURE_ARG_POINTER(aFileURL);
  *aFileURL = nsnull;

  if (mFiles.Count() == 0)
    return NS_OK;

  return NS_NewFileURI(aFileURL, mFiles.ObjectAt(0));
}

NS_IMETHODIMP nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  return NS_NewArrayEnumerator(aFiles, mFiles);
}

NS_IMETHODIMP nsFilePicker::SetDefaultString(const nsAString& aString)
{
  mDefault = aString;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(nsAString& aString)
{
  return NS_ERROR_FAILURE;
}

// The default extension to use for files
NS_IMETHODIMP nsFilePicker::GetDefaultExtension(nsAString& aExtension)
{
  aExtension.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const nsAString& aExtension)
{
  return NS_OK;
}

// Append an entry to the filters array
NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
  mFilters.AppendElement(aFilter);
  mTitles.AppendElement(aTitle);
  
  return NS_OK;
}

// Get the filter index - do we still need this?
NS_IMETHODIMP nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  *aFilterIndex = mSelectedTypeIndex;
  return NS_OK;
}

// Set the filter index - do we still need this?
NS_IMETHODIMP nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  mSelectedTypeIndex = aFilterIndex;
  return NS_OK;
}
