/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsIServiceManager.h"

#include "nsLocalFile.h" // includes platform-specific headers

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsPrintfCString.h"
#include "nsCRT.h"
#include "nsNativeCharsetUtils.h"
#include "nsUTF8Utils.h"

#ifdef XP_WIN
#include <string.h>
#endif


void NS_StartupLocalFile()
{
    nsLocalFile::GlobalInit();
}

void NS_ShutdownLocalFile()
{
    nsLocalFile::GlobalShutdown();
}

#if !defined(MOZ_WIDGET_COCOA) && !defined(XP_WIN)
NS_IMETHODIMP
nsLocalFile::InitWithFile(nsIFile *aFile)
{
    if (NS_WARN_IF(!aFile))
        return NS_ERROR_INVALID_ARG;
    
    nsAutoCString path;
    aFile->GetNativePath(path);
    if (path.IsEmpty())
        return NS_ERROR_INVALID_ARG;
    return InitWithNativePath(path); 
}
#endif

#define kMaxFilenameLength 255
#define kMaxExtensionLength 100
#define kMaxSequenceNumberLength 5 // "-9999"
// requirement: kMaxExtensionLength < kMaxFilenameLength - kMaxSequenceNumberLength

NS_IMETHODIMP
nsLocalFile::CreateUnique(uint32_t type, uint32_t attributes)
{
    nsresult rv;
    bool longName;

#ifdef XP_WIN
    nsAutoString pathName, leafName, rootName, suffix;
    rv = GetPath(pathName);
#else
    nsAutoCString pathName, leafName, rootName, suffix; 
    rv = GetNativePath(pathName);
#endif
    if (NS_FAILED(rv))
        return rv;

    longName = (pathName.Length() + kMaxSequenceNumberLength >
                kMaxFilenameLength);
    if (!longName)
    {
        rv = Create(type, attributes);
        if (rv != NS_ERROR_FILE_ALREADY_EXISTS)
            return rv;
    }

#ifdef XP_WIN
    rv = GetLeafName(leafName);
    if (NS_FAILED(rv))
        return rv;

    const int32_t lastDot = leafName.RFindChar(PRUnichar('.'));
#else
    rv = GetNativeLeafName(leafName);
    if (NS_FAILED(rv))
        return rv;

    const int32_t lastDot = leafName.RFindChar('.');
#endif

    if (lastDot == kNotFound)
    {
        rootName = leafName;
    } 
    else
    {
        suffix = Substring(leafName, lastDot);      // include '.'
        rootName = Substring(leafName, 0, lastDot); // strip suffix and dot
    }

    if (longName)
    {
        int32_t maxRootLength = (kMaxFilenameLength -
                                 (pathName.Length() - leafName.Length()) -
                                 suffix.Length() - kMaxSequenceNumberLength);

        // We cannot create an item inside a directory whose name is too long.
        // Also, ensure that at least one character remains after we truncate
        // the root name, as we don't want to end up with an empty leaf name.
        if (maxRootLength < 2)
            return NS_ERROR_FILE_UNRECOGNIZED_PATH;

#ifdef XP_WIN
        // ensure that we don't cut the name in mid-UTF16-character
        rootName.SetLength(NS_IS_LOW_SURROGATE(rootName[maxRootLength]) ?
                           maxRootLength - 1 : maxRootLength);
        SetLeafName(rootName + suffix);
#else
        if (NS_IsNativeUTF8())
        {
            // ensure that we don't cut the name in mid-UTF8-character
            // (assume the name is valid UTF8 to begin with)
            while (UTF8traits::isInSeq(rootName[maxRootLength]))
                --maxRootLength;

            // Another check to avoid ending up with an empty leaf name.
            if (maxRootLength == 0 && suffix.IsEmpty())
                return NS_ERROR_FILE_UNRECOGNIZED_PATH;
        }

        rootName.SetLength(maxRootLength);
        SetNativeLeafName(rootName + suffix);
#endif
        nsresult rv = Create(type, attributes);
        if (rv != NS_ERROR_FILE_ALREADY_EXISTS)
            return rv;
    }

    for (int indx = 1; indx < 10000; indx++)
    {
        // start with "Picture-1.jpg" after "Picture.jpg" exists
#ifdef XP_WIN
        SetLeafName(rootName +
                    NS_ConvertASCIItoUTF16(nsPrintfCString("-%d", indx)) +
                    suffix);
#else
        SetNativeLeafName(rootName + nsPrintfCString("-%d", indx) + suffix);
#endif
        rv = Create(type, attributes);
        if (NS_SUCCEEDED(rv) || rv != NS_ERROR_FILE_ALREADY_EXISTS) 
            return rv;
    }
 
    // The disk is full, sort of
    return NS_ERROR_FILE_TOO_BIG;
}

#if defined(XP_WIN) || defined(XP_OS2)
static const PRUnichar kPathSeparatorChar       = '\\';
#elif defined(XP_UNIX)
static const PRUnichar kPathSeparatorChar       = '/';
#else
#error Need to define file path separator for your platform
#endif

