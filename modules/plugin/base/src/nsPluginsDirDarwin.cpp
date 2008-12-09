/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
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

/*
  nsPluginsDirDarwin.cpp
  
  Mac OS X implementation of the nsPluginsDir/nsPluginsFile classes.
  
  by Patrick C. Beard.
 */

#include "prlink.h"
#include "prnetdb.h"
#include "nsXPCOM.h"

#include "nsPluginsDir.h"
#include "nsNPAPIPlugin.h"
#include "nsPluginsDirUtils.h"

#include "nsILocalFileMac.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

typedef NS_NPAPIPLUGIN_CALLBACK(const char *, NP_GETMIMEDESCRIPTION) ();
typedef NS_NPAPIPLUGIN_CALLBACK(OSErr, BP_GETSUPPORTEDMIMETYPES) (BPSupportedMIMETypes *mimeInfo, UInt32 flags);


/*
** Returns a CFBundleRef if the FSSpec refers to a Mac OS X bundle directory.
** The caller is responsible for calling CFRelease() to deallocate.
*/
static CFBundleRef getPluginBundle(const char* path)
{
    CFBundleRef bundle = NULL;
    CFStringRef pathRef = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    if (pathRef) {
        CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, pathRef, kCFURLPOSIXPathStyle, true);
        if (bundleURL) {
            bundle = CFBundleCreate(NULL, bundleURL);
            CFRelease(bundleURL);
        }
        CFRelease(pathRef);
    }
    return bundle;
}

static OSErr toFSSpec(nsIFile* file, FSSpec& outSpec)
{
    nsCOMPtr<nsILocalFileMac> lfm = do_QueryInterface(file);
    if (!lfm)
        return -1;
    FSSpec foo;
    lfm->GetFSSpec(&foo);
    outSpec = foo;

    return NS_OK;
}

static nsresult toCFURLRef(nsIFile* file, CFURLRef& outURL)
{
  nsCOMPtr<nsILocalFileMac> lfm = do_QueryInterface(file);
  if (!lfm)
    return NS_ERROR_FAILURE;
  CFURLRef url;
  nsresult rv = lfm->GetCFURL(&url);
  if (NS_SUCCEEDED(rv))
    outURL = url;
  
  return rv;
}

// function to test whether or not this is a loadable plugin
static PRBool IsLoadablePlugin(CFURLRef aURL)
{
  if (!aURL)
    return PR_FALSE;
  
  PRBool isLoadable = PR_FALSE;
  char path[PATH_MAX];
  if (CFURLGetFileSystemRepresentation(aURL, TRUE, (UInt8*)path, sizeof(path))) {
    UInt32 magic;
    int f = open(path, O_RDONLY);
    if (f != -1) {
      // Mach-O headers use the byte ordering of the architecture on which
      // they run, so test against the magic number in the byte order
      // we're compiling for. Fat headers are always big-endian, so swap
      // them to host before comparing to host representation of the magic
      if (read(f, &magic, sizeof(magic)) == sizeof(magic)) {
        if ((magic == MH_MAGIC) || (PR_ntohl(magic) == FAT_MAGIC))
          isLoadable = PR_TRUE;
      }
      close(f);
    }
  }
  return isLoadable;
}

