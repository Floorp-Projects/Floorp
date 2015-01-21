/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Mac OS X-specific local file uri parsing */
#include "nsURLHelper.h"
#include "nsEscape.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include <Carbon/Carbon.h>

static nsTArray<nsCString> *gVolumeList = nullptr;

static bool pathBeginsWithVolName(const nsACString& path, nsACString& firstPathComponent)
{
  // Return whether the 1st path component in path (escaped) is equal to the name
  // of a mounted volume. Return the 1st path component (unescaped) in any case.
  // This needs to be done as quickly as possible, so we cache a list of volume names.
  // XXX Register an event handler to detect drives being mounted/unmounted?
  
  if (!gVolumeList) {
    gVolumeList = new nsTArray<nsCString>;
    if (!gVolumeList) {
      return false; // out of memory
    }
  }

  // Cache a list of volume names
  if (!gVolumeList->Length()) {
    OSErr err;
    ItemCount volumeIndex = 1;
    
    do {
      HFSUniStr255 volName;
      FSRef rootDirectory;
      err = ::FSGetVolumeInfo(0, volumeIndex, nullptr, kFSVolInfoNone, nullptr,
                              &volName, &rootDirectory);
      if (err == noErr) {
        NS_ConvertUTF16toUTF8 volNameStr(Substring((char16_t *)volName.unicode,
                                                   (char16_t *)volName.unicode + volName.length));
        gVolumeList->AppendElement(volNameStr);
        volumeIndex++;
      }
    } while (err == noErr);
  }
  
  // Extract the first component of the path
  nsACString::const_iterator start;
  path.BeginReading(start);
  start.advance(1); // path begins with '/'
  nsACString::const_iterator directory_end;
  path.EndReading(directory_end);
  nsACString::const_iterator component_end(start);
  FindCharInReadable('/', component_end, directory_end);
  
  nsAutoCString flatComponent((Substring(start, component_end)));
  NS_UnescapeURL(flatComponent);
  int32_t foundIndex = gVolumeList->IndexOf(flatComponent);
  firstPathComponent = flatComponent;
  return (foundIndex != -1);
}

void
net_ShutdownURLHelperOSX()
{
  delete gVolumeList;
  gVolumeList = nullptr;
}

static nsresult convertHFSPathtoPOSIX(const nsACString& hfsPath, nsACString& posixPath)
{
  // Use CFURL to do the conversion. We don't want to do this by simply
  // using SwapSlashColon - we need the charset mapped from MacRoman
  // to UTF-8, and we need "/Volumes" (or whatever - Apple says this is subject to change)
  // prepended if the path is not on the boot drive.

  CFStringRef pathStrRef = CFStringCreateWithCString(nullptr,
                              PromiseFlatCString(hfsPath).get(),
                              kCFStringEncodingMacRoman);
  if (!pathStrRef)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_ERROR_FAILURE;
  CFURLRef urlRef = CFURLCreateWithFileSystemPath(nullptr,
                              pathStrRef, kCFURLHFSPathStyle, true);
  if (urlRef) {
    UInt8 pathBuf[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(urlRef, true, pathBuf, sizeof(pathBuf))) {
      posixPath = (char *)pathBuf;
      rv = NS_OK;
    }
  }
  CFRelease(pathStrRef);
  if (urlRef)
    CFRelease(urlRef);
  return rv;
}

static void SwapSlashColon(char *s)
{
  while (*s) {
    if (*s == '/')
      *s = ':';
    else if (*s == ':')
      *s = '/';
    s++;
  }
} 

nsresult
net_GetURLSpecFromActualFile(nsIFile *aFile, nsACString &result)
{
  // NOTE: This is identical to the implementation in nsURLHelperUnix.cpp
  
  nsresult rv;
  nsAutoCString ePath;

  // construct URL spec from native file path
  rv = aFile->GetNativePath(ePath);
  if (NS_FAILED(rv))
    return rv;

  nsAutoCString escPath;
  NS_NAMED_LITERAL_CSTRING(prefix, "file://");
      
  // Escape the path with the directory mask
  if (NS_EscapeURL(ePath.get(), ePath.Length(), esc_Directory+esc_Forced, escPath))
    escPath.Insert(prefix, 0);
  else
    escPath.Assign(prefix + ePath);

  // esc_Directory does not escape the semicolons, so if a filename 
  // contains semicolons we need to manually escape them.
  // This replacement should be removed in bug #473280
  escPath.ReplaceSubstring(";", "%3b");

  result = escPath;
  return NS_OK;
}

nsresult
net_GetFileFromURLSpec(const nsACString &aURL, nsIFile **result)
{
  // NOTE: See also the implementation in nsURLHelperUnix.cpp
  // This matches it except for the HFS path handling.

  nsresult rv;

  nsCOMPtr<nsIFile> localFile;
  rv = NS_NewNativeLocalFile(EmptyCString(), true, getter_AddRefs(localFile));
  if (NS_FAILED(rv))
    return rv;
  
  nsAutoCString directory, fileBaseName, fileExtension, path;
  bool bHFSPath = false;

  rv = net_ParseFileURL(aURL, directory, fileBaseName, fileExtension);
  if (NS_FAILED(rv))
    return rv;

  if (!directory.IsEmpty()) {
    NS_EscapeURL(directory, esc_Directory|esc_AlwaysCopy, path);

    // The canonical form of file URLs on OSX use POSIX paths:
    //   file:///path-name.
    // But, we still encounter file URLs that use HFS paths:
    //   file:///volume-name/path-name
    // Determine that here and normalize HFS paths to POSIX.
    nsAutoCString possibleVolName;
    if (pathBeginsWithVolName(directory, possibleVolName)) {        
      // Though we know it begins with a volume name, it could still
      // be a valid POSIX path if the boot drive is named "Mac HD"
      // and there is a directory "Mac HD" at its root. If such a
      // directory doesn't exist, we'll assume this is an HFS path.
      FSRef testRef;
      possibleVolName.Insert("/", 0);
      if (::FSPathMakeRef((UInt8*)possibleVolName.get(), &testRef, nullptr) != noErr)
        bHFSPath = true;
    }

    if (bHFSPath) {
      // "%2F"s need to become slashes, while all other slashes need to
      // become colons. If we start out by changing "%2F"s to colons, we
      // can reply on SwapSlashColon() to do what we need
      path.ReplaceSubstring("%2F", ":");
      path.Cut(0, 1); // directory begins with '/'
      SwapSlashColon((char *)path.get());
      // At this point, path is an HFS path made using the same
      // algorithm as nsURLHelperMac. We'll convert to POSIX below.
    }
  }
  if (!fileBaseName.IsEmpty())
    NS_EscapeURL(fileBaseName, esc_FileBaseName|esc_AlwaysCopy, path);
  if (!fileExtension.IsEmpty()) {
    path += '.';
    NS_EscapeURL(fileExtension, esc_FileExtension|esc_AlwaysCopy, path);
  }

  NS_UnescapeURL(path);
  if (path.Length() != strlen(path.get()))
    return NS_ERROR_FILE_INVALID_PATH;

  if (bHFSPath)
    convertHFSPathtoPOSIX(path, path);

  // assuming path is encoded in the native charset
  rv = localFile->InitWithNativePath(path);
  if (NS_FAILED(rv))
    return rv;

  NS_ADDREF(*result = localFile);
  return NS_OK;
}
