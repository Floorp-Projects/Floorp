/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Steve Dagley <sdagley@netscape.com>
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

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsIComponentManager.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsToolkit.h"
#include "nsIEventSink.h"
#include "nsArrayEnumerator.h"

#include <InternetConfig.h>

#include "nsMacControl.h"
#include "nsCarbonHelpers.h"

#include "nsFilePicker.h"
#include "nsWatchTask.h"

#include "nsIInternetConfigService.h"
#include "nsIMIMEInfo.h"


NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

OSType nsFilePicker::sCurrentProcessSignature = 0;

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
  : mAllFilesDisplayed(PR_TRUE)
  , mApplicationsDisplayed(PR_FALSE)
  , mSelectedType(0)
  , mTypeOffset(0)
{
  
  // Zero out the type lists
  for (int i = 0; i < kMaxTypeListCount; i++)
  	mTypeLists[i] = 0L;
  
  mSelectedType = 0;
  // If NavServces < 2.0 we need to play games with the mSelectedType
  mTypeOffset = (NavLibraryVersion() < 0x02000000) ? 1 : 0;

  if (sCurrentProcessSignature == 0)
  {
    ProcessSerialNumber psn;
    ProcessInfoRec  info;
    
    psn.highLongOfPSN = 0;
    psn.lowLongOfPSN  = kCurrentProcess;

    info.processInfoLength = sizeof(ProcessInfoRec);
    info.processName = nil;
    info.processAppSpec = nil;
    OSErr err = ::GetProcessInformation(&psn, &info);
    if (err == noErr)
        sCurrentProcessSignature = info.processSignature;
    // Try again next time if error
  }
}


//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
	// Destroy any filters we have built
	if ( mFilters.Count() ) {
	  for (int i = 0; i < kMaxTypeListCount; i++) {
	  	if (mTypeLists[i])
	  		DisposePtr((Ptr)mTypeLists[i]);
	  }
	}	
  mFilters.Clear();
  mTitles.Clear();

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
  
  mFiles.Clear();
  
  // XXX Ignore the filter list for now....
  
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


//
// HandleShowPopupMenuSelect
//
// Figure out which menu item was selected and set mSelectedType accordingly
// We do this by the rather easy/skanky method of using the menuType field of the
// NavMenuItemSpec as the index into the menu items we've added
// And oh so strangely enough Nav Services 3.0 uses the menuType field the same way - as an
// index into CFArray of popupExtension strings so we don't have to special case this
// based on TARGET_CARBON
void nsFilePicker::HandleShowPopupMenuSelect(NavCBRecPtr callBackParms)
{
  if (callBackParms)
  {
    NavMenuItemSpec menuItemSpec = *(NavMenuItemSpec*)callBackParms->eventData.eventDataParms.param;
    PRUint32        numMenuItems = mTitles.Count();
    if (mTypeOffset && (numMenuItems != 0))
    { // Special case Nav Services prior to 2.0
      // Make sure the menu item selected was one of ours
      if ((menuItemSpec.menuType != menuItemSpec.menuCreator) ||
          ((PRInt32)menuItemSpec.menuType < mTypeOffset) ||
          (menuItemSpec.menuType > numMenuItems))
      { // Doesn't appear to be one of our items selected so force it to be
        NavMenuItemSpec  menuItem;
        menuItem.version = kNavMenuItemSpecVersion;
        menuItem.menuType = mSelectedType + mTypeOffset;
        menuItem.menuCreator = mSelectedType + mTypeOffset;
        menuItem.menuItemName[0] = 0;
        (void)::NavCustomControl(callBackParms->context, kNavCtlSelectCustomType, &menuItem);
      }
      else
        mSelectedType = menuItemSpec.menuType - mTypeOffset;
    }
    else
      mSelectedType = menuItemSpec.menuType;
  }
}