PRBool nsPluginsDir::IsPluginFile(nsIFile* file)
{
  nsCString temp;
  file->GetNativeLeafName(temp);
  /*
   * Don't load the VDP fake plugin, to avoid tripping a bad bug in OS X
   * 10.5.3 (see bug 436575).
   */
  if (!strcmp(temp.get(), "VerifiedDownloadPlugin.plugin")) {
    NS_WARNING("Preventing load of VerifiedDownloadPlugin.plugin (see bug 436575)");
    return PR_FALSE;
  }
    
  CFURLRef pluginURL = NULL;
  if (NS_FAILED(toCFURLRef(file, pluginURL)))
    return PR_FALSE;
  
  PRBool isPluginFile = PR_FALSE;
  
  CFBundleRef pluginBundle = CFBundleCreate(kCFAllocatorDefault, pluginURL);
  if (pluginBundle) {
    UInt32 packageType, packageCreator;
    CFBundleGetPackageInfo(pluginBundle, &packageType, &packageCreator);
    if (packageType == 'BRPL' || packageType == 'IEPL' || packageType == 'NSPL') {
      CFURLRef executableURL = CFBundleCopyExecutableURL(pluginBundle);
      if (executableURL) {
        isPluginFile = IsLoadablePlugin(executableURL);
        CFRelease(executableURL);
      }
    }
    CFRelease(pluginBundle);
  }
  else {
    LSItemInfoRecord info;
    if (LSCopyItemInfoForURL(pluginURL, kLSRequestTypeCreator, &info) == noErr) {
      if ((info.filetype == 'shlb' && info.creator == 'MOSS') ||
          info.filetype == 'NSPL' ||
          info.filetype == 'BRPL' ||
          info.filetype == 'IEPL') {
        isPluginFile = IsLoadablePlugin(pluginURL);
      }
    }
  }
  
  CFRelease(pluginURL);
  return isPluginFile;
}

// Caller is responsible for freeing returned buffer.
static char* CFStringRefToUTF8Buffer(CFStringRef cfString)
{
  int bufferLength = ::CFStringGetLength(cfString) + 1;
  char* newBuffer = static_cast<char*>(NS_Alloc(bufferLength));
  if (newBuffer && !::CFStringGetCString(cfString, newBuffer, bufferLength, kCFStringEncodingUTF8)) {
    NS_Free(newBuffer);
    newBuffer = nsnull;
  }
  return newBuffer;
}

static void ParsePlistPluginInfo(nsPluginInfo& info, CFBundleRef bundle)
{
  CFTypeRef mimeDict = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginMIMETypes"));
  if (mimeDict && ::CFGetTypeID(mimeDict) == ::CFDictionaryGetTypeID() && ::CFDictionaryGetCount(static_cast<CFDictionaryRef>(mimeDict)) > 0) {
    int mimeDictKeyCount = ::CFDictionaryGetCount(static_cast<CFDictionaryRef>(mimeDict));

    // Allocate memory for mime data
    int mimeDataArraySize = mimeDictKeyCount * sizeof(char*);
    info.fMimeTypeArray = static_cast<char**>(NS_Alloc(mimeDataArraySize));
    if (!info.fMimeTypeArray)
      return;
    memset(info.fMimeTypeArray, 0, mimeDataArraySize);
    info.fExtensionArray = static_cast<char**>(NS_Alloc(mimeDataArraySize));
    if (!info.fExtensionArray)
      return;
    memset(info.fExtensionArray, 0, mimeDataArraySize);
    info.fMimeDescriptionArray = static_cast<char**>(NS_Alloc(mimeDataArraySize));
    if (!info.fMimeDescriptionArray)
      return;
    memset(info.fMimeDescriptionArray, 0, mimeDataArraySize);

    // Allocate memory for mime dictionary keys and values
    nsAutoArrayPtr<CFTypeRef> keys(new CFTypeRef[mimeDictKeyCount]);
    if (!keys)
      return;
    nsAutoArrayPtr<CFTypeRef> values(new CFTypeRef[mimeDictKeyCount]);
    if (!values)
      return;

    // Set the variant count now that we have safely allocated memory
    info.fVariantCount = mimeDictKeyCount;

    ::CFDictionaryGetKeysAndValues(static_cast<CFDictionaryRef>(mimeDict), keys, values);
    for (int i = 0; i < mimeDictKeyCount; i++) {
      CFTypeRef mimeString = keys[i];
      if (mimeString && ::CFGetTypeID(mimeString) == ::CFStringGetTypeID()) {
        info.fMimeTypeArray[i] = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(mimeString));
      }
      else {
        info.fVariantCount -= 1;
        continue;
      }
      CFTypeRef mimeDict = values[i];
      if (mimeDict && ::CFGetTypeID(mimeDict) == ::CFDictionaryGetTypeID()) {
        CFTypeRef extensions = ::CFDictionaryGetValue(static_cast<CFDictionaryRef>(mimeDict), CFSTR("WebPluginExtensions"));
        if (extensions && ::CFGetTypeID(extensions) == ::CFArrayGetTypeID()) {
          int extensionCount = ::CFArrayGetCount(static_cast<CFArrayRef>(extensions));
          CFMutableStringRef extensionList = ::CFStringCreateMutable(kCFAllocatorDefault, 0);
          for (int j = 0; j < extensionCount; j++) {
            CFTypeRef extension = ::CFArrayGetValueAtIndex(static_cast<CFArrayRef>(extensions), j);
            if (extension && ::CFGetTypeID(extension) == ::CFStringGetTypeID()) {
              if (j > 0)
                ::CFStringAppend(extensionList, CFSTR(","));
              ::CFStringAppend(static_cast<CFMutableStringRef>(extensionList), static_cast<CFStringRef>(extension));
            }
          }
          info.fExtensionArray[i] = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(extensionList));
          ::CFRelease(extensionList);
        }
        CFTypeRef description = ::CFDictionaryGetValue(static_cast<CFDictionaryRef>(mimeDict), CFSTR("WebPluginTypeDescription"));
        if (description && ::CFGetTypeID(description) == ::CFStringGetTypeID())
          info.fMimeDescriptionArray[i] = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(description));
      }
    }
  }
}

