/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
  nsPluginsDirMac.cpp
  
  Mac OS implementation of the nsPluginsDir/nsPluginsFile classes.
  
  by Patrick C. Beard.
 */

#include "nsPluginsDir.h"
#include "prlink.h"
#include "ns4xPlugin.h"
#include "nsPluginsDirUtils.h"

#include <Processes.h>
#include <Folders.h>
#include <Resources.h>
#include <TextUtils.h>
#include <Aliases.h>
#include <string.h>
#if TARGET_CARBON && (UNIVERSAL_INTERFACES_VERSION < 0x0340)
enum {
  kLocalDomain                  = -32765, /* All users of a single machine have access to these resources.*/
  kUserDomain                   = -32763, /* Read/write. Resources that are private to the user.*/
  kClassicDomain                = -32762 /* Domain referring to the currently configured Classic System Folder*/
};
#endif

typedef NS_4XPLUGIN_CALLBACK(const char *, NP_GETMIMEDESCRIPTION) ();
typedef NS_4XPLUGIN_CALLBACK(OSErr, BP_GETSUPPORTEDMIMETYPES) (BPSupportedMIMETypes *mimeInfo, UInt32 flags);

#if TARGET_CARBON
#include <CFURL.h>
#include <CFBundle.h>
#include <CFString.h>
#include <CodeFragments.h>

/*
** Returns a CFBundleRef if the FSSpec refers to a Mac OS X bundle directory.
** The caller is responsible for calling CFRelease() to deallocate.
*/
static CFBundleRef getPluginBundle(const FSSpec& spec)
{
    CFBundleRef bundle = NULL;
    FSRef ref;
    OSErr err = FSpMakeFSRef(&spec, &ref);
    char path[512];
    if (err == noErr && (UInt32(FSRefMakePath) != kUnresolvedCFragSymbolAddress)) {
        err = FSRefMakePath(&ref, (UInt8*)path, sizeof(path) - 1);
        if (err == noErr) {
            CFStringRef pathRef = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
            if (pathRef) {
              CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, pathRef, kCFURLPOSIXPathStyle, true);
              if (bundleURL != NULL) {
                    bundle = CFBundleCreate(NULL, bundleURL);
                    CFRelease(bundleURL);
              }
              CFRelease(pathRef);
            }
        }
    }
    return bundle;
}

extern "C" {
// Not yet in Universal Interfaces that I'm using.
EXTERN_API_C( SInt16 )
CFBundleOpenBundleResourceMap(CFBundleRef bundle);

EXTERN_API_C( void )
CFBundleGetPackageInfo(CFBundleRef bundle, UInt32 * packageType, UInt32 * packageCreator);
}

#endif

static nsresult getApplicationSpec(FSSpec& outAppSpec)
{
  // Use the process manager to get the application's FSSpec,
  // then construct an nsFileSpec that encapsulates it.
  ProcessInfoRec info;
  info.processInfoLength = sizeof(info);
  info.processName = NULL;
  info.processAppSpec = &outAppSpec;
  ProcessSerialNumber psn = { 0, kCurrentProcess };
  OSErr result = GetProcessInformation(&psn, &info);
  return (result == noErr ? NS_OK : NS_ERROR_FAILURE);
}

PRBool nsPluginsDir::IsPluginFile(const nsFileSpec& fileSpec)
{
  // look at file's creator/type and make sure it is a code fragment, etc.
  FInfo info;
  const FSSpec& spec = fileSpec;
  OSErr result = FSpGetFInfo(&spec, &info);
  if (result == noErr && ((info.fdType == 'shlb' && info.fdCreator == 'MOSS') ||
                           info.fdType == 'NSPL'))
      return PR_TRUE;

#if TARGET_CARBON
  // Some additional plugin types for Carbon/Mac OS X
  if (result == noErr && (info.fdType == 'BRPL' || info.fdType == 'IEPL'))
      return PR_TRUE;

  // for Mac OS X bundles.
  CFBundleRef bundle = getPluginBundle(spec);
  if (bundle) {
    UInt32 packageType, packageCreator;
    CFBundleGetPackageInfo(bundle, &packageType, &packageCreator);
    CFRelease(bundle);
    switch (packageType) {
    case 'BRPL':
    case 'IEPL':
    case 'NSPL':
      return PR_TRUE;
    }
  }
#endif

  return PR_FALSE;
}

nsPluginFile::nsPluginFile(const nsFileSpec& spec)
  : nsFileSpec(spec)
{
}

nsPluginFile::~nsPluginFile() {}

/**
 * Loads the plugin into memory using NSPR's shared-library loading
 * mechanism. Handles platform differences in loading shared libraries.
 */
nsresult nsPluginFile::LoadPlugin(PRLibrary* &outLibrary)
{
  const char* path = this->GetCString();
  outLibrary = PR_LoadLibrary(path);
  pLibrary = outLibrary;
  return NS_OK;
}

