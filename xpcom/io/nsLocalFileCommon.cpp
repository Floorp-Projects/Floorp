/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLocalFile.h"  // includes platform-specific headers

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsPrintfCString.h"
#include "nsCRT.h"
#include "nsNativeCharsetUtils.h"
#include "nsUTF8Utils.h"
#include "nsArray.h"
#include "nsLocalFileCommon.h"

#ifdef XP_WIN
#  include <string.h>
#endif

// Extensions that should be considered 'executable', ie will not allow users
// to open immediately without first saving to disk, and potentially provoke
// other warnings. PLEASE read the longer comment in
// toolkit/components/reputationservice/ApplicationReputation.cpp
// before modifying this list!
// If you update this list, make sure to update the length of sExecutableExts
// in nsLocalFileCommmon.h.
/* static */
const char* const sExecutableExts[] = {
    // clang-format off
  ".accda",       // MS Access database
  ".accdb",       // MS Access database
  ".accde",       // MS Access database
  ".accdr",       // MS Access database
  ".ad",
  ".ade",         // access project extension
  ".adp",
  ".afploc",      // Apple Filing Protocol Location
  ".air",         // Adobe AIR installer
  ".app",         // executable application
  ".application", // from bug 348763
  ".appref-ms",   // ClickOnce link
  ".appx",
  ".appxbundle",
  ".asp",
  ".atloc",       // Appletalk Location
  ".bas",
  ".bat",
  ".cer",         // Signed certificate file
  ".chm",
  ".cmd",
  ".com",
  ".cpl",
  ".crt",
  ".der",
  ".diagcab",     // Windows archive
  ".exe",
  ".fileloc",     // Apple finder internet location data file
  ".ftploc",      // Apple FTP Location
  ".fxp",         // FoxPro compiled app
  ".hlp",
  ".hta",
  ".inetloc",     // Apple finder internet location data file
  ".inf",
  ".ins",
  ".isp",
  ".jar",         // java application bundle
#ifndef MOZ_ESR
  ".jnlp",
#endif
  ".js",
  ".jse",
  ".library-ms",  // Windows Library Files
  ".lnk",
  ".mad",         // Access Module Shortcut
  ".maf",         // Access
  ".mag",         // Access Diagram Shortcut
  ".mam",         // Access Macro Shortcut
  ".maq",         // Access Query Shortcut
  ".mar",         // Access Report Shortcut
  ".mas",         // Access Stored Procedure
  ".mat",         // Access Table Shortcut
  ".mau",         // Media Attachment Unit
  ".mav",         // Access View Shortcut
  ".maw",         // Access Data Access Page
  ".mda",         // Access Add-in, MDA Access 2 Workgroup
  ".mdb",
  ".mde",
  ".mdt",         // Access Add-in Data
  ".mdw",         // Access Workgroup Information
  ".mdz",         // Access Wizard Template
  ".msc",
  ".msh",         // Microsoft Shell
  ".msh1",        // Microsoft Shell
  ".msh1xml",     // Microsoft Shell
  ".msh2",        // Microsoft Shell
  ".msh2xml",     // Microsoft Shell
  ".mshxml",      // Microsoft Shell
  ".msi",
  ".msix",
  ".msixbundle",
  ".msp",
  ".mst",
  ".ops",         // Office Profile Settings
  ".pcd",
  ".pif",
  ".plg",         // Developer Studio Build Log
  ".prf",         // windows system file
  ".prg",
  ".pst",
  ".reg",
  ".scf",         // Windows explorer command
  ".scr",
  ".sct",
  ".settingcontent-ms",
  ".shb",
  ".shs",
  ".url",
  ".vb",
  ".vbe",
  ".vbs",
  ".vdx",
  ".vsd",
  ".vsdm",
  ".vsdx",
  ".vsmacros",    // Visual Studio .NET Binary-based Macro Project
  ".vss",
  ".vssm",
  ".vssx",
  ".vst",
  ".vstm",
  ".vstx",
  ".vsw",
  ".vsx",
  ".vtx",
  ".webloc",       // MacOS website location file
  ".ws",
  ".wsc",
  ".wsf",
  ".wsh",
  ".xll",         // MS Excel dynamic link library
  ".xrm-ms"
    // clang-format on
};