nsPluginFile::nsPluginFile(nsIFile *spec)
    : mPlugin(spec)
{
}

nsPluginFile::~nsPluginFile() {}

/**
 * Loads the plugin into memory using NSPR's shared-library loading
 * mechanism. Handles platform differences in loading shared libraries.
 */
nsresult nsPluginFile::LoadPlugin(PRLibrary* &outLibrary)
{
    const char* path;

    if (!mPlugin)
        return NS_ERROR_NULL_POINTER;

    nsCAutoString temp;
    mPlugin->GetNativePath(temp);
    path = temp.get();

    outLibrary = PR_LoadLibrary(path);
    pLibrary = outLibrary;
    if (!outLibrary) {
        return NS_ERROR_FAILURE;
    }
#ifdef DEBUG
    printf("[loaded plugin %s]\n", path);
#endif
    return NS_OK;
}

static char* p2cstrdup(StringPtr pstr)
{
    int len = pstr[0];
    char* cstr = static_cast<char*>(NS_Alloc(len + 1));
    if (cstr) {
        ::BlockMoveData(pstr + 1, cstr, len);
        cstr[len] = '\0';
    }
    return cstr;
}

static char* GetNextPluginStringFromHandle(Handle h, short *index)
{
  char *ret = p2cstrdup((unsigned char*)(*h + *index));
  *index += (ret ? PL_strlen(ret) : 0) + 1;
  return ret;
}

static char* GetPluginString(short id, short index)
{
    Str255 str;
    ::GetIndString(str, id, index);
    return p2cstrdup(str);
}

// Opens the resource fork for the plugin
// Also checks if the plugin is a CFBundle and opens gets the correct resource
static short OpenPluginResourceFork(nsIFile *pluginFile)
{
  FSSpec spec;
  OSErr err = toFSSpec(pluginFile, spec);
  Boolean targetIsFolder, wasAliased;
  err = ::ResolveAliasFile(&spec, true, &targetIsFolder, &wasAliased);
  short refNum = ::FSpOpenResFile(&spec, fsRdPerm);
  if (refNum < 0) {
    nsCString path;
    pluginFile->GetNativePath(path);
    CFBundleRef bundle = getPluginBundle(path.get());
    if (bundle) {
      refNum = CFBundleOpenBundleResourceMap(bundle);
      CFRelease(bundle);
    }
  }
  return refNum;
}