//
// FileDialogEventHandlerProc
//
// An event filter proc for NavServices so the dialogs will be movable-modals.
//
pascal void nsFilePicker::FileDialogEventHandlerProc(NavEventCallbackMessage msg, NavCBRecPtr cbRec, NavCallBackUserData callbackUD)
{
  nsFilePicker* self = NS_REINTERPRET_CAST(nsFilePicker*, callbackUD);
  switch (msg)
  {
    case kNavCBEvent:
      switch (cbRec->eventData.eventDataParms.event->what)
      {
        case updateEvt:
        {
          WindowPtr window = reinterpret_cast<WindowPtr>(cbRec->eventData.eventDataParms.event->message);
          nsCOMPtr<nsIEventSink> sink;
          nsToolkit::GetWindowEventSink (window, getter_AddRefs(sink));
          if (sink)
          {
            ::BeginUpdate(window);
            PRBool handled = PR_FALSE;
            sink->DispatchEvent(cbRec->eventData.eventDataParms.event, &handled);
            ::EndUpdate(window);	        
          }        
        }
        break;
      }
      break;
    
    case kNavCBStart:
    {
      NavMenuItemSpec  menuItem;
      menuItem.version = kNavMenuItemSpecVersion;
      menuItem.menuType = self->mSelectedType + self->mTypeOffset;
      menuItem.menuCreator = self->mSelectedType + self->mTypeOffset;
      menuItem.menuItemName[0] = 0;
      (void)::NavCustomControl(cbRec->context, kNavCtlSelectCustomType, &menuItem);
    }
    break;
    
    case kNavCBPopupMenuSelect:
      // Format menu boinked - see what's happening
      if (self)
        self->HandleShowPopupMenuSelect(cbRec);
      break;
  }
}


//
// IsFileInFilterList
//
// Check our |mTypeLists| list to see if the given type is in there.
//
Boolean
nsFilePicker::IsTypeInFilterList ( ResType inType )
{
  for ( int i = 0; i < mFilters.Count(); ++i ) {
    for ( int j = 0; j < mTypeLists[i]->osTypeCount; ++j ) {
      if ( mTypeLists[i]->osType[j] == inType ) 
        return true;
    }  // foreach type w/in the group
  } // for each filter group
  
  return false;
  
} // IsFileInFilterList


Boolean
nsFilePicker::IsExtensionInFilterList ( StrFileName & inFileName )
{
  char extension[256];
  
  // determine the extension from the file name
  unsigned char* curr = &inFileName[inFileName[0]];
  while ( curr != inFileName && *curr-- != '.' ) ;
  if ( curr == inFileName )                         // no '.' in string, fails this check
    return false;
  ++curr;                                           // we took one too many steps back
  short extensionLen = (inFileName + inFileName[0]) - curr + 1;
  strncpy ( extension, (char*)curr, extensionLen);
  extension[extensionLen] = '\0';
  
  // see if it is in our list
  for ( int i = 0; i < mFlatFilters.Count(); ++i ) {
    if ( mFlatFilters[i]->Equals(extension) )
      return true;
  }
  return false;
}


//
// FileDialogFilterProc
//
// Called from navServices with our filePicker object as |callbackUD|, check our
// internal list to see if the file should be displayed.
//
pascal
Boolean 
nsFilePicker::FileDialogFilterProc(AEDesc* theItem, void* theInfo,
                                        NavCallBackUserData callbackUD, NavFilterModes filterMode)
{
  Boolean shouldDisplay = true;
  nsFilePicker* self = NS_REINTERPRET_CAST(nsFilePicker*, callbackUD);
  if ( self && !self->mAllFilesDisplayed ) {
    if ( theItem->descriptorType == typeFSS ) {
      NavFileOrFolderInfo* info = NS_REINTERPRET_CAST ( NavFileOrFolderInfo*, theInfo );
      if ( !info->isFolder ) {
        // check it against our list. If that fails, check the extension directly
        if ( ! self->IsTypeInFilterList(info->fileAndFolder.fileInfo.finderInfo.fdType) ) {
          FSSpec fileSpec;
          if ( ::AEGetDescData(theItem, &fileSpec, sizeof(FSSpec)) == noErr )
            if ( ! self->IsExtensionInFilterList(fileSpec.name) )
              shouldDisplay = false;
        }
      } // if file isn't a folder
    } // if the item is an FSSpec
  }
  
  return shouldDisplay;
  
} // FileDialogFilterProc                                        



//-------------------------------------------------------------------------
//
// GetFile
//
// Use NavServices to do a GetFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, ioLocalFile is initialized to that file.
//
//-------------------------------------------------------------------------


