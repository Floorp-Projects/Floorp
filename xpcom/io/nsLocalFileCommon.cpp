/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#include "nsIServiceManager.h"

#include "nsLocalFile.h" // includes platform-specific headers
#include "nsLocalFileUnicode.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsPrintfCString.h"


void NS_StartupLocalFile()
{
    nsLocalFile::GlobalInit();
}

void NS_ShutdownLocalFile()
{
    nsLocalFile::GlobalShutdown();
}

#if !defined(XP_MAC)
NS_IMETHODIMP
nsLocalFile::InitWithFile(nsILocalFile *aFile)
{
    NS_ENSURE_ARG(aFile);
    
    nsCAutoString path;
    aFile->GetPath(path);
    if (path.IsEmpty())
        return NS_ERROR_FAILURE;
    return InitWithPath(path); 
}
#endif

// should work on Macintosh, Unix, and Win32.
#define kMaxFilenameLength 31  

NS_IMETHODIMP
nsLocalFile::CreateUnique(PRUint32 type, PRUint32 attributes)
{
    nsresult rv = Create(type, attributes);
    
    if (NS_SUCCEEDED(rv)) return NS_OK;
    if (rv != NS_ERROR_FILE_ALREADY_EXISTS) return rv;

    nsCAutoString leafName; 
    rv = GetNativeLeafName(leafName);

    if (NS_FAILED(rv)) return rv;

    char* lastDot = strrchr(leafName.get(), '.');
    char suffix[kMaxFilenameLength + 1] = "";
    if (lastDot)
    {
        strncpy(suffix, lastDot, kMaxFilenameLength); // include '.'
        suffix[kMaxFilenameLength] = 0; // make sure it's null terminated
        leafName.SetLength(lastDot - leafName.get()); // strip suffix and dot.
    }

    // 27 should work on Macintosh, Unix, and Win32. 
    const int maxRootLength = 27 - strlen(suffix) - 1;

    if ((int)leafName.Length() > (int)maxRootLength)
        leafName.SetLength(maxRootLength);

    for (short indx = 1; indx < 10000; indx++)
    {
        // start with "Picture-1.jpg" after "Picture.jpg" exists
        SetNativeLeafName(leafName +
                          nsPrintfCString("-%d", indx) +
                          nsDependentCString(suffix));

        rv = Create(type, attributes);
    
        if (NS_SUCCEEDED(rv) || rv != NS_ERROR_FILE_ALREADY_EXISTS) 
        {
            return rv;
        }
    }
 
    // The disk is full, sort of
    return NS_ERROR_FILE_TOO_BIG;
}

#if defined(XP_MAC)
static const char kPathSeparatorChar       = ':';
#elif defined(XP_WIN) || defined(XP_OS2)
static const char kPathSeparatorChar       = '\\';
#elif defined(XP_UNIX) || defined(XP_BEOS)
static const char kPathSeparatorChar       = '/';
#else
#error Need to define file path separator for your platform
#endif

#if defined(XP_MAC)
static const char kSlashStr[] = "/";
static const char kESCSlashStr[] = "%2F";
#endif

static PRInt32 SplitPath(char *path, char **nodeArray, PRInt32 arrayLen)
{
    if (*path == 0)
      return 0;

    char **nodePtr = nodeArray;
    if (*path == kPathSeparatorChar)
      path++;    
    *nodePtr++ = path;
    
    for (char *cp = path; *cp != 0; cp++) {
      if (*cp == kPathSeparatorChar) {
        *cp++ = 0;
        if (*cp != 0) {
          if (nodePtr - nodeArray >= arrayLen)
            return -1;
          *nodePtr++ = cp;
        }
      }
    }
    return nodePtr - nodeArray;
}

 
NS_IMETHODIMP
nsLocalFile::GetRelativeDescriptor(nsILocalFile *fromFile, nsACString& _retval)
{
    const PRInt32 kMaxNodesInPath = 32;
        
    nsresult rv;
    _retval.Truncate(0);

    nsCAutoString thisPath, fromPath;
    char *thisNodes[kMaxNodesInPath], *fromNodes[kMaxNodesInPath];
    PRInt32  thisNodeCnt, fromNodeCnt, nodeIndex;
    
    rv = GetPath(thisPath);
    if (NS_FAILED(rv))
        return rv;
    rv = fromFile->GetPath(fromPath);
    if (NS_FAILED(rv))
        return rv;
    
    thisNodeCnt = SplitPath((char *)thisPath.get(), thisNodes, kMaxNodesInPath);
    fromNodeCnt = SplitPath((char *)fromPath.get(), fromNodes, kMaxNodesInPath);
    if (thisNodeCnt < 0 || fromNodeCnt < 0)
      return NS_ERROR_FAILURE;
    
    for (nodeIndex = 0; nodeIndex < thisNodeCnt && nodeIndex < fromNodeCnt; nodeIndex++) {
      if (!strcmp(thisNodes[nodeIndex], fromNodes[nodeIndex]))
        break;
    }
    
    PRInt32 branchIndex = nodeIndex;
    for (nodeIndex = branchIndex; nodeIndex < fromNodeCnt; nodeIndex++) 
      _retval.Append(NS_LITERAL_CSTRING("../"));
    for (nodeIndex = branchIndex; nodeIndex < thisNodeCnt; nodeIndex++) {
#ifdef XP_MAC
      nsCAutoString nodeStr(thisNodes[nodeIndex]);
      nodeStr.ReplaceSubstring(kSlashStr, kESCSlashStr);
#else
      nsDependentCString nodeStr(thisNodes[nodeIndex]);
#endif
      _retval.Append(nodeStr);
      if (nodeIndex + 1 < thisNodeCnt)
        _retval.Append('/');
    }
        
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetRelativeDescriptor(nsILocalFile *fromFile, const nsACString& relativeDesc)
{
    NS_NAMED_LITERAL_CSTRING(kParentDirStr, "../");
 
    nsCOMPtr<nsIFile> targetFile;
    nsresult rv = fromFile->Clone(getter_AddRefs(targetFile));
    if (NS_FAILED(rv))
        return rv;

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
        targetFile = parentDir;

        nodeBegin = nodeEnd;
        pos = nodeEnd;
        nodeEnd = strEnd;
    }

    nodeBegin = nodeEnd = pos;
    while (nodeEnd != strEnd) {
      FindCharInReadable('/', nodeEnd, strEnd);
#ifdef XP_MAC
      nsCAutoString nodeString(Substring(nodeBegin, nodeEnd));      
      nodeString.ReplaceSubstring(kESCSlashStr, kSlashStr);
      targetFile->Append(nodeString);
#else
      targetFile->Append(Substring(nodeBegin, nodeEnd));
#endif
      if (nodeEnd != strEnd) // If there's more left in the string, inc over the '/' nodeEnd is on.
        ++nodeEnd;
      nodeBegin = nodeEnd;
    }

    nsCOMPtr<nsILocalFile> targetLocalFile(do_QueryInterface(targetFile));
    return InitWithFile(targetLocalFile);
}
  
