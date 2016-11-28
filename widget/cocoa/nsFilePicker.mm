/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsFilePicker.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsIComponentManager.h"
#include "nsIFile.h"
#include "nsILocalFileMac.h"
#include "nsIURL.h"
#include "nsArrayEnumerator.h"
#include "nsIStringBundle.h"
#include "nsCocoaUtils.h"
#include "mozilla/Preferences.h"

// This must be included last:
#include "nsObjCExceptions.h"

using namespace mozilla;

const float kAccessoryViewPadding = 5;
const int   kSaveTypeControlTag = 1;

static bool gCallSecretHiddenFileAPI = false;
const char kShowHiddenFilesPref[] = "filepicker.showHiddenFiles";

/**
 * This class is an observer of NSPopUpButton selection change.
 */
@interface NSPopUpButtonObserver : NSObject
{
  NSPopUpButton* mPopUpButton;
  NSOpenPanel*   mOpenPanel;
  nsFilePicker*  mFilePicker;
}
- (void) setPopUpButton:(NSPopUpButton*)aPopUpButton;
- (void) setOpenPanel:(NSOpenPanel*)aOpenPanel;
- (void) setFilePicker:(nsFilePicker*)aFilePicker;
- (void) menuChangedItem:(NSNotification*)aSender;
@end

NS_IMPL_ISUPPORTS(nsFilePicker, nsIFilePicker)

// We never want to call the secret show hidden files API unless the pref
// has been set. Once the pref has been set we always need to call it even
// if it disappears so that we stop showing hidden files if a user deletes
// the pref. If the secret API was used once and things worked out it should
// continue working for subsequent calls so the user is at no more risk.
static void SetShowHiddenFileState(NSSavePanel* panel)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  bool show = false;
  if (NS_SUCCEEDED(Preferences::GetBool(kShowHiddenFilesPref, &show))) {
    gCallSecretHiddenFileAPI = true;
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
: mSelectedTypeIndex(0)
{
}

nsFilePicker::~nsFilePicker()
{
}

void
nsFilePicker::InitNative(nsIWidget *aParent, const nsAString& aTitle)
{
  mTitle = aTitle;
}

NSView* nsFilePicker::GetAccessoryView()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSView* accessoryView = [[[NSView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)] autorelease];

  // Set a label's default value.
  NSString* label = @"Format:";

  // Try to get the localized string.
  nsCOMPtr<nsIStringBundleService> sbs = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = sbs->CreateBundle("chrome://global/locale/filepicker.properties", getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv)) {
    nsXPIDLString locaLabel;
    bundle->GetStringFromName(u"formatLabel", getter_Copies(locaLabel));
    if (locaLabel) {
      label = [NSString stringWithCharacters:reinterpret_cast<const unichar*>(locaLabel.get())
                                      length:locaLabel.Length()];
    }
  }

  // set up label text field
  NSTextField* textField = [[[NSTextField alloc] init] autorelease];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setBordered:NO];
  [textField setFont:[NSFont labelFontOfSize:13.0]];
  [textField setStringValue:label];
  [textField setTag:0];
  [textField sizeToFit];

  // set up popup button
  NSPopUpButton* popupButton = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 0, 0) pullsDown:NO] autorelease];
  uint32_t numMenuItems = mTitles.Length();
  for (uint32_t i = 0; i < numMenuItems; i++) {
    const nsString& currentTitle = mTitles[i];
    NSString *titleString;
    if (currentTitle.IsEmpty()) {
      const nsString& currentFilter = mFilters[i];
      titleString = [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar*>(currentFilter.get())
                                                  length:currentFilter.Length()];
    }
    else {
      titleString = [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar*>(currentTitle.get())
                                                  length:currentTitle.Length()];
    }
    [popupButton addItemWithTitle:titleString];
    [titleString release];
  }
  if (mSelectedTypeIndex >= 0 && (uint32_t)mSelectedTypeIndex < numMenuItems)
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
NS_IMETHODIMP nsFilePicker::Show(int16_t *retval)
{
  NS_ENSURE_ARG_POINTER(retval);

  *retval = returnCancel;

  int16_t userClicksOK = returnCancel;

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
  nsCOMPtr<nsIFile> theFile;

  switch (mMode)
  {
    case modeOpen:
      userClicksOK = GetLocalFiles(mTitle, false, mFiles);
      break;
    
    case modeOpenMultiple:
      userClicksOK = GetLocalFiles(mTitle, true, mFiles);
      break;
      
    case modeSave:
      userClicksOK = PutLocalFile(mTitle, mDefault, getter_AddRefs(theFile));
      break;
      
    case modeGetFolder:
      userClicksOK = GetLocalFolder(mTitle, getter_AddRefs(theFile));
      break;
    
    default:
      NS_ERROR("Unknown file picker mode");
      break;
  }

  if (theFile)
    mFiles.AppendObject(theFile);
  
  *retval = userClicksOK;
  return NS_OK;
}

