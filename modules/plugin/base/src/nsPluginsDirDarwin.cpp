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
 *   Patrick C. Beard <beard@netscape.com>
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
  nsPluginsDirDarwin.cpp
  
  Mac OS X implementation of the nsPluginsDir/nsPluginsFile classes.
  
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

#include <CFURL.h>
#include <CFBundle.h>
#include <CFString.h>
#include <CodeFragments.h>

typedef NS_4XPLUGIN_CALLBACK(const char *, NP_GETMIMEDESCRIPTION) ();
typedef NS_4XPLUGIN_CALLBACK(OSErr, BP_GETSUPPORTEDMIMETYPES) (BPSupportedMIMETypes *mimeInfo, UInt32 flags);


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
        if (bundleURL != NULL) {
            bundle = CFBundleCreate(NULL, bundleURL);
            CFRelease(bundleURL);
        }
        CFRelease(pathRef);
    }
    return bundle;
}

static OSErr toFSSpec(const nsFileSpec& inFileSpec, FSSpec& outSpec)
{
    FSRef ref;
    OSErr err = FSPathMakeRef((const UInt8*)inFileSpec.GetCString(), &ref, NULL);
    if (err == noErr)
        err = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL, &outSpec, NULL);
    return err;
}

PRBool nsPluginsDir::IsPluginFile(const nsFileSpec& fileSpec)
{
#ifdef DEBUG
    printf("nsPluginsDir::IsPluginFile:  checking %s\n", fileSpec.GetCString());
#endif
    // look at file's creator/type and make sure it is a code fragment, etc.
    FSSpec spec;
    OSErr err = toFSSpec(fileSpec, spec);
    if (err != noErr)
        return PR_FALSE;

    FInfo info;
    err = FSpGetFInfo(&spec, &info);
    if (err == noErr && ((info.fdType == 'shlb' && info.fdCreator == 'MOSS') ||
                         info.fdType == 'NSPL')) {
#ifdef DEBUG
        printf("found plugin '%s'.\n", fileSpec.GetCString());
#endif
        return PR_TRUE;
    }

    // Some additional plugin types for Carbon/Mac OS X
    if (err == noErr && (info.fdType == 'BRPL' || info.fdType == 'IEPL'))
        return PR_TRUE;

    // for Mac OS X bundles.
    CFBundleRef bundle = getPluginBundle(fileSpec.GetCString());
    if (bundle) {
        UInt32 packageType, packageCreator;
        CFBundleGetPackageInfo(bundle, &packageType, &packageCreator);
        CFRelease(bundle);
        switch (packageType) {
        case 'BRPL':
        case 'IEPL':
        case 'NSPL':
#ifdef DEBUG
            printf("found bundled plugin '%s'.\n", fileSpec.GetCString());
#endif
            return PR_TRUE;
        }
    }

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

static char* GetPluginString(short id, short index)
{
    Str255 str;
    ::GetIndString(str, id, index);
    return p2cstrdup(str);
}

// Opens the resource fork for the plugin
// Also checks if the plugin is a CFBundle and opens gets the correct resource
short nsPluginFile::OpenPluginResource()
{
    FSSpec spec;
    OSErr err = toFSSpec(*this, spec);
    Boolean targetIsFolder, wasAliased;
    err = ::ResolveAliasFile(&spec, true, &targetIsFolder, &wasAliased);
    short refNum = ::FSpOpenResFile(&spec, fsRdPerm);
  
    if (refNum == -1) {
        CFBundleRef bundle = getPluginBundle(this->GetCString());
        if (bundle) {
            refNum = CFBundleOpenBundleResourceMap(bundle);
            CFRelease(bundle);
        }
    }
  
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
            info.fName = GetPluginString(126, 2);
      
            // 'STR#', 126, 1 => plugin description.
            info.fDescription = GetPluginString(126, 1);
      
            FSSpec spec;
            toFSSpec(*this, spec);
            info.fFileName = p2cstrdup(spec.name);
            info.fFullPath = PL_strdup(this->GetCString());
      
            CFBundleRef bundle = getPluginBundle(this->GetCString());
            if (bundle) {
                info.fBundle = PR_TRUE;
                CFRelease(bundle);
            } else
                info.fBundle = PR_FALSE;

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
          nsresult rv = ParsePluginMimeDescription(pfnGetMimeDesc(), info);
          if (NS_SUCCEEDED(rv)) {    // if we could parse the mime types from NP_GetMIMEDescription,
            ::CloseResFile(refNum);  // we've got what we need, close the resource, we're done
            return rv;
          }
        }

        // Next check for mime info from BP_GetSupportedMIMETypes
        BP_GETSUPPORTEDMIMETYPES pfnMime = 
          (BP_GETSUPPORTEDMIMETYPES)PR_FindSymbol(pLibrary, "BP_GetSupportedMIMETypes");
        if (pfnMime && noErr == pfnMime(&mi, 0) && mi.typeStrings) {        
          info.fVariantCount = (**(short**)mi.typeStrings) / 2;
          ::HLock(mi.typeStrings);
          if (mi.infoStrings)  // it's possible some plugins have infoStrings missing
            ::HLock(mi.infoStrings);
        }
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
      info.fMimeTypeArray      = new char*[variantCount];
      info.fExtensionArray     = new char*[variantCount];
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
    if (info.fPluginInfoSize <= sizeof(nsPluginInfo)) 
        {
            delete[] info.fName;
            delete[] info.fDescription;
            int variantCount = info.fVariantCount;
            for (int i = 0; i < variantCount; i++) 
                {
                    delete[] info.fMimeTypeArray[i];
                    delete[] info.fExtensionArray[i];
                    delete[] info.fMimeDescriptionArray[i];
                }
            delete[] info.fMimeTypeArray;
            delete[] info.fMimeDescriptionArray;
            delete[] info.fExtensionArray;
            delete[] info.fFileName;
            delete[] info.fFullPath;
        }
    return NS_OK;
}
