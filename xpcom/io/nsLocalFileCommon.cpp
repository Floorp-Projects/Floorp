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
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"


void NS_StartupLocalFile()
{
#ifdef XP_WIN
  nsresult rv = NS_CreateShortcutResolver();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Shortcut resolver could not be created");
#endif
#ifdef XP_OS2
  nsresult rv = NS_CreateUnicodeConverters();
#endif
}

void NS_ShutdownLocalFile()
{
#ifndef XP_OS2
  NS_ShutdownLocalFileUnicode();
#endif
  
#ifdef XP_WIN
  NS_DestroyShortcutResolver();
#endif
#ifdef XP_OS2
  NS_DestroyUnicodeConverters();
#endif
}

#if !defined(XP_MAC)
NS_IMETHODIMP
nsLocalFile::InitWithFile(nsILocalFile *aFile)
{
    NS_ENSURE_ARG(aFile);
    
    nsXPIDLCString path;
    aFile->GetPath(getter_Copies(path));
    if (!path)
        return NS_ERROR_FAILURE;
    return InitWithPath(path.get()); 
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

    char* leafName; 
    rv = GetLeafName(&leafName);

    if (NS_FAILED(rv)) return rv;

    char* lastDot = strrchr(leafName, '.');
    char suffix[kMaxFilenameLength + 1] = "";
    if (lastDot)
    {
        strncpy(suffix, lastDot, kMaxFilenameLength); // include '.'
        suffix[kMaxFilenameLength] = 0; // make sure it's null terminated
        *lastDot = '\0'; // strip suffix and dot.
    }

    // 27 should work on Macintosh, Unix, and Win32. 
    const int maxRootLength = 27 - strlen(suffix) - 1;

    if ((int)strlen(leafName) > (int)maxRootLength)
        leafName[maxRootLength] = '\0';

    for (short indx = 1; indx < 10000; indx++)
    {
        // start with "Picture-1.jpg" after "Picture.jpg" exists
        char newName[kMaxFilenameLength + 1];
        sprintf(newName, "%s-%d%s", leafName, indx, suffix);
        SetLeafName(newName);

        rv = Create(type, attributes);
    
        if (NS_SUCCEEDED(rv) || rv != NS_ERROR_FILE_ALREADY_EXISTS) 
        {
            nsMemory::Free(leafName);
            return rv;
        }
    }
 
    nsMemory::Free(leafName);
    // The disk is full, sort of
    return NS_ERROR_FILE_TOO_BIG;
}

#if defined(XP_MAC)
const PRUnichar kPathSeparatorChar       = ':';
#elif defined(XP_WIN) || defined(XP_OS2)
const PRUnichar kPathSeparatorChar       = '\\';
#elif defined(XP_UNIX) || defined(XP_BEOS)
const PRUnichar kPathSeparatorChar       = '/';
#else
#error Need to define file path separator for your platform
#endif

#if defined(XP_MAC)
const char* kSlashStr = "/";
const char* kESCSlashStr = "%2F";
#endif

static PRInt32 SplitPath(PRUnichar *path, PRUnichar **nodeArray, PRInt32 arrayLen)
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

    PRUnichar *thisPath = nsnull, *fromPath = nsnull;
    PRUnichar *thisNodes[kMaxNodesInPath], *fromNodes[kMaxNodesInPath];
    PRInt32  thisNodeCnt, fromNodeCnt, nodeIndex;
    
    rv = GetUnicodePath(&thisPath);
    if (NS_FAILED(rv))
        return rv;
    rv = fromFile->GetUnicodePath(&fromPath);
    if (NS_FAILED(rv)) {
        nsMemory::Free(thisPath);
        return rv;
    }
    
    thisNodeCnt = SplitPath(thisPath, thisNodes, kMaxNodesInPath);
    fromNodeCnt = SplitPath(fromPath, fromNodes, kMaxNodesInPath);
    if (thisNodeCnt < 0 || fromNodeCnt < 0) {
      nsMemory::Free(thisPath);
      nsMemory::Free(fromPath);
      return NS_ERROR_FAILURE;
    }      
    
    for (nodeIndex = 0; nodeIndex < thisNodeCnt && nodeIndex < fromNodeCnt; nodeIndex++) {
      if (!(nsDependentString(thisNodes[nodeIndex])).Equals(nsDependentString(fromNodes[nodeIndex])))
        break;
    }
    
    PRInt32 branchIndex = nodeIndex;
    for (nodeIndex = branchIndex; nodeIndex < fromNodeCnt; nodeIndex++) 
      _retval.Append("../");
    for (nodeIndex = branchIndex; nodeIndex < thisNodeCnt; nodeIndex++) {
      NS_ConvertUCS2toUTF8 nodeStr(thisNodes[nodeIndex]);
#ifdef XP_MAC
      nodeStr.ReplaceSubstring(kSlashStr, kESCSlashStr);
#endif
      _retval.Append(nodeStr.get());
      if (nodeIndex + 1 < thisNodeCnt)
        _retval.Append('/');
    }
        
    nsMemory::Free(thisPath);
    nsMemory::Free(fromPath);
        
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
      nsCAutoString nodeString(Substring(nodeBegin, nodeEnd));      
#ifdef XP_MAC
      nodeString.ReplaceSubstring(kESCSlashStr, kSlashStr);
#endif
      targetFile->AppendUnicode((NS_ConvertUTF8toUCS2(nodeString)).get());
      if (nodeEnd != strEnd) // If there's more left in the string, inc over the '/' nodeEnd is on.
        ++nodeEnd;
      nodeBegin = nodeEnd;
    }

    nsCOMPtr<nsILocalFile> targetLocalFile(do_QueryInterface(targetFile));
    return InitWithFile(targetLocalFile);
}