static
void UpdatePanelFileTypes(NSOpenPanel* aPanel, NSArray* aFilters)
{
  // If we show all file types, also "expose" bundles' contents.
  [aPanel setTreatsFilePackagesAsDirectories:!aFilters];

  [aPanel setAllowedFileTypes:aFilters];
}

@implementation NSPopUpButtonObserver
- (void) setPopUpButton:(NSPopUpButton*)aPopUpButton
{
  mPopUpButton = aPopUpButton;
}

- (void) setOpenPanel:(NSOpenPanel*)aOpenPanel
{
  mOpenPanel = aOpenPanel;
}

- (void) setFilePicker:(nsFilePicker*)aFilePicker
{
  mFilePicker = aFilePicker;
}

- (void) menuChangedItem:(NSNotification *)aSender
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;
  int32_t selectedItem = [mPopUpButton indexOfSelectedItem];
  if (selectedItem < 0) {
    return;
  }

  mFilePicker->SetFilterIndex(selectedItem);
  UpdatePanelFileTypes(mOpenPanel, mFilePicker->GetFilterList());

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN();
}
@end

// Use OpenPanel to do a GetFile. Returns |returnOK| if the user presses OK in the dialog. 
int16_t
nsFilePicker::GetLocalFiles(const nsString& inTitle, bool inAllowMultiple, nsCOMArray<nsIFile>& outFiles)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  int16_t retVal = (int16_t)returnCancel;
  NSOpenPanel *thePanel = [NSOpenPanel openPanel];

  SetShowHiddenFileState(thePanel);

  // Set the options for how the get file dialog will appear
  SetDialogTitle(inTitle, thePanel);
  [thePanel setAllowsMultipleSelection:inAllowMultiple];
  [thePanel setCanSelectHiddenExtension:YES];
  [thePanel setCanChooseDirectories:NO];
  [thePanel setCanChooseFiles:YES];
  [thePanel setResolvesAliases:YES]; //this is default - probably doesn't need to be set
  
  // Get filters
  // filters may be null, if we should allow all file types.
  NSArray *filters = GetFilterList();

  // set up default directory
  NSString *theDir = PanelDefaultDirectory();
  
  // if this is the "Choose application..." dialog, and no other start
  // dir has been set, then use the Applications folder.
  if (!theDir) {
    if (filters && [filters count] == 1 &&
        [(NSString *)[filters objectAtIndex:0] isEqualToString:@"app"])
      theDir = @"/Applications/";
    else
      theDir = @"";
  }

  if (theDir) {
    [thePanel setDirectoryURL:[NSURL fileURLWithPath:theDir isDirectory:YES]];
  }

  int result;
  nsCocoaUtils::PrepareForNativeAppModalDialog();
  if (mFilters.Length() > 1) {
    // [NSURL initWithString:] (below) throws an exception if URLString is nil.

    NSPopUpButtonObserver* observer = [[NSPopUpButtonObserver alloc] init];

    NSView* accessoryView = GetAccessoryView();
    [thePanel setAccessoryView:accessoryView];

    [observer setPopUpButton:[accessoryView viewWithTag:kSaveTypeControlTag]];
    [observer setOpenPanel:thePanel];
    [observer setFilePicker:this];

    [[NSNotificationCenter defaultCenter]
      addObserver:observer
      selector:@selector(menuChangedItem:)
      name:NSMenuWillSendActionNotification object:nil];

    UpdatePanelFileTypes(thePanel, filters);
    result = [thePanel runModal];

    [[NSNotificationCenter defaultCenter] removeObserver:observer];
    [observer release];
  } else {
    // If we show all file types, also "expose" bundles' contents.
    if (!filters) {
      [thePanel setTreatsFilePackagesAsDirectories:YES];
    }
    [thePanel setAllowedFileTypes:filters];
    result = [thePanel runModal];
  }
  nsCocoaUtils::CleanUpAfterNativeAppModalDialog();
  
  if (result == NSFileHandlingPanelCancelButton)
    return retVal;

  // Converts data from a NSArray of NSURL to the returned format.
  // We should be careful to not call [thePanel URLs] more than once given that
  // it creates a new array each time.
  // We are using Fast Enumeration, thus the NSURL array is created once then
  // iterated.
  for (NSURL* url in [thePanel URLs]) {
    if (!url) {
      continue;
    }

    nsCOMPtr<nsIFile> localFile;
    NS_NewLocalFile(EmptyString(), true, getter_AddRefs(localFile));
    nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
    if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)url))) {
      outFiles.AppendObject(localFile);
    }
  }

  if (outFiles.Count() > 0)
    retVal = returnOK;

  return retVal;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