PRInt16
nsFilePicker::GetLocalFiles(const nsString& inTitle, PRBool inAllowMultiple, nsCOMArray<nsILocalFile>& outFiles)
{
  PRInt16 retVal = returnCancel;
  NavEventUPP eventProc = NewNavEventUPP(FileDialogEventHandlerProc);  // doesn't really matter if this fails
  NavObjectFilterUPP filterProc = NewNavObjectFilterUPP(FileDialogFilterProc);  // doesn't really matter if this fails
  NavDialogRef dialog;
  NavDialogCreationOptions dialogCreateOptions;
  // Set defaults
  OSErr anErr = ::NavGetDefaultDialogCreationOptions(&dialogCreateOptions);
  if (anErr != noErr)
    return retVal;
    
  // Set the options for how the get file dialog will appear
  dialogCreateOptions.optionFlags |= kNavNoTypePopup;
  dialogCreateOptions.optionFlags |= kNavDontAutoTranslate;
  dialogCreateOptions.optionFlags |= kNavDontAddTranslateItems;
  if (inAllowMultiple)
    dialogCreateOptions.optionFlags |= kNavAllowMultipleFiles;
  dialogCreateOptions.modality = kWindowModalityAppModal;
  dialogCreateOptions.parentWindow = NULL;
  CFStringRef titleRef = CFStringCreateWithCharacters(NULL, 
                                                     (const UniChar *) inTitle.get(), inTitle.Length());
  dialogCreateOptions.windowTitle = titleRef;

  // sets up the |mTypeLists| array so the filter proc can use it
  MapFilterToFileTypes();
	
  // allow packages to be chosen if the filter is "*" or "..apps"
  if (mAllFilesDisplayed || mApplicationsDisplayed)
    dialogCreateOptions.optionFlags |= kNavSupportPackages;		

  // Display the get file dialog. Only use a filter proc if there are any
  // filters registered.
  anErr = ::NavCreateGetFileDialog(
                                  &dialogCreateOptions,
                                  NULL, //  NavTypeListHandle
                                  eventProc,
                                  NULL, //  NavPreviewUPP
                                  mFilters.Count() ? filterProc : NULL,
                                  this, //  inClientData
                                  &dialog);
  if (anErr == noErr)
  {
    nsWatchTask::GetTask().Suspend();
    anErr = ::NavDialogRun(dialog);
    nsWatchTask::GetTask().Resume();

    if (anErr == noErr)
    {
    	NavReplyRecord reply;
      anErr = ::NavDialogGetReply(dialog, &reply);
      if (anErr == noErr && reply.validRecord)
      {
        long numItems;
        anErr = ::AECountItems((const AEDescList *)&reply.selection, &numItems);
        if (anErr == noErr)
        {
          for (long i = 1; i <= numItems; i ++)
          {
            AEKeyword   theKeyword;
            DescType    actualType;
            Size        actualSize;
            FSRef       theFSRef;

            // Get the FSRef for the file to be opened (or directory in case of a package)
            anErr = ::AEGetNthPtr(&(reply.selection), i, typeFSRef, &theKeyword, &actualType,
                                  &theFSRef, sizeof(theFSRef), &actualSize);
            if (anErr == noErr)
            {
              nsCOMPtr<nsILocalFile> localFile;
              NS_NewLocalFile(nsString(), PR_TRUE, getter_AddRefs(localFile));
              nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
              if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithFSRef(&theFSRef)))
                outFiles.AppendObject(localFile);
            }
          }

          if (outFiles.Count() > 0)
            retVal = returnOK;
        }
        
        // Some housekeeping for Nav Services 
        ::NavDisposeReply(&reply);
      }
    }
    ::NavDialogDispose(dialog);
  }
  
  // Free the CF objects from the dialogCreateOptions struct
  if (titleRef)
    CFRelease(titleRef);

  if (filterProc)
    ::DisposeNavObjectFilterUPP(filterProc);
	
  if ( eventProc )
    ::DisposeNavEventUPP(eventProc);
		
  return retVal;
} // GetFile