static int32_t SplitPath(PRUnichar *path, PRUnichar **nodeArray, int32_t arrayLen)
{
    if (*path == 0)
      return 0;

    PRUnichar **nodePtr = nodeArray;
    if (*path == kPathSeparatorChar)
      path++;    
    *nodePtr++ = path;
    
    for (PRUnichar *cp = path; *cp != 0; cp++) {
      if (*cp == kPathSeparatorChar) {
        *cp++ = 0;
        if (*cp == 0)
          break;
        if (nodePtr - nodeArray >= arrayLen)
          return -1;
        *nodePtr++ = cp;
      }
    }
    return nodePtr - nodeArray;
}

 
NS_IMETHODIMP
nsLocalFile::GetRelativeDescriptor(nsIFile *fromFile, nsACString& _retval)
{
    if (NS_WARN_IF(!fromFile))
        return NS_ERROR_INVALID_ARG;
    const int32_t kMaxNodesInPath = 32;

    //
    // _retval will be UTF-8 encoded
    // 
        
    nsresult rv;
    _retval.Truncate(0);

    nsAutoString thisPath, fromPath;
    PRUnichar *thisNodes[kMaxNodesInPath], *fromNodes[kMaxNodesInPath];
    int32_t  thisNodeCnt, fromNodeCnt, nodeIndex;
    
    rv = GetPath(thisPath);
    if (NS_FAILED(rv))
        return rv;
    rv = fromFile->GetPath(fromPath);
    if (NS_FAILED(rv))
        return rv;

    // get raw pointer to mutable string buffer
    PRUnichar *thisPathPtr; thisPath.BeginWriting(thisPathPtr);
    PRUnichar *fromPathPtr; fromPath.BeginWriting(fromPathPtr);
    
    thisNodeCnt = SplitPath(thisPathPtr, thisNodes, kMaxNodesInPath);
    fromNodeCnt = SplitPath(fromPathPtr, fromNodes, kMaxNodesInPath);
    if (thisNodeCnt < 0 || fromNodeCnt < 0)
      return NS_ERROR_FAILURE;
    
    for (nodeIndex = 0; nodeIndex < thisNodeCnt && nodeIndex < fromNodeCnt; ++nodeIndex) {
#ifdef XP_WIN
      if (_wcsicmp(char16ptr_t(thisNodes[nodeIndex]), char16ptr_t(fromNodes[nodeIndex])))
        break;
#else
      if (nsCRT::strcmp(thisNodes[nodeIndex], fromNodes[nodeIndex]))
        break;
#endif
    }
    
    int32_t branchIndex = nodeIndex;
    for (nodeIndex = branchIndex; nodeIndex < fromNodeCnt; nodeIndex++) 
      _retval.AppendLiteral("../");
    for (nodeIndex = branchIndex; nodeIndex < thisNodeCnt; nodeIndex++) {
      NS_ConvertUTF16toUTF8 nodeStr(thisNodes[nodeIndex]);
      _retval.Append(nodeStr);
      if (nodeIndex + 1 < thisNodeCnt)
        _retval.Append('/');
    }
        
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetRelativeDescriptor(nsIFile *fromFile, const nsACString& relativeDesc)
{
    NS_NAMED_LITERAL_CSTRING(kParentDirStr, "../");
 
    nsCOMPtr<nsIFile> targetFile;
    nsresult rv = fromFile->Clone(getter_AddRefs(targetFile));
    if (NS_FAILED(rv))
        return rv;

    //
    // relativeDesc is UTF-8 encoded
    // 

    nsCString::const_iterator strBegin, strEnd;
    relativeDesc.BeginReading(strBegin);
    relativeDesc.EndReading(strEnd);
    
    nsCString::const_iterator nodeBegin(strBegin), nodeEnd(strEnd);
    nsCString::const_iterator pos(strBegin);
    
    nsCOMPtr<nsIFile> parentDir;
    while (FindInReadable(kParentDirStr, nodeBegin, nodeEnd)) {
        rv = targetFile->GetParent(getter_AddRefs(parentDir));
        if (NS_FAILED(rv))
            return rv;
        if (!parentDir)
            return NS_ERROR_FILE_UNRECOGNIZED_PATH;
        targetFile = parentDir;

        nodeBegin = nodeEnd;
        pos = nodeEnd;
        nodeEnd = strEnd;
    }

    nodeBegin = nodeEnd = pos;
    while (nodeEnd != strEnd) {
      FindCharInReadable('/', nodeEnd, strEnd);
      targetFile->Append(NS_ConvertUTF8toUTF16(Substring(nodeBegin, nodeEnd)));
      if (nodeEnd != strEnd) // If there's more left in the string, inc over the '/' nodeEnd is on.
        ++nodeEnd;
      nodeBegin = nodeEnd;
    }

    return InitWithFile(targetFile);
}