// Use OpenPanel to do a GetFolder. Returns |returnOK| if the user presses OK in the dialog.
int16_t
nsFilePicker::GetLocalFolder(const nsString& inTitle, nsIFile** outFile)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;
  NS_ASSERTION(outFile, "this protected member function expects a null initialized out pointer");
  
  int16_t retVal = (int16_t)returnCancel;
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
  if (theDir) {
    [thePanel setDirectoryURL:[NSURL fileURLWithPath:theDir isDirectory:YES]];
  }
  nsCocoaUtils::PrepareForNativeAppModalDialog();
  int result = [thePanel runModal];
  nsCocoaUtils::CleanUpAfterNativeAppModalDialog();

  if (result == NSFileHandlingPanelCancelButton)
    return retVal;

  // get the path for the folder (we allow just 1, so that's all we get)
  NSURL *theURL = [[thePanel URLs] objectAtIndex:0];
  if (theURL) {
    nsCOMPtr<nsIFile> localFile;
    NS_NewLocalFile(EmptyString(), true, getter_AddRefs(localFile));
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
int16_t
nsFilePicker::PutLocalFile(const nsString& inTitle, const nsString& inDefaultName, nsIFile** outFile)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;
  NS_ASSERTION(outFile, "this protected member function expects a null initialized out pointer");

  int16_t retVal = returnCancel;
  NSSavePanel *thePanel = [NSSavePanel savePanel];

  SetShowHiddenFileState(thePanel);

  SetDialogTitle(inTitle, thePanel);

  // set up accessory view for file format options
  NSView* accessoryView = GetAccessoryView();
  [thePanel setAccessoryView:accessoryView];

  // set up default file name
  NSString* defaultFilename = [NSString stringWithCharacters:(const unichar*)inDefaultName.get() length:inDefaultName.Length()];

  // set up allowed types; this prevents the extension from being selected
  // use the UTI for the file type to allow alternate extensions (e.g., jpg vs. jpeg)
  NSString* extension = defaultFilename.pathExtension;
  if (extension.length != 0) {
    CFStringRef type = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (CFStringRef)extension, NULL);

    if (type) {
      thePanel.allowedFileTypes = @[(NSString*)type];
      CFRelease(type);
    } else {
      // if there's no UTI for the file extension, use the extension itself.
      thePanel.allowedFileTypes = @[extension];
    }
  }
  // Allow users to change the extension.
  thePanel.allowsOtherFileTypes = YES;

  // set up default directory
  NSString *theDir = PanelDefaultDirectory();
  if (theDir) {
    [thePanel setDirectoryURL:[NSURL fileURLWithPath:theDir isDirectory:YES]];
  }

  // load the panel
  nsCocoaUtils::PrepareForNativeAppModalDialog();
  [thePanel setNameFieldStringValue:defaultFilename];
  int result = [thePanel runModal];
  nsCocoaUtils::CleanUpAfterNativeAppModalDialog();
  if (result == NSFileHandlingPanelCancelButton)
    return retVal;

  // get the save type
  NSPopUpButton* popupButton = [accessoryView viewWithTag:kSaveTypeControlTag];
  if (popupButton) {
    mSelectedTypeIndex = [popupButton indexOfSelectedItem];
  }

  NSURL* fileURL = [thePanel URL];
  if (fileURL) { 
    nsCOMPtr<nsIFile> localFile;
    NS_NewLocalFile(EmptyString(), true, getter_AddRefs(localFile));
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

NSArray *
nsFilePicker::GetFilterList()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mFilters.Length()) {
    return nil;
  }

  if (mFilters.Length() <= (uint32_t)mSelectedTypeIndex) {
    NS_WARNING("An out of range index has been selected. Using the first index instead.");
    mSelectedTypeIndex = 0;
  }

  const nsString& filterWide = mFilters[mSelectedTypeIndex];
  if (!filterWide.Length()) {
    return nil;
  }

  if (filterWide.Equals(NS_LITERAL_STRING("*"))) {
    return nil;
  }

  // The extensions in filterWide are in the format "*.ext" but are expected
  // in the format "ext" by NSOpenPanel. So we need to filter some characters.
  NSMutableString* filterString = [[[NSMutableString alloc] initWithString:
                                    [NSString stringWithCharacters:reinterpret_cast<const unichar*>(filterWide.get())
                                                            length:filterWide.Length()]] autorelease];
  NSCharacterSet *set = [NSCharacterSet characterSetWithCharactersInString:@". *"];
  NSRange range = [filterString rangeOfCharacterFromSet:set];
  while (range.length) {
    [filterString replaceCharactersInRange:range withString:@""];
    range = [filterString rangeOfCharacterFromSet:set];
  }

  return [[[NSArray alloc] initWithArray:
           [filterString componentsSeparatedByString:@";"]] autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// Sets the dialog title to whatever it should be.  If it fails, eh,
// the OS will provide a sensible default.
void
nsFilePicker::SetDialogTitle(const nsString& inTitle, id aPanel)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [aPanel setTitle:[NSString stringWithCharacters:(const unichar*)inTitle.get() length:inTitle.Length()]];

  if (!mOkButtonLabel.IsEmpty()) {
    [aPanel setPrompt:[NSString stringWithCharacters:(const unichar*)mOkButtonLabel.get() length:mOkButtonLabel.Length()]];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
} 

// Converts path from an nsIFile into a NSString path
// If it fails, returns an empty string.
NSString *
nsFilePicker::PanelDefaultDirectory()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSString *directory = nil;
  if (mDisplayDirectory) {
    nsAutoString pathStr;
    mDisplayDirectory->GetPath(pathStr);
    directory = [[[NSString alloc] initWithCharacters:reinterpret_cast<const unichar*>(pathStr.get())
                                               length:pathStr.Length()] autorelease];
  }
  return directory;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

NS_IMETHODIMP nsFilePicker::GetFile(nsIFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = nullptr;
  
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
  *aFileURL = nullptr;

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
  // "..apps" has to be translated with native executable extensions.
  if (aFilter.EqualsLiteral("..apps")) {
    mFilters.AppendElement(NS_LITERAL_STRING("*.app"));
  } else {
    mFilters.AppendElement(aFilter);
  }
  mTitles.AppendElement(aTitle);
  
  return NS_OK;
}

// Get the filter index - do we still need this?
NS_IMETHODIMP nsFilePicker::GetFilterIndex(int32_t *aFilterIndex)
{
  *aFilterIndex = mSelectedTypeIndex;
  return NS_OK;
}

// Set the filter index - do we still need this?
NS_IMETHODIMP nsFilePicker::SetFilterIndex(int32_t aFilterIndex)
{
  mSelectedTypeIndex = aFilterIndex;
  return NS_OK;
}