//-------------------------------------------------------------------------
//
// GetFolder
//
// Use NavServices to do a GetFolder. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, ioLocalFile is initialized to be the chosen folder.
//
//-------------------------------------------------------------------------
PRInt16
nsFilePicker::GetLocalFolder(const nsString& inTitle, nsILocalFile** outFile)
{
  NS_ENSURE_ARG_POINTER(outFile);
  *outFile = nsnull;
  
  PRInt16 retVal = returnCancel;
  NavEventUPP eventProc = NewNavEventUPP(FileDialogEventHandlerProc);  // doesn't really matter if this fails
  NavDialogRef dialog;
  NavDialogCreationOptions dialogCreateOptions;

  // Set defaults
  OSErr anErr = ::NavGetDefaultDialogCreationOptions(&dialogCreateOptions);
  if (anErr != noErr)
    return retVal;

  // Set the options for how the get file dialog will appear
  dialogCreateOptions.optionFlags |= kNavNoTypePopup;
  dialogCreateOptions.optionFlags |= kNavDontAutoTranslate;
  dialogCreateOptions.optionFlags |= kNavDontAddTranslateItems;
  dialogCreateOptions.optionFlags ^= kNavAllowMultipleFiles;
  dialogCreateOptions.modality = kWindowModalityAppModal;
  dialogCreateOptions.parentWindow = NULL;
  CFStringRef titleRef = CFStringCreateWithCharacters(NULL, (const UniChar *) inTitle.get(), inTitle.Length());
  dialogCreateOptions.windowTitle = titleRef;

  anErr = ::NavCreateChooseFolderDialog(
                                        &dialogCreateOptions,
                                        eventProc,
                                        NULL, // filter proc
                                        this, // inClientData
                                        &dialog);

  if (anErr == noErr)
  {
    nsWatchTask::GetTask().Suspend();  
    anErr = ::NavDialogRun(dialog);
    nsWatchTask::GetTask().Resume();  
    if (anErr == noErr)
    {
    	NavReplyRecord reply;
      anErr = ::NavDialogGetReply(dialog, &reply);
      if (anErr == noErr && reply.validRecord) {
        AEKeyword   theKeyword;
        DescType    actualType;
        Size        actualSize;
        FSRef       theFSRef;
         
        // Get the FSRef for the selected folder
        anErr = ::AEGetNthPtr(&(reply.selection), 1, typeFSRef, &theKeyword, &actualType,
                              &theFSRef, sizeof(theFSRef), &actualSize);
        if (anErr == noErr)
        {
          nsCOMPtr<nsILocalFile> localFile;
          NS_NewLocalFile(nsString(), PR_TRUE, getter_AddRefs(localFile));
          nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
          if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithFSRef(&theFSRef)))
          {
            *outFile = localFile;
            NS_ADDREF(*outFile);
            retVal = returnOK;
          }
        }
        // Some housekeeping for Nav Services 
        ::NavDisposeReply(&reply);
      }
    }
    ::NavDialogDispose(dialog);
  }
  
  // Free the CF objects from the dialogCreateOptions struct
  if (dialogCreateOptions.windowTitle)
    CFRelease(dialogCreateOptions.windowTitle);
	
  if (eventProc)
    ::DisposeNavEventUPP(eventProc);
		
  return retVal;
} // GetFolder