short nsPluginFile::OpenPluginResource()
{
  return OpenPluginResourceFork(mPlugin);
}

class nsAutoCloseResourceObject {
public:
  nsAutoCloseResourceObject(nsIFile *pluginFile)
  {
    mRefNum = OpenPluginResourceFork(pluginFile);
  }
  ~nsAutoCloseResourceObject()
  {
    if (mRefNum > 0)
      ::CloseResFile(mRefNum);
  }
  PRBool ResourceOpened()
  {
    return (mRefNum > 0);
  }
private:
  short mRefNum;
};

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
  // clear out the info, except for the first field.
  memset(&info.fName, 0, sizeof(info) - sizeof(PRUint32));

  if (info.fPluginInfoSize < sizeof(nsPluginInfo))
    return NS_ERROR_FAILURE;

  // First open up resource we can use to get plugin info.

  // Try to open a resource fork.
  nsAutoCloseResourceObject resourceObject(mPlugin);
  bool resourceOpened = resourceObject.ResourceOpened();
  // Try to get a bundle reference.
  nsCString path;
  mPlugin->GetNativePath(path);
  CFBundleRef bundle = getPluginBundle(path.get());

  // Get fBundle
  if (bundle)
    info.fBundle = PR_TRUE;

  // Get fName
  if (bundle) {
    CFTypeRef name = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginName"));
    if (name && ::CFGetTypeID(name) == ::CFStringGetTypeID())
      info.fName = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(name));
  }
  if (!info.fName && resourceOpened) {
    // 'STR#', 126, 2 => plugin name.
    info.fName = GetPluginString(126, 2);
  }

  // Get fDescription
  if (bundle) {
    CFTypeRef description = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginDescription"));
    if (description && ::CFGetTypeID(description) == ::CFStringGetTypeID())
      info.fDescription = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(description));
  }
  if (!info.fDescription && resourceOpened) {
    // 'STR#', 126, 1 => plugin description.
    info.fDescription = GetPluginString(126, 1);
  }

  // Get fFileName
  FSSpec spec;
  toFSSpec(mPlugin, spec);
  info.fFileName = p2cstrdup(spec.name);

  // Get fFullPath
  info.fFullPath = PL_strdup(path.get());

  // Get fVersion
  if (bundle) {
    // Look for the release version first
    CFTypeRef version = ::CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleShortVersionString"));
    if (!version) // try the build version
      version = ::CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleVersionKey);
    if (version && ::CFGetTypeID(version) == ::CFStringGetTypeID())
      info.fVersion = CFStringRefToUTF8Buffer(static_cast<CFStringRef>(version));
  }

  // The last thing we need to do is get MIME data
  // fVariantCount, fMimeTypeArray, fExtensionArray, fMimeDescriptionArray

  // First look for data in a bundle plist
  if (bundle) {
    ParsePlistPluginInfo(info, bundle);
    ::CFRelease(bundle);
    if (info.fVariantCount > 0)
      return NS_OK;    
  }

  // It's possible that our plugin has 2 entry points that'll give us mime type
  // info. Quicktime does this to get around the need of having admin rights to
  // change mime info in the resource fork. We need to use this info instead of
  // the resource. See bug 113464.

  // Try to get data from NP_GetMIMEDescription
  if (pLibrary) {
    NP_GETMIMEDESCRIPTION pfnGetMimeDesc = (NP_GETMIMEDESCRIPTION)PR_FindFunctionSymbol(pLibrary, NP_GETMIMEDESCRIPTION_NAME); 
    if (pfnGetMimeDesc)
      ParsePluginMimeDescription(pfnGetMimeDesc(), info);
    if (info.fVariantCount)
      return NS_OK;
  }

  // We'll fill this in using BP_GetSupportedMIMETypes and/or resource fork data
  BPSupportedMIMETypes mi = {kBPSupportedMIMETypesStructVers_1, NULL, NULL};

  // Try to get data from BP_GetSupportedMIMETypes
  if (pLibrary) {
    BP_GETSUPPORTEDMIMETYPES pfnMime = (BP_GETSUPPORTEDMIMETYPES)PR_FindFunctionSymbol(pLibrary, "BP_GetSupportedMIMETypes");
    if (pfnMime && noErr == pfnMime(&mi, 0) && mi.typeStrings) {
      info.fVariantCount = (**(short**)mi.typeStrings) / 2;
      ::HLock(mi.typeStrings);
      if (mi.infoStrings)  // it's possible some plugins have infoStrings missing
        ::HLock(mi.infoStrings);
    }
  }

  // Try to get data from the resource fork
  if (!info.fVariantCount && resourceObject.ResourceOpened()) {
    mi.typeStrings = ::Get1Resource('STR#', 128);
    if (mi.typeStrings) {
      info.fVariantCount = (**(short**)mi.typeStrings) / 2;
      ::DetachResource(mi.typeStrings);
      ::HLock(mi.typeStrings);
    } else {
      // Don't add this plugin because no mime types could be found
      return NS_ERROR_FAILURE;
    }
    
    mi.infoStrings = ::Get1Resource('STR#', 127);
    if (mi.infoStrings) {
      ::DetachResource(mi.infoStrings);
      ::HLock(mi.infoStrings);
    }
  }

  //XXX FIXME: past this point some (unlikely) error cases will leak memory
  // (leak is bug 462023)

  // Fill in the info struct based on the data in the BPSupportedMIMETypes struct
  int variantCount = info.fVariantCount;
  info.fMimeTypeArray = static_cast<char**>(NS_Alloc(variantCount * sizeof(char*)));
  if (!info.fMimeTypeArray)
    return NS_ERROR_OUT_OF_MEMORY;
  info.fExtensionArray = static_cast<char**>(NS_Alloc(variantCount * sizeof(char*)));
  if (!info.fExtensionArray)
    return NS_ERROR_OUT_OF_MEMORY;
  if (mi.infoStrings) {
    info.fMimeDescriptionArray = static_cast<char**>(NS_Alloc(variantCount * sizeof(char*)));
    if (!info.fMimeDescriptionArray)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  short mimeIndex = 2;
  short descriptionIndex = 2;
  for (int i = 0; i < variantCount; i++) {
    info.fMimeTypeArray[i] = GetNextPluginStringFromHandle(mi.typeStrings, &mimeIndex);
    info.fExtensionArray[i] = GetNextPluginStringFromHandle(mi.typeStrings, &mimeIndex);
    if (mi.infoStrings)
      info.fMimeDescriptionArray[i] = GetNextPluginStringFromHandle(mi.infoStrings, &descriptionIndex);
  }

  ::HUnlock(mi.typeStrings);
  ::DisposeHandle(mi.typeStrings);
  if (mi.infoStrings) {
    ::HUnlock(mi.infoStrings);
    ::DisposeHandle(mi.infoStrings);
  }

  return NS_OK;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  if (info.fPluginInfoSize <= sizeof(nsPluginInfo)) {
    NS_Free(info.fName);
    NS_Free(info.fDescription);
    int variantCount = info.fVariantCount;
    for (int i = 0; i < variantCount; i++) {
      NS_Free(info.fMimeTypeArray[i]);
      NS_Free(info.fExtensionArray[i]);
      NS_Free(info.fMimeDescriptionArray[i]);
    }
    NS_Free(info.fMimeTypeArray);
    NS_Free(info.fMimeDescriptionArray);
    NS_Free(info.fExtensionArray);
    NS_Free(info.fFileName);
    NS_Free(info.fFullPath);
    NS_Free(info.fVersion);
  }

  return NS_OK;
}