#define GET_UTF8(func, result)              \
    PR_BEGIN_MACRO                          \
        PRUnichar *buf = nsnull;            \
        nsresult rv = (func)(&buf);         \
        if (NS_FAILED(rv)) return rv;       \
        result = NS_ConvertUCS2toUTF8(buf); \
        nsMemory::Free(buf);                \
    PR_END_MACRO

NS_IMETHODIMP
nsLocalFile::Append(const nsACString &aNode)
{
    if (aNode.IsEmpty() || FSCharsetIsUTF8() || IsASCII(aNode))
        return AppendNative(aNode);

    return AppendUnicode(NS_ConvertUTF8toUCS2(aNode).get());
}

NS_IMETHODIMP
nsLocalFile::GetLeafName(nsACString &aLeafName)
{
    if (FSCharsetIsUTF8() || LeafIsASCII())
        return GetNativeLeafName(aLeafName);

    GET_UTF8(GetUnicodeLeafName, aLeafName);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetLeafName(const nsACString &aLeafName)
{
    if (aLeafName.IsEmpty() || FSCharsetIsUTF8() || IsASCII(aLeafName))
        return SetNativeLeafName(aLeafName);

    return SetUnicodeLeafName(NS_ConvertUTF8toUCS2(aLeafName).get());
}

NS_IMETHODIMP
nsLocalFile::CopyTo(nsIFile *aNewParentDir, const nsACString &aNewName)
{
    if (aNewName.IsEmpty() || FSCharsetIsUTF8() || IsASCII(aNewName))
        return CopyToNative(aNewParentDir, aNewName);

    return CopyToUnicode(aNewParentDir, NS_ConvertUTF8toUCS2(aNewName).get());
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinks(nsIFile *aNewParentDir, const nsACString &aNewName)
{
    if (aNewName.IsEmpty() || FSCharsetIsUTF8() || IsASCII(aNewName))
        return CopyToFollowingLinksNative(aNewParentDir, aNewName);

    return CopyToFollowingLinksUnicode(aNewParentDir, NS_ConvertUTF8toUCS2(aNewName).get());
}

NS_IMETHODIMP
nsLocalFile::MoveTo(nsIFile *aNewParentDir, const nsACString &aNewName)
{
    if (aNewName.IsEmpty() || FSCharsetIsUTF8() || IsASCII(aNewName))
        return MoveToNative(aNewParentDir, aNewName);

    return MoveToUnicode(aNewParentDir, NS_ConvertUTF8toUCS2(aNewName).get());
}

NS_IMETHODIMP
nsLocalFile::GetTarget(nsACString &aTarget)
{
    if (FSCharsetIsUTF8())
        return GetNativeTarget(aTarget);

    // XXX unfortunately, there is no way to know if the target will contain
    // non-ASCII characters until after we resolve it.
    GET_UTF8(GetUnicodeTarget, aTarget);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPath(nsACString &aPath)
{
    if (FSCharsetIsUTF8() || PathIsASCII())
        return GetNativePath(aPath);

    GET_UTF8(GetUnicodePath, aPath);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::InitWithPath(const nsACString &aPath)
{
    if (aPath.IsEmpty() || FSCharsetIsUTF8() || IsASCII(aPath))
        return InitWithNativePath(aPath);

    return InitWithUnicodePath(NS_ConvertUTF8toUCS2(aPath).get());
}

NS_IMETHODIMP
nsLocalFile::AppendRelativePath(const nsACString &aRelativePath)
{
    if (aRelativePath.IsEmpty() || FSCharsetIsUTF8() || IsASCII(aRelativePath))
        return AppendRelativeNativePath(aRelativePath);

    return AppendRelativeUnicodePath(NS_ConvertUTF8toUCS2(aRelativePath).get());
}

nsresult
NS_NewLocalFile(const nsACString &aPath, PRBool aFollowLinks, nsILocalFile **aResult)
{
    if (aPath.IsEmpty() || nsLocalFile::FSCharsetIsUTF8() || IsASCII(aPath))
        return NS_NewNativeLocalFile(aPath, aFollowLinks, aResult);

    return NS_NewUnicodeLocalFile(NS_ConvertUTF8toUCS2(aPath).get(), aFollowLinks, aResult);
}