static char* p2cstrdup(StringPtr pstr)
{
  int len = pstr[0];
  char* cstr = new char[len + 1];
  if (cstr != NULL) {
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

static char* GetPluginStringFromResource(short id, short index)
{
  Str255 str;
  ::GetIndString(str, id, index);
  return p2cstrdup(str);
}

// Opens the resource fork for the plugin
// Also checks if the plugin is a CFBundle and opens gets the correct resource
short nsPluginFile::OpenPluginResource()
{
  FSSpec spec = *this;
  Boolean targetIsFolder, wasAliased;
  OSErr err = ::ResolveAliasFile(&spec, true, &targetIsFolder, &wasAliased);
  short refNum = ::FSpOpenResFile(&spec, fsRdPerm);
  
#if TARGET_CARBON
  if (refNum == -1) {
    CFBundleRef bundle = getPluginBundle(spec);
    if (bundle) {
      refNum = CFBundleOpenBundleResourceMap(bundle);
      CFRelease(bundle);
    }
  }
#endif
  
  return refNum;
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
   // clear out the info, except for the first field.
   memset(&info.fName, 0, sizeof(info) - sizeof(PRUint32));

  // need to open the plugin's resource file and read some resources.
  short refNum = OpenPluginResource();

  if (refNum != -1) {
    if (info.fPluginInfoSize >= sizeof(nsPluginInfo)) {
      // 'STR#', 126, 2 => plugin name.
      info.fName = GetPluginStringFromResource(126, 2);
      
      // 'STR#', 126, 1 => plugin description.
      info.fDescription = GetPluginStringFromResource(126, 1);
      
      FSSpec spec = *this;
      info.fFileName = p2cstrdup(spec.name);
      info.fFullPath = PL_strdup(this->GetCString());
#if TARGET_CARBON
      CFBundleRef bundle = getPluginBundle(spec);
      if (bundle) {
        info.fBundle = PR_TRUE;
        CFRelease(bundle);
        } else
        info.fBundle = PR_FALSE;
#endif

      // It's possible that our plugin has 2 special extra entry points that'll give us more
      // mime type info. Quicktime does this to get around the need of having admin rights
      // to change mime info in the resource fork. We need to use this info instead of the
      // resource. See bug 113464.
      BPSupportedMIMETypes mi = {kBPSupportedMIMETypesStructVers_1, NULL, NULL};
      if (pLibrary) {

        // First, check for NP_GetMIMEDescription
        NP_GETMIMEDESCRIPTION pfnGetMimeDesc = 
          (NP_GETMIMEDESCRIPTION)PR_FindSymbol(pLibrary, NP_GETMIMEDESCRIPTION_NAME); 
        if (pfnGetMimeDesc) {
          nsresult rv = NS_ERROR_FAILURE;
#if TARGET_CARBON
          rv = ParsePluginMimeDescription(pfnGetMimeDesc(), info);
#else
          const char *ret = CallNP_GetMIMEDescEntryProc(pfnGetMimeDesc);
          if (ret)
            rv= ParsePluginMimeDescription(ret, info);
#endif
          if (NS_SUCCEEDED(rv)) {    // if we could parse the mime types from NP_GetMIMEDescription,
            ::CloseResFile(refNum);  // we've got what we need, close the resource, we're done
            return rv;
          }
        }

        // Second, on OSX, next check for mime info from BP_GetSupportedMIMETypes
#if TARGET_CARBON
        BP_GETSUPPORTEDMIMETYPES pfnMime = 
          (BP_GETSUPPORTEDMIMETYPES)PR_FindSymbol(pLibrary, "BP_GetSupportedMIMETypes");
        if (pfnMime && noErr == pfnMime(&mi, 0) && mi.typeStrings) {        
          info.fVariantCount = (**(short**)mi.typeStrings) / 2;
          ::HLock(mi.typeStrings);
          if (mi.infoStrings)  // it's possible some plugins have infoStrings missing
            ::HLock(mi.infoStrings);
        }
#endif
      }
      
      // Last, we couldn't get info from an extra entry point for some reason, 
      // Lets get info from normal resources
      if (!info.fVariantCount) {
        mi.typeStrings = ::Get1Resource('STR#', 128);
        if (mi.typeStrings) {
          info.fVariantCount = (**(short**)mi.typeStrings) / 2;
          ::DetachResource(mi.typeStrings);
          ::HLock(mi.typeStrings);
        } else {
          // Don't add this plugin because no mime types could be found
          ::CloseResFile(refNum);
          return NS_ERROR_FAILURE;
        }

        mi.infoStrings = ::Get1Resource('STR#', 127);
        if (mi.infoStrings) {
          ::DetachResource(mi.infoStrings);
          ::HLock(mi.infoStrings);
        }
      }

      // fill-in rest of info struct
      int variantCount = info.fVariantCount;
      info.fMimeTypeArray  = new char*[variantCount];
      info.fExtensionArray = new char*[variantCount];      
      if (mi.infoStrings)
        info.fMimeDescriptionArray = new char*[variantCount];

      short mimeIndex = 2, descriptionIndex = 2;
      for (int i = 0; i < variantCount; i++) {
        info.fMimeTypeArray[i]          = GetNextPluginStringFromHandle(mi.typeStrings, &mimeIndex);
        info.fExtensionArray[i]         = GetNextPluginStringFromHandle(mi.typeStrings, &mimeIndex);
        if (mi.infoStrings)
          info.fMimeDescriptionArray[i] = GetNextPluginStringFromHandle(mi.infoStrings, &descriptionIndex);
      }

      ::HUnlock(mi.typeStrings);
      ::DisposeHandle(mi.typeStrings);
      if (mi.infoStrings) {
        ::HUnlock(mi.infoStrings);      
        ::DisposeHandle(mi.infoStrings);
      }
    }
    
    ::CloseResFile(refNum);
  }
  return NS_OK;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  if (info.fPluginInfoSize <= sizeof(nsPluginInfo)) {
    delete[] info.fName;
    delete[] info.fDescription;

    int variantCount = info.fVariantCount;
    for (int i = 0; i < variantCount; i++) {
      delete[] info.fMimeTypeArray[i];
      delete[] info.fExtensionArray[i];
    }
    delete[] info.fMimeTypeArray;

    if (info.fMimeDescriptionArray) {
      for (int i = 0; i < variantCount; i++) {
        delete[] info.fMimeDescriptionArray[i];
      }
      delete[] info.fMimeDescriptionArray;
    }

    delete[] info.fExtensionArray;
    delete[] info.fFileName;
    delete[] info.fFullPath;
  }
  return NS_OK;
}