PRInt16
nsFilePicker::PutLocalFile(const nsString& inTitle, const nsString& inDefaultName, nsILocalFile** outFile)
{
  NS_ENSURE_ARG_POINTER(outFile);
  *outFile = nsnull;
  
  PRInt16 retVal = returnCancel;
  NavEventUPP eventProc = NewNavEventUPP(FileDialogEventHandlerProc);  // doesn't really matter if this fails
  OSType typeToSave = 'TEXT';
  OSType creatorToSave = (sCurrentProcessSignature == 0) ? 'MOZZ' : sCurrentProcessSignature;
  NavDialogRef dialog;
  NavDialogCreationOptions dialogCreateOptions;

  // Set defaults
  OSErr anErr = ::NavGetDefaultDialogCreationOptions(&dialogCreateOptions);
  if (anErr != noErr)
    return retVal;

  // Set the options for how the put file dialog will appear
  if (mTitles.Count() == 0)
  { // If we have no filter titles then suppress the save type popup
    dialogCreateOptions.optionFlags |= kNavNoTypePopup;
  }
  else
  {
    dialogCreateOptions.optionFlags &= ~kNavAllowStationery;  // remove Stationery option
    creatorToSave = kNavGenericSignature;   // This supresses the default format menu items
    SetupFormatMenuItems(&dialogCreateOptions);
  }
  
  dialogCreateOptions.optionFlags |= kNavDontAutoTranslate;
  dialogCreateOptions.optionFlags |= kNavDontAddTranslateItems;
  dialogCreateOptions.optionFlags ^= kNavAllowMultipleFiles;
  dialogCreateOptions.modality = kWindowModalityAppModal;
  dialogCreateOptions.parentWindow = NULL;
  CFStringRef titleRef = CFStringCreateWithCharacters(NULL, 
                                                      (const UniChar *) inTitle.get(), inTitle.Length());
  dialogCreateOptions.windowTitle = titleRef;
  CFStringRef defaultFileNameRef = CFStringCreateWithCharacters(NULL, 
                                                                (const UniChar *) inDefaultName.get(), 
                                                                inDefaultName.Length());
  dialogCreateOptions.saveFileName = defaultFileNameRef;

  anErr = ::NavCreatePutFileDialog(
                                   &dialogCreateOptions,
                                   typeToSave,
                                   creatorToSave,
                                   eventProc,
                                   this, // inClientData
                                   &dialog);

  if (anErr == noErr)
  {
    nsWatchTask::GetTask().Suspend();  
    anErr = ::NavDialogRun(dialog);
    nsWatchTask::GetTask().Resume();  

    if (anErr == noErr)
    {
    	NavReplyRecord reply;
      anErr = ::NavDialogGetReply(dialog, &reply);
      if (anErr == noErr && reply.validRecord)
      {
        AEKeyword   theKeyword;
        DescType    actualType;
        Size        actualSize;
        FSRef       theFSRef;

        // Create a CFURL of the file to be saved. The impl of InitWithCFURL in
        // nsLocalFileMac.cpp handles truncating a too-long file name since only
        // it has that problem.
        anErr = ::AEGetNthPtr(&(reply.selection), 1, typeFSRef, &theKeyword, &actualType,
                              &theFSRef, sizeof(theFSRef), &actualSize);
        if (anErr == noErr)
        {
          CFURLRef fileURL;
          CFURLRef parentURL = ::CFURLCreateFromFSRef(NULL, &theFSRef);
          if (parentURL)
          {
            fileURL = ::CFURLCreateCopyAppendingPathComponent(NULL, parentURL, reply.saveFileName, PR_FALSE);
            if (fileURL)
            {
              nsCOMPtr<nsILocalFile> localFile;
              NS_NewLocalFile(nsString(), PR_TRUE, getter_AddRefs(localFile));
              nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(localFile);
              if (macLocalFile && NS_SUCCEEDED(macLocalFile->InitWithCFURL(fileURL)))
              {
                *outFile = localFile;
                NS_ADDREF(*outFile);
                retVal = reply.replacing ? returnReplace : returnOK;
              }
              ::CFRelease(fileURL);
            }
            ::CFRelease(parentURL);
          }
        }  			  

        // Some housekeeping for Nav Services 
        ::NavCompleteSave(&reply, kNavTranslateInPlace);
        ::NavDisposeReply(&reply);
      }
    }
    ::NavDialogDispose(dialog);
  }  
  
  // Free the CF objects from the dialogCreateOptions struct
  if (dialogCreateOptions.windowTitle)
    CFRelease(dialogCreateOptions.windowTitle);
  if (dialogCreateOptions.saveFileName)
    CFRelease(dialogCreateOptions.saveFileName);
  if (dialogCreateOptions.popupExtension)
    CFRelease(dialogCreateOptions.popupExtension);
	
  if (eventProc)
    ::DisposeNavEventUPP(eventProc);
	
  return retVal;	
}