#if !defined(MOZ_WIDGET_COCOA) && !defined(XP_WIN)
NS_IMETHODIMP
nsLocalFile::InitWithFile(nsIFile* aFile) {
  if (NS_WARN_IF(!aFile)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString path;
  aFile->GetNativePath(path);
  if (path.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  return InitWithNativePath(path);
}
#endif

#define kMaxFilenameLength 255
#define kMaxExtensionLength 100
#define kMaxSequenceNumberLength 5  // "-9999"
// requirement: kMaxExtensionLength <
//                kMaxFilenameLength - kMaxSequenceNumberLength

NS_IMETHODIMP
nsLocalFile::CreateUnique(uint32_t aType, uint32_t aAttributes) {
  nsresult rv;
  bool longName;

#ifdef XP_WIN
  nsAutoString pathName, leafName, rootName, suffix;
  rv = GetPath(pathName);
#else
  nsAutoCString pathName, leafName, rootName, suffix;
  rv = GetNativePath(pathName);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }

  auto FailedBecauseExists = [&](nsresult aRv) {
    if (aRv == NS_ERROR_FILE_ACCESS_DENIED) {
      bool exists;
      return NS_SUCCEEDED(Exists(&exists)) && exists;
    }
    return aRv == NS_ERROR_FILE_ALREADY_EXISTS;
  };

  longName =
      (pathName.Length() + kMaxSequenceNumberLength > kMaxFilenameLength);
  if (!longName) {
    rv = Create(aType, aAttributes);
    if (!FailedBecauseExists(rv)) {
      return rv;
    }
  }

#ifdef XP_WIN
  rv = GetLeafName(leafName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  const int32_t lastDot = leafName.RFindChar(char16_t('.'));
#else
  rv = GetNativeLeafName(leafName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  const int32_t lastDot = leafName.RFindChar('.');
#endif

  if (lastDot == kNotFound) {
    rootName = leafName;
  } else {
    suffix = Substring(leafName, lastDot);       // include '.'
    rootName = Substring(leafName, 0, lastDot);  // strip suffix and dot
  }

  if (longName) {
    int32_t maxRootLength =
        (kMaxFilenameLength - (pathName.Length() - leafName.Length()) -
         suffix.Length() - kMaxSequenceNumberLength);

    // We cannot create an item inside a directory whose name is too long.
    // Also, ensure that at least one character remains after we truncate
    // the root name, as we don't want to end up with an empty leaf name.
    if (maxRootLength < 2) {
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }

#ifdef XP_WIN
    // ensure that we don't cut the name in mid-UTF16-character
    rootName.SetLength(NS_IS_LOW_SURROGATE(rootName[maxRootLength])
                           ? maxRootLength - 1
                           : maxRootLength);
    SetLeafName(rootName + suffix);
#else
    if (NS_IsNativeUTF8()) {
      // ensure that we don't cut the name in mid-UTF8-character
      // (assume the name is valid UTF8 to begin with)
      while (UTF8traits::isInSeq(rootName[maxRootLength])) {
        --maxRootLength;
      }

      // Another check to avoid ending up with an empty leaf name.
      if (maxRootLength == 0 && suffix.IsEmpty()) {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
      }
    }

    rootName.SetLength(maxRootLength);
    SetNativeLeafName(rootName + suffix);
#endif
    nsresult rvCreate = Create(aType, aAttributes);
    if (!FailedBecauseExists(rvCreate)) {
      return rvCreate;
    }
  }

  for (int indx = 1; indx < 10000; ++indx) {
    // start with "Picture-1.jpg" after "Picture.jpg" exists
#ifdef XP_WIN
    SetLeafName(rootName +
                NS_ConvertASCIItoUTF16(nsPrintfCString("-%d", indx)) + suffix);
#else
    SetNativeLeafName(rootName + nsPrintfCString("-%d", indx) + suffix);
#endif
    rv = Create(aType, aAttributes);
    if (NS_SUCCEEDED(rv) || !FailedBecauseExists(rv)) {
      return rv;
    }
  }

  // The disk is full, sort of
  return NS_ERROR_FILE_TOO_BIG;
}

#if defined(XP_WIN)
static const char16_t kPathSeparatorChar = '\\';
#elif defined(XP_UNIX)
static const char16_t kPathSeparatorChar = '/';
#else
#  error Need to define file path separator for your platform
#endif

static void SplitPath(char16_t* aPath, nsTArray<char16_t*>& aNodeArray) {
  if (*aPath == 0) {
    return;
  }

  if (*aPath == kPathSeparatorChar) {
    aPath++;
  }
  aNodeArray.AppendElement(aPath);

  for (char16_t* cp = aPath; *cp != 0; ++cp) {
    if (*cp == kPathSeparatorChar) {
      *cp++ = 0;
      if (*cp == 0) {
        break;
      }
      aNodeArray.AppendElement(cp);
    }
  }
}

NS_IMETHODIMP
nsLocalFile::GetRelativeDescriptor(nsIFile* aFromFile, nsACString& aResult) {
  if (NS_WARN_IF(!aFromFile)) {
    return NS_ERROR_INVALID_ARG;
  }

  //
  // aResult will be UTF-8 encoded
  //

  nsresult rv;
  aResult.Truncate(0);

  nsAutoString thisPath, fromPath;
  AutoTArray<char16_t*, 32> thisNodes;
  AutoTArray<char16_t*, 32> fromNodes;

  rv = GetPath(thisPath);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = aFromFile->GetPath(fromPath);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // get raw pointer to mutable string buffer
  char16_t* thisPathPtr = thisPath.BeginWriting();
  char16_t* fromPathPtr = fromPath.BeginWriting();

  SplitPath(thisPathPtr, thisNodes);
  SplitPath(fromPathPtr, fromNodes);

  size_t nodeIndex;
  for (nodeIndex = 0;
       nodeIndex < thisNodes.Length() && nodeIndex < fromNodes.Length();
       ++nodeIndex) {
#ifdef XP_WIN
    if (_wcsicmp(char16ptr_t(thisNodes[nodeIndex]),
                 char16ptr_t(fromNodes[nodeIndex]))) {
      break;
    }
#else
    if (nsCRT::strcmp(thisNodes[nodeIndex], fromNodes[nodeIndex])) {
      break;
    }
#endif
  }

  size_t branchIndex = nodeIndex;
  for (nodeIndex = branchIndex; nodeIndex < fromNodes.Length(); ++nodeIndex) {
    aResult.AppendLiteral("../");
  }
  StringJoinAppend(aResult, "/"_ns, mozilla::Span{thisNodes}.From(branchIndex),
                   [](nsACString& dest, const auto& thisNode) {
                     // XXX(Bug 1682869) We wouldn't need to reconstruct a
                     // nsDependentString here if SplitPath already returned
                     // nsDependentString. In fact, it seems SplitPath might be
                     // replaced by ParseString?
                     AppendUTF16toUTF8(nsDependentString{thisNode}, dest);
                   });

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetRelativeDescriptor(nsIFile* aFromFile,
                                   const nsACString& aRelativeDesc) {
  constexpr auto kParentDirStr = "../"_ns;

  nsCOMPtr<nsIFile> targetFile;
  nsresult rv = aFromFile->Clone(getter_AddRefs(targetFile));
  if (NS_FAILED(rv)) {
    return rv;
  }

  //
  // aRelativeDesc is UTF-8 encoded
  //

  nsCString::const_iterator strBegin, strEnd;
  aRelativeDesc.BeginReading(strBegin);
  aRelativeDesc.EndReading(strEnd);

  nsCString::const_iterator nodeBegin(strBegin), nodeEnd(strEnd);
  nsCString::const_iterator pos(strBegin);

  nsCOMPtr<nsIFile> parentDir;
  while (FindInReadable(kParentDirStr, nodeBegin, nodeEnd)) {
    rv = targetFile->GetParent(getter_AddRefs(parentDir));
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (!parentDir) {
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    targetFile = parentDir;

    nodeBegin = nodeEnd;
    pos = nodeEnd;
    nodeEnd = strEnd;
  }

  nodeBegin = nodeEnd = pos;
  while (nodeEnd != strEnd) {
    FindCharInReadable('/', nodeEnd, strEnd);
    targetFile->Append(NS_ConvertUTF8toUTF16(Substring(nodeBegin, nodeEnd)));
    if (nodeEnd != strEnd) {  // If there's more left in the string, inc over
                              // the '/' nodeEnd is on.
      ++nodeEnd;
    }
    nodeBegin = nodeEnd;
  }

  return InitWithFile(targetFile);
}

NS_IMETHODIMP
nsLocalFile::GetRelativePath(nsIFile* aFromFile, nsACString& aResult) {
  return GetRelativeDescriptor(aFromFile, aResult);
}

NS_IMETHODIMP
nsLocalFile::SetRelativePath(nsIFile* aFromFile,
                             const nsACString& aRelativePath) {
  return SetRelativeDescriptor(aFromFile, aRelativePath);
}