//
// MapFilterToFileTypes
//
// Take the list of file types (in a nice win32-specific format) and ask IC to give us 
// the MacOS file type codes for them.
//
void
nsFilePicker::MapFilterToFileTypes ( )
{
  nsCOMPtr<nsIInternetConfigService> icService ( do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID) );
  NS_ASSERTION(icService, "Can't get InternetConfig Service, bailing out");
  if ( !icService ) {
    // We couldn't get the IC Service, bail. Since |mAllFilesDisplayed| is still
    // set, the dialog will allow selection of all files. 
    return;
  }
  
  if (mFilters.Count())
  {
    // First we allocate the memory for the Mac type lists
    for (PRInt32 loop1 = 0; loop1 < mFilters.Count() && loop1 < kMaxTypeListCount; loop1++)
    {
      mTypeLists[loop1] =
        (NavTypeListPtr)NewPtrClear(sizeof(NavTypeList) + kMaxTypesPerFilter * sizeof(OSType));     
      if ( !mTypeLists[loop1] )
        return;                     // don't worry, we'll clean up in the dtor
    }
    
    // Now loop through each of the filter strings
    for (PRInt32 loop1 = 0; loop1 < mFilters.Count(); loop1++)
    {
      const nsString& filterWide = *mFilters[loop1];
      char* filter = ToNewCString(filterWide);

      NS_ASSERTION ( filterWide.Length(), "Oops. filepicker.properties not correctly installed");       

      // look for the flag indicating applications
      if (filterWide.EqualsLiteral("..apps"))
        mApplicationsDisplayed = PR_TRUE;

      if ( filterWide.Length() && filter )
      {
        PRUint32 filterIndex = 0;         // Index into the filter string
        PRUint32 typeTempIndex = 0;       // Index into the temp string for a single filter type
        PRUint32 typesInThisFilter = 0;   // Count for # of types in this filter
        bool finishedThisFilter = false;  // Flag so we know when we're finsihed with the filter
        char typeTemp[256];
        char tempChar;           // char we're currently looking at

        // Loop through the characters of filter string. Every time we get to a
        // semicolon (or a null, meaning we've hit the end of the full string)
        // then we've found the filter and can pass it off to IC to get a macOS 
        // file type out of it.
        do
        {
          tempChar = filter[filterIndex];
          if ((tempChar == ';') || (tempChar == 0)) {   // End of filter type reached
            typeTemp[typeTempIndex] = '\0';             // null terminate
            
            // to make it easier to match file extensions while we're filtering, flatten
            // out the list. Ignore filters that are just "*" and also remove the
            // leading "*" from filters we do add.
            if ( strlen(typeTemp) > 1 )
              mFlatFilters.AppendCString ( nsCString((char*)&typeTemp[1]) );  // cut out the "*"
            
            // ask IC if it's not "all files" (designated by "*")
            if ( !(typeTemp[1] == '\0' && typeTemp[0] == '*') ) {
              mAllFilesDisplayed = PR_FALSE;
              
              nsCOMPtr<nsIMIMEInfo> icEntry;
              icService->GetMIMEInfoFromExtension(typeTemp, getter_AddRefs(icEntry));
              if ( icEntry )
              {
                bool addToList = true;
                OSType tempOSType;
                icEntry->GetMacType(NS_REINTERPRET_CAST(PRUint32*, (&tempOSType)));
                for (PRUint32 typeIndex = 0; typeIndex < typesInThisFilter; typeIndex++)
                {
                  if (mTypeLists[loop1]->osType[typeIndex] == tempOSType)
                  {
                    addToList = false;
                    break;
                  }
                }
                if (addToList && typesInThisFilter < kMaxTypesPerFilter)
                  mTypeLists[loop1]->osType[typesInThisFilter++] = tempOSType;
              }
            } // if not "*"
            
            typeTempIndex = 0;      // Reset the temp string for the type
            if (tempChar == '\0')
              finishedThisFilter = true;
          }
          else
          {
            // strip out whitespace as we go
            if ( tempChar != ' ' )
              typeTemp[typeTempIndex++] = tempChar;
          }
          
          filterIndex++;
        } while (!finishedThisFilter);
        
        // Set how many OSTypes we actually found
        mTypeLists[loop1]->osTypeCount = typesInThisFilter;
        
        nsMemory::Free ( NS_REINTERPRET_CAST(void*, filter) );
      }
    }
  }

} // MapFilterToFileTypes


//-------------------------------------------------------------------------
// Util func to take the array of mTitles and make it into something
// Nav Services can use.
// This is the TARGET_CARBON version for Nav Services 3.0
//-------------------------------------------------------------------------
void nsFilePicker::SetupFormatMenuItems (NavDialogCreationOptions* dialogCreateOptions)
{
  PRInt32   numMenuItems = mTitles.Count();
  PRInt32   index;
  CFStringRef      itemStr = NULL;
  CFArrayCallBacks callBacks = kCFTypeArrayCallBacks;
  
  // Under NavServices 3.0 the popupExtension is actually a CFArray of CFStrings
  dialogCreateOptions->popupExtension = CFArrayCreateMutable(kCFAllocatorDefault, numMenuItems, &callBacks);
  
  if (dialogCreateOptions->popupExtension)
  {
    for (index = 0; index < numMenuItems; ++index)
    {
      const nsString& titleWide = *mTitles[index];
      itemStr = CFStringCreateWithCharacters(NULL, (const unsigned short *)titleWide.get(), titleWide.Length());
      CFArrayInsertValueAtIndex((CFMutableArrayRef)dialogCreateOptions->popupExtension, index, (void*)itemStr);
      CFRelease(itemStr);
    }
  }
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
// Get the filter index
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  *aFilterIndex = mSelectedType;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the filter index
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  mSelectedType = aFilterIndex;
  return NS_OK;
}

