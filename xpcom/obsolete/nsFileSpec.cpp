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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
#include "nsFileSpec.h"

#include "nsDebug.h"
#include "nsEscape.h"
#include "nsMemory.h"

#include "prtypes.h"
#include "plstr.h"
#include "plbase64.h"
#include "prmem.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsILocalFile.h"

#include <string.h>
#include <stdio.h>

#if defined(XP_WIN)
#include <mbstring.h>
#endif

#ifdef XP_OS2
extern unsigned char* _mbsrchr( const unsigned char*, int);
#endif

// return pointer to last instance of the given separator
static inline char *GetLastSeparator(const char *str, char sep)
{
#if defined(XP_WIN) || defined(XP_OS2)
    return (char*) _mbsrchr((const unsigned char *) str, sep);
#else
    return (char*) strrchr(str, sep);
#endif
}

#if defined(XP_MACOSX)
#include <sys/stat.h>
#endif

#if defined(XP_MAC) || defined(XP_MACOSX)
#include <Aliases.h>
#include <TextUtils.h>
#endif

//========================================================================================
//            class nsSimpleCharString
//========================================================================================

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString()
//----------------------------------------------------------------------------------------
:   mData(nsnull)
{
    
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString(const char* inString)
//----------------------------------------------------------------------------------------
:   mData(nsnull)
{
    if (inString)
        CopyFrom(inString, strlen(inString));
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString(const nsString& inString)
//----------------------------------------------------------------------------------------
:   mData(nsnull)
{
    *this = inString;
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString(const nsSimpleCharString& inOther)
//----------------------------------------------------------------------------------------
{
    mData = inOther.mData;
    AddRefData();
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString(const char* inData, PRUint32 inLength)
//----------------------------------------------------------------------------------------
:   mData(nsnull)
{
    CopyFrom(inData, inLength);
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::~nsSimpleCharString()
//----------------------------------------------------------------------------------------
{
    ReleaseData();
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
void nsSimpleCharString::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    if (inString)
        CopyFrom(inString, strlen(inString));
    else
        SetToEmpty();
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::operator = (const nsString& inString)
//----------------------------------------------------------------------------------------
{
    PRUint32 len = inString.Length();
    ReallocData(len);
    if (!mData)
        return;
    inString.ToCString(mData->mString, len + 1);  
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::operator = (const nsSimpleCharString& inOther)
//----------------------------------------------------------------------------------------
{
    if (mData == inOther.mData)
        return;
    ReleaseData();
    mData = inOther.mData;
    AddRefData();
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::operator += (const char* inOther)
//----------------------------------------------------------------------------------------
{
    if (!inOther)
        return;
    int newLength = Length() + strlen(inOther);
    ReallocData(newLength);
    strcat(mData->mString, inOther);
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
nsSimpleCharString nsSimpleCharString::operator + (const char* inOther) const
//----------------------------------------------------------------------------------------
{
    nsSimpleCharString result(*this);
    result += inOther;
    return result;
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::Catenate(const char* inString1, const char* inString2)
//----------------------------------------------------------------------------------------
{
    if (!inString2)
    {
        *this += inString1;
        return;
    }
    int newLength = Length() + strlen(inString1) + strlen(inString2);
    ReallocData(newLength);
    strcat(mData->mString, inString1);
    strcat(mData->mString, inString2);
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::CopyFrom(const char* inData, PRUint32 inLength)
//----------------------------------------------------------------------------------------
{
    if (!inData)
        return;
    ReallocData(inLength);
    if (!mData)
        return;
    if (inLength != 0) {
        memcpy(mData->mString, inData, inLength);
    }
    mData->mString[inLength] = '\0';
} // nsSimpleCharString::CopyFrom

//----------------------------------------------------------------------------------------
void nsSimpleCharString::SetToEmpty()
//----------------------------------------------------------------------------------------
{
    ReleaseData();
} // nsSimpleCharString::SetToEmpty

//----------------------------------------------------------------------------------------
void nsSimpleCharString::Unescape()
//----------------------------------------------------------------------------------------
{
    if (!mData)
        return;
    ReallocData(mData->mLength);
    if (!mData)
        return;
    nsUnescape(mData->mString);
    mData->mLength = strlen(mData->mString);       
} // nsSimpleCharString::Unescape


//----------------------------------------------------------------------------------------
void nsSimpleCharString::AddRefData()
//----------------------------------------------------------------------------------------
{
    if (mData)
        ++mData->mRefCount;
} // nsSimpleCharString::AddRefData

//----------------------------------------------------------------------------------------
void nsSimpleCharString::ReleaseData()
//----------------------------------------------------------------------------------------
{
    if (!mData)
        return;
    NS_ASSERTION(mData->mRefCount > 0, "String deleted too many times!");
    if (--mData->mRefCount == 0)
        PR_Free(mData);
    mData = nsnull;
} // nsSimpleCharString::ReleaseData

//----------------------------------------------------------------------------------------
inline PRUint32 CalculateAllocLength(PRUint32 logicalLength)
// Round up to the next multiple of 256.
//----------------------------------------------------------------------------------------
{
    return ((1 + (logicalLength >> 8)) << 8);
}

//----------------------------------------------------------------------------------------
void nsSimpleCharString::ReallocData(PRUint32 inLength)
// Reallocate mData to a new length.  Since this presumably precedes a change to the string,
// we want to detach ourselves if the data is shared by another string, even if the length
// requested would not otherwise require a reallocation.
//----------------------------------------------------------------------------------------
{
    PRUint32 newAllocLength = CalculateAllocLength(inLength);
    PRUint32 oldAllocLength = CalculateAllocLength(Length());
    if (mData)
    {
        NS_ASSERTION(mData->mRefCount > 0, "String deleted too many times!");
        if (mData->mRefCount == 1)
        {
            // We are the sole owner, so just change its length, if necessary.
            if (newAllocLength > oldAllocLength)
                mData = (Data*)PR_Realloc(mData, newAllocLength + sizeof(Data));
            mData->mLength = inLength;
            mData->mString[inLength] = '\0'; // we may be truncating
            return;
        }
    }
    PRUint32 copyLength = Length();
    if (inLength < copyLength)
        copyLength = inLength;
    Data* newData = (Data*)PR_Malloc(newAllocLength + sizeof(Data));
    // If data was already allocated when we get to here, then we are cloning the data
    // from a shared pointer.
    if (mData)
    {
        memcpy(newData, mData, sizeof(Data) + copyLength);
        mData->mRefCount--; // Say goodbye
    }
    else
        newData->mString[0] = '\0';

    mData = newData;
    mData->mRefCount = 1;
    mData->mLength = inLength;
} // nsSimpleCharString::ReleaseData


//========================================================================================
NS_NAMESPACE nsFileSpecHelpers
//========================================================================================
{
    enum
    {    kMaxFilenameLength = 31                // should work on Macintosh, Unix, and Win32.
    ,    kMaxAltDigitLength    = 5
    ,    kMaxCoreLeafNameLength    = (kMaxFilenameLength - (kMaxAltDigitLength + 1))
    };
#if !defined(XP_MAC)
    NS_NAMESPACE_PROTOTYPE void Canonify(nsSimpleCharString& ioPath, PRBool inMakeDirs);
    NS_NAMESPACE_PROTOTYPE void MakeAllDirectories(const char* inPath, int mode);
#endif
#if defined(XP_WIN) || defined(XP_OS2)
    NS_NAMESPACE_PROTOTYPE void NativeToUnix(nsSimpleCharString& ioPath);
    NS_NAMESPACE_PROTOTYPE void UnixToNative(nsSimpleCharString& ioPath);
#endif
} NS_NAMESPACE_END

//----------------------------------------------------------------------------------------
nsresult ns_file_convert_result(PRInt32 nativeErr)
//----------------------------------------------------------------------------------------
{
    return nativeErr ?
        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,((nativeErr)&0xFFFF))
        : NS_OK;
}

//----------------------------------------------------------------------------------------
void nsSimpleCharString::LeafReplace(char inSeparator, const char* inLeafName)
//----------------------------------------------------------------------------------------
{
    // Find the existing leaf name
    if (IsEmpty())
        return;
    if (!inLeafName)
    {
        SetToEmpty();
        return;
    }
    char* chars = mData->mString;
    char* lastSeparator = GetLastSeparator(chars, inSeparator);
    int oldLength = Length();
    PRBool trailingSeparator = (lastSeparator + 1 == chars + oldLength);
    if (trailingSeparator)
    {
        char savedCh = *lastSeparator;
        char *savedLastSeparator = lastSeparator;
        *lastSeparator = '\0';
        lastSeparator = GetLastSeparator(chars, inSeparator);
        *savedLastSeparator = savedCh;
    }
    if (lastSeparator)
        lastSeparator++; // point at the trailing string
    else
        lastSeparator = chars; // the full monty

    PRUint32 savedLastSeparatorOffset = (lastSeparator - chars);
    int newLength =
        (lastSeparator - chars) + strlen(inLeafName) + (trailingSeparator != 0);
    ReallocData(newLength);

    chars = mData->mString; // it might have moved.
    chars[savedLastSeparatorOffset] = '\0'; // strip the current leaf name

    strcat(chars, inLeafName);
    if (trailingSeparator)
    {
        // If the original ended in a slash, then the new one should, too.
        char sepStr[2] = "/";
        *sepStr = inSeparator;
        strcat(chars, sepStr);
    }
} // nsSimpleCharString::LeafReplace

//----------------------------------------------------------------------------------------
char* nsSimpleCharString::GetLeaf(char inSeparator) const
// Returns a pointer to an allocated string representing the leaf.
//----------------------------------------------------------------------------------------
{
    if (IsEmpty())
        return nsnull;

    char* chars = mData->mString;
    const char* lastSeparator = GetLastSeparator(chars, inSeparator);
    // If there was no separator, then return a copy of our path.
    if (!lastSeparator)
        return nsCRT::strdup(*this);

    // So there's at least one separator.  What's just after it?
    // If the separator was not the last character, return the trailing string.
    const char* leafPointer = lastSeparator + 1;
    if (*leafPointer)
        return nsCRT::strdup(leafPointer);

    // So now, separator was the last character. Poke in a null instead.
    *(char*)lastSeparator = '\0'; // Should use const_cast, but Unix has old compiler.
    leafPointer = GetLastSeparator(chars, inSeparator);
    char* result = leafPointer ? nsCRT::strdup(++leafPointer) : nsCRT::strdup(chars);
    // Restore the poked null before returning.
    *(char*)lastSeparator = inSeparator;
#if defined(XP_WIN) || defined(XP_OS2)
    // If it's a drive letter use the colon notation.
    if (!leafPointer && result[1] == '|' && result[2] == 0)
        result[1] = ':';
#endif
    return result;
} // nsSimpleCharString::GetLeaf


#if 0
#pragma mark -
#endif

#if (defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS))

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::MakeAllDirectories(const char* inPath, int mode)
// Make the path a valid one by creating all the intermediate directories.  Does NOT
// make the leaf into a directory.  This should be a unix path.
//----------------------------------------------------------------------------------------
{
    if (!inPath)
        return;
        
    char* pathCopy = nsCRT::strdup( inPath );
    if (!pathCopy)
        return;

    const char kSeparator = '/'; // I repeat: this should be a unix-style path.
    const int kSkipFirst = 1;

#if defined(XP_WIN) || defined(XP_OS2)
    // Either this is a relative path, or we ensure that it has
    // a drive letter specifier.
    NS_ASSERTION( pathCopy[0] != '/' || (pathCopy[1] && (pathCopy[2] == '|' || pathCopy[2] == '/')),
        "Not a UNC path and no drive letter!" );
#endif
    char* currentStart = pathCopy;
    char* currentEnd = strchr(currentStart + kSkipFirst, kSeparator);
    if (currentEnd)
    {
        nsFileSpec spec;
        *currentEnd = '\0';
        
#if defined(XP_WIN) || defined(XP_OS2)
        /* 
           if we have a drive letter path, we must make sure that the inital path has a '/' on it, or
           Canonify will turn "/c|" into a path relative to the running executable.
        */
        if (pathCopy[0] == '/' && pathCopy[1] && pathCopy[2] == '|')
        {
            char* startDir = (char*)PR_Malloc(strlen(pathCopy) + 2);
            strcpy(startDir, pathCopy);
            strcat(startDir, "/");

            spec = nsFilePath(startDir, PR_FALSE);
            
            PR_Free(startDir);
        }
        else
        {
            // move after server name and share name in UNC path
            if (pathCopy[0] == '/' &&
                currentEnd == currentStart+kSkipFirst)
            {
                *currentEnd = '/';
                currentStart = strchr(pathCopy+2, kSeparator);
                currentStart = strchr(currentStart+1, kSeparator);
                currentEnd = strchr(currentStart+1, kSeparator);
                if (currentEnd)
                    *currentEnd = '\0';
            }
            spec = nsFilePath(pathCopy, PR_FALSE);
        }
#else
        spec = nsFilePath(pathCopy, PR_FALSE);
#endif        
        do
        {
            // If the node doesn't exist, and it is not the initial node in a full path,
            // then make a directory (We cannot make the initial (volume) node).
            if (!spec.Exists() && *currentStart != kSeparator)
                spec.CreateDirectory(mode);
            
            currentStart = ++currentEnd;
            currentEnd = strchr(currentStart, kSeparator);
            if (!currentEnd)
                break;
            
            *currentEnd = '\0';

            spec += currentStart; // "lengthen" the path, adding the next node.
        } while (currentEnd);
    }
    nsCRT::free(pathCopy);
} // nsFileSpecHelpers::MakeAllDirectories

#endif // XP_UNIX || XP_WIN || XP_OS2 || XP_BEOS

#if 0
#pragma mark -
#endif

#if defined(XP_WIN)
#include "nsFileSpecWin.cpp" // Windows-specific implementations
#elif defined(XP_MAC)
//#include "nsFileSpecMac.cpp" // Macintosh-specific implementations
// we include the .cpp file in the project now.
#elif defined(XP_BEOS)
#include "nsFileSpecBeOS.cpp" // BeOS-specific implementations
#elif defined(XP_UNIX) || defined(XP_MACOSX)
#include "nsFileSpecUnix.cpp" // Unix-specific implementations
#elif defined(XP_OS2)
#include "nsFileSpecOS2.cpp" // OS/2-specific implementations
#endif

//========================================================================================
//                                nsFileURL implementation
//========================================================================================

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
    if (!inString)
        return;
    NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
    // Make canonical and absolute. Since it's a parameter to this constructor,
    // inString is escaped. We want to make an nsFilePath, which requires
    // an unescaped string.
    nsSimpleCharString unescapedPath(inString + kFileURLPrefixLength);
    unescapedPath.Unescape();
    nsFilePath path(unescapedPath, inCreateDirs);
    *this = path;
} // nsFileURL::nsFileURL
#endif

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
    NS_LossyConvertUCS2toASCII cstring(inString);
    if (!inString.Length())
        return;
    NS_ASSERTION(strstr(cstring.get(), kFileURLPrefix) == cstring.get(),
                 "Not a URL!");
    // Make canonical and absolute. Since it's a parameter to this constructor,
    // inString is escaped. We want to make an nsFilePath, which requires
    // an unescaped string.
    nsSimpleCharString unescapedPath(cstring.get() + kFileURLPrefixLength);
    unescapedPath.Unescape();
    nsFilePath path(unescapedPath, inCreateDirs);
    *this = path;
} // nsFileURL::nsFileURL
#endif

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
:    mURL(inOther.mURL)
#if defined(XP_MAC)
,    mFileSpec(inOther.GetFileSpec())
#endif
{
} // nsFileURL::nsFileURL

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
    *this = inOther;
} // nsFileURL::nsFileURL
#endif

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    *this = inOther;
} // nsFileURL::nsFileURL
#endif

//----------------------------------------------------------------------------------------
nsFileURL::~nsFileURL()
//----------------------------------------------------------------------------------------
{
}

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    // XXX is this called by nsFileSpecImpl.cpp::SetURLString?
    // if so, there's a bug...

    mURL = inString;
    NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
} // nsFileURL::operator =
#endif

//----------------------------------------------------------------------------------------
void nsFileURL::operator +=(const char* inRelativeUnixPath)
//----------------------------------------------------------------------------------------
{
    char* escapedPath = nsEscape(inRelativeUnixPath, url_Path);
    mURL += escapedPath;
    nsCRT::free(escapedPath);
#if defined(XP_MAC)
    mFileSpec += inRelativeUnixPath;
#endif
} // nsFileURL::operator +=

//----------------------------------------------------------------------------------------
nsFileURL nsFileURL::operator +(const char* inRelativeUnixPath) const
//----------------------------------------------------------------------------------------
{
   nsFileURL result(*this);
   result += inRelativeUnixPath;
   return result;
}  // nsFileURL::operator +

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
    mURL = inOther.mURL;
#if defined(XP_MAC)
    mFileSpec = inOther.GetFileSpec();
#endif
} // nsFileURL::operator =

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
    mURL = kFileURLPrefix;
    char* original = (char*)(const char*)inOther; // we shall modify, but restore.
    if (!original || !*original) return;
#if defined(XP_WIN) || defined(XP_OS2)
    // because we don't want to escape the '|' character, change it to a letter.
    // Note that a UNC path will not have a '|' character.
    char oldchar = original[2];
    original[2] = 'x';
    char* escapedPath = nsEscape(original, url_Path);
    original[2] = escapedPath[2] = oldchar; // restore it
#else
    char* escapedPath = nsEscape(original, url_Path);
#endif
    if (escapedPath)
        mURL += escapedPath;
    nsCRT::free(escapedPath);
} // nsFileURL::operator =
#endif

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    *this = nsFilePath(inOther);
    if (mURL[mURL.Length() - 1] != '/' && inOther.IsDirectory())
        mURL += "/";
} // nsFileURL::operator =
#endif

#if 0
#pragma mark -
#endif

//========================================================================================
//                                nsFilePath implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
    : mPath(inPath.mPath)
#if defined(XP_MAC)
    , mFileSpec(inPath.mFileSpec)
#endif
{
}

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(inString)
{
    if (mPath.IsEmpty())
    	return;
    	
    NS_ASSERTION(strstr(inString, kFileURLPrefix) != inString, "URL passed as path");

#if defined(XP_WIN) || defined(XP_OS2)
    nsFileSpecHelpers::UnixToNative(mPath);
#endif
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
#if defined(XP_WIN) || defined(XP_OS2)
    // Assert native path is of one of these forms:
    //    -  regular: X:\some\path
    //    -  UNC: \\some_machine\some\path
    NS_ASSERTION( mPath[1] == ':' || (mPath[0] == '\\' && mPath[1] == '\\'),
                 "unexpected canonical path" );
    nsFileSpecHelpers::NativeToUnix(mPath);
#endif
}
#endif

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(inString)
{
    if (mPath.IsEmpty())
    	return;

    NS_ASSERTION(strstr((const char*)mPath, kFileURLPrefix) != (const char*)mPath, "URL passed as path");
#if defined(XP_WIN) || defined(XP_OS2)
    nsFileSpecHelpers::UnixToNative(mPath);
#endif
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
#if defined(XP_WIN) || defined(XP_OS2)
    NS_ASSERTION( mPath[1] == ':' || (mPath[0] == '\\' && mPath[1] == '\\'),
                 "unexpected canonical path" );
    nsFileSpecHelpers::NativeToUnix(mPath);
#endif
}
#endif

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
    mPath = (const char*)inOther.mURL + kFileURLPrefixLength;
    mPath.Unescape();
}
#endif

#if (defined XP_UNIX || defined XP_BEOS)
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
:    mPath(inOther.mPath)
{
}
#endif // XP_UNIX

//----------------------------------------------------------------------------------------
nsFilePath::~nsFilePath()
//----------------------------------------------------------------------------------------
{
}

#if (defined XP_UNIX || defined XP_BEOS)
//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    // XXX bug here, again if.

    mPath = inOther.mPath;
}
#endif // XP_UNIX

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{

    NS_ASSERTION(strstr(inString, kFileURLPrefix) != inString, "URL passed as path");
    mPath = inString;
    if (mPath.IsEmpty())
    	return;
#if defined(XP_WIN) || defined(XP_OS2)
    nsFileSpecHelpers::UnixToNative(mPath);
#endif
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, PR_FALSE /* XXX? */);
#if defined(XP_WIN) || defined(XP_OS2)
    nsFileSpecHelpers::NativeToUnix(mPath);
#endif
}
#endif // XP_MAC

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
    mPath = (const char*)nsFilePath(inOther);
}
#endif

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
    mPath = inOther.mPath;
#if defined(XP_MAC)
    mFileSpec = inOther.GetFileSpec();
#endif
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator +=(const char* inRelativeUnixPath)
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inRelativeUnixPath, "Attempt append relative path with null path");

    char* escapedPath = nsEscape(inRelativeUnixPath, url_Path);
    mPath += escapedPath;
    nsCRT::free(escapedPath);
#if defined(XP_MAC)
    mFileSpec += inRelativeUnixPath;
#endif
} // nsFilePath::operator +=

//----------------------------------------------------------------------------------------
nsFilePath nsFilePath::operator +(const char* inRelativeUnixPath) const
//----------------------------------------------------------------------------------------
{
   NS_ASSERTION(inRelativeUnixPath, "Attempt append relative path with null path");

   nsFilePath resultPath(*this);
   resultPath += inRelativeUnixPath;
   return resultPath;
}  // nsFilePath::operator +


#if 0
#pragma mark -
#endif

//========================================================================================
//                                nsFileSpec implementation
//========================================================================================

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec()
//----------------------------------------------------------------------------------------
:    mError(NS_OK)		// XXX shouldn't this be NS_ERROR_NOT_INITIALIZED?
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
}

//----------------------------------------------------------------------------------------
void nsFileSpec::Clear()
//----------------------------------------------------------------------------------------
{
    mPath.SetToEmpty();
    mError = NS_ERROR_NOT_INITIALIZED;
}

#endif

//----------------------------------------------------------------------------------------
nsFileSpec::~nsFileSpec()
//----------------------------------------------------------------------------------------
{
    // mPath cleans itself up
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsPersistentFileDescriptor& inDescriptor)
//----------------------------------------------------------------------------------------
{
    *this = inDescriptor;
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFileURL& inURL)
//----------------------------------------------------------------------------------------
{
    *this = nsFilePath(inURL); // convert to unix path first
}

//----------------------------------------------------------------------------------------
void nsFileSpec::MakeUnique(const char* inSuggestedLeafName)
//----------------------------------------------------------------------------------------
{
    if (inSuggestedLeafName && *inSuggestedLeafName)
        SetLeafName(inSuggestedLeafName);

    MakeUnique();
} // nsFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsFileSpec::MakeUnique()
//----------------------------------------------------------------------------------------
{
    if (!Exists())
        return;

    char* leafName = GetLeafName();
    if (!leafName)
        return;

    char* lastDot = strrchr(leafName, '.');
    char* suffix = "";
    if (lastDot)
    {
        suffix = nsCRT::strdup(lastDot); // include '.'
        *lastDot = '\0'; // strip suffix and dot.
    }
    const int kMaxRootLength
        = nsFileSpecHelpers::kMaxCoreLeafNameLength - strlen(suffix) - 1;
    if ((int)strlen(leafName) > (int)kMaxRootLength)
        leafName[kMaxRootLength] = '\0';
    for (short indx = 1; indx < 1000 && Exists(); indx++)
    {
        // start with "Picture-1.jpg" after "Picture.jpg" exists
        char newName[nsFileSpecHelpers::kMaxFilenameLength + 1];
        sprintf(newName, "%s-%d%s", leafName, indx, suffix);
        SetLeafName(newName);
    }
    if (*suffix)
        nsCRT::free(suffix);
    nsCRT::free(leafName);
} // nsFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFileURL& inURL)
//----------------------------------------------------------------------------------------
{
    *this = nsFilePath(inURL); // convert to unix path first
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsPersistentFileDescriptor& inDescriptor)
//----------------------------------------------------------------------------------------
{

    nsCAutoString data;
    inDescriptor.GetData(data);

#if defined (XP_MAC) || defined(XP_MACOSX)
    // Decode descriptor into a Handle (which is actually an AliasHandle)
    char* decodedData = PL_Base64Decode(data.get(), data.Length(), nsnull);
    Handle aliasH = nsnull;
    mError = NS_FILE_RESULT(::PtrToHand(decodedData, &aliasH, (data.Length() * 3) / 4));
    PR_Free(decodedData);
    if (NS_FAILED(mError))
        return; // not enough memory?
#endif

#if defined(XP_MAC)
    Boolean changed;
    mError = NS_FILE_RESULT(::ResolveAlias(nsnull, (AliasHandle)aliasH, &mSpec, &changed));
    DisposeHandle((Handle) aliasH);
    mPath.SetToEmpty();
#elif defined(XP_MACOSX)
    Boolean changed;
    FSRef fileRef;
    mError = NS_FILE_RESULT(::FSResolveAlias(nsnull, (AliasHandle)aliasH, &fileRef, &changed));
    ::DisposeHandle(aliasH);

    UInt8 pathBuf[PATH_MAX];
    mError = NS_FILE_RESULT(::FSRefMakePath(&fileRef, pathBuf, PATH_MAX));
    if (NS_FAILED(mError))
      return;
    mPath = (const char*)pathBuf;
#else
    mPath = data.get();
    mError = NS_OK;
#endif
}

//========================================================================================
//                                UNIX & WIN nsFileSpec implementation
//========================================================================================

#if (defined XP_UNIX || defined XP_BEOS)
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
:    mPath((const char*)inPath)
,    mError(NS_OK)
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
    mPath = (const char*)inPath;
    mError = NS_OK;
}
#endif //XP_UNIX

#if (defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS))
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:    mPath(inSpec.mPath)
,    mError(NS_OK)
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(inString)
,    mError(NS_OK)
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(inString)
,    mError(NS_OK)
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
    mPath = inSpec.mPath;
    mError = inSpec.Error();
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    mPath = inString;
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, PR_FALSE /* XXX? */);
    mError = NS_OK;
}
#endif //XP_UNIX,XP_WIN,XP_OS2,XP_BEOS

//----------------------------------------------------------------------------------------
nsFileSpec nsFileSpec::operator + (const char* inRelativePath) const
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inRelativePath, "Attempt to append name with a null string");

    nsFileSpec resultSpec = *this;
    resultSpec += inRelativePath;
    return resultSpec;
} // nsFileSpec::operator +

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::operator == (const nsFileSpec& inOther) const
//----------------------------------------------------------------------------------------
{

#if defined(XP_MAC)
    if ( inOther.mSpec.vRefNum == mSpec.vRefNum &&
        inOther.mSpec.parID   == mSpec.parID &&
        EqualString(inOther.mSpec.name, mSpec.name, false, true))
        return PR_TRUE;
#else
    PRBool amEmpty = mPath.IsEmpty();
    PRBool heEmpty = inOther.mPath.IsEmpty();
    if (amEmpty) // we're the same if he's empty...
        return heEmpty;
    if (heEmpty) // ('cuz I'm not...)
        return PR_FALSE;
    
    nsSimpleCharString      str = mPath;
    nsSimpleCharString      inStr = inOther.mPath;
    
    // Length() is size of buffer, not length of string
    PRUint32 strLast = str.Length() - 1, inLast = inStr.Length() - 1;
#if defined(XP_WIN) || defined(XP_OS2)
#define DIR_SEPARATOR '\\'      // XXX doesn't NSPR have this?
    /* windows does not care about case. */
#ifdef XP_OS2
#define DIR_STRCMP     strcmp
#else
#define DIR_STRCMP    _stricmp
#endif
#else
#define DIR_SEPARATOR '/'
#if defined(VMS)
#define DIR_STRCMP     strcasecmp
#else
#define DIR_STRCMP     strcmp
#endif
#endif
    
    if(str[strLast] == DIR_SEPARATOR)
        str[strLast] = '\0';

    if(inStr[inLast] == DIR_SEPARATOR)
        inStr[inLast] = '\0';

    if (DIR_STRCMP(str, inStr ) == 0)
           return PR_TRUE;
#undef DIR_SEPARATOR
#undef DIR_STRCMP
#endif
   return PR_FALSE;
}

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::operator != (const nsFileSpec& inOther) const
//----------------------------------------------------------------------------------------
{
    return (! (*this == inOther) );
}

#if !defined(XP_MAC)
//----------------------------------------------------------------------------------------
// This is the only automatic conversion to const char*
// that is provided, and it allows the
// path to be "passed" to NSPR file routines.  This practice
// is VERY EVIL and should only be used to support legacy
// code.  Using it guarantees bugs on Macintosh. The path is NOT allocated, so do
// not even think of deleting (or freeing) it.
const char* nsFileSpec::GetCString() const
//----------------------------------------------------------------------------------------
{
    return mPath;
}
#endif

//----------------------------------------------------------------------------------------
// Is our spec a child of the provided parent?
PRBool nsFileSpec::IsChildOf(nsFileSpec &possibleParent)
//----------------------------------------------------------------------------------------
{
    nsFileSpec iter = *this, parent;
#ifdef DEBUG
    int depth = 0;
#endif
    while (1) {
#ifdef DEBUG
        // sanity
        NS_ASSERTION(depth < 100, "IsChildOf has lost its little mind");
        if (depth > 100)
            return PR_FALSE;
#endif
        if (iter == possibleParent)
            return PR_TRUE;

        iter.GetParent(parent); // shouldn't this be an error on parent?
        if (iter.Failed())
            return PR_FALSE;

        if (iter == parent)     // hit bottom
            return PR_FALSE;
        
        iter = parent;
#ifdef DEBUG
        depth++;
#endif
    }

    // not reached, but I bet some compiler will whine
    return PR_FALSE;
}

#if 0 
#pragma mark -
#endif

//========================================================================================
//    class nsPersistentFileDescriptor
//========================================================================================

//----------------------------------------------------------------------------------------
nsPersistentFileDescriptor::nsPersistentFileDescriptor(const nsPersistentFileDescriptor& inDesc)
//----------------------------------------------------------------------------------------
    : mDescriptorString(inDesc.mDescriptorString)
{
} // nsPersistentFileDescriptor::nsPersistentFileDescriptor

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::operator = (const nsPersistentFileDescriptor& inDesc)
//----------------------------------------------------------------------------------------
{
    mDescriptorString = inDesc.mDescriptorString;
} // nsPersistentFileDescriptor::operator =

//----------------------------------------------------------------------------------------
nsPersistentFileDescriptor::nsPersistentFileDescriptor(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
    *this = inSpec;
} // nsPersistentFileDescriptor::nsPersistentFileDescriptor

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
#if defined(XP_MAC)
    if (inSpec.Error())
        return;
    AliasHandle    aliasH;
    OSErr err = NewAlias(nil, inSpec.GetFSSpecPtr(), &aliasH);
    if (err != noErr)
        return;

    PRUint32 bytes = GetHandleSize((Handle) aliasH);
    HLock((Handle) aliasH);
    char* buf = PL_Base64Encode((const char*)*aliasH, bytes, nsnull);
    DisposeHandle((Handle) aliasH);

    mDescriptorString = buf;
    PR_Free(buf);
#elif  defined(XP_MACOSX)
    if (inSpec.Error())
        return;
    
    FSRef fileRef;
    Boolean isDir;
    OSErr err = ::FSPathMakeRef((const UInt8*)inSpec.GetCString(), &fileRef, &isDir);
    if (err != noErr)
        return;
    
    AliasHandle    aliasH;
    err = ::FSNewAlias(nsnull, &fileRef, &aliasH);
    if (err != noErr)
        return;

    PRUint32 bytes = ::GetHandleSize((Handle) aliasH);
    ::HLock((Handle)aliasH);
    char* buf = PL_Base64Encode((const char*)*aliasH, bytes, nsnull);
    ::DisposeHandle((Handle) aliasH);

    mDescriptorString = buf;
    PR_Free(buf);
#else
    mDescriptorString = inSpec.GetCString();
#endif // XP_MAC
} // nsPersistentFileDescriptor::operator =

//----------------------------------------------------------------------------------------
nsPersistentFileDescriptor::~nsPersistentFileDescriptor()
//----------------------------------------------------------------------------------------
{
} // nsPersistentFileDescriptor::~nsPersistentFileDescriptor

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::GetData(nsAFlatCString& outData) const
//----------------------------------------------------------------------------------------
{
    outData.Assign(mDescriptorString, mDescriptorString.Length());
}

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::SetData(const nsAFlatCString& inData)
//----------------------------------------------------------------------------------------
{
    mDescriptorString.CopyFrom(inData.get(), inData.Length());
}

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::SetData(const char* inData, PRInt32 inSize)
//----------------------------------------------------------------------------------------
{
    mDescriptorString.CopyFrom(inData, inSize);
}

//========================================================================================
//    class nsNSPRPath
//========================================================================================

//----------------------------------------------------------------------------------------
nsNSPRPath::operator const char*() const
// NSPR expects a UNIX path on unix and Macintosh, but a native path on windows. NSPR
// cannot be changed, so we have to do the dirty work.
//----------------------------------------------------------------------------------------
{
#if defined(XP_WIN) || defined(XP_OS2)
    if (!modifiedNSPRPath)
    {
        // If this is the first call, initialize modifiedNSPRPath. Start by cloning
        // mFilePath, but strip the leading separator, if present
        const char* unixPath = (const char*)mFilePath;
        if (!unixPath)
            return nsnull;

        ((nsNSPRPath*)this)->modifiedNSPRPath
                = nsCRT::strdup(*unixPath == '/' ? unixPath + 1: unixPath);
        
        // Replace the bar
        if (modifiedNSPRPath[1] == '|')
             modifiedNSPRPath[1] = ':';
        
        // Remove the ending separator only if it is not the last separator
        int len = strlen(modifiedNSPRPath);
        if (modifiedNSPRPath[len - 1 ] == '/' && modifiedNSPRPath[len - 2 ] != ':')
            modifiedNSPRPath[len - 1 ] = '\0';     
    }
    return modifiedNSPRPath;    
#else
    return (const char*)mFilePath;
#endif
}

//----------------------------------------------------------------------------------------
nsNSPRPath::~nsNSPRPath()
//----------------------------------------------------------------------------------------
{
#if defined(XP_WIN) || defined(XP_OS2)
    if (modifiedNSPRPath)
        nsCRT::free(modifiedNSPRPath);
#endif
}


nsresult 
NS_FileSpecToIFile(nsFileSpec* fileSpec, nsILocalFile* *result)
{
    nsresult rv;

    nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));

    if (!file) return NS_ERROR_FAILURE;

#if defined(XP_MAC)
    {
        FSSpec spec  = fileSpec->GetFSSpec();
        nsCOMPtr<nsILocalFileMac> psmAppMacFile = do_QueryInterface(file, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = psmAppMacFile->InitWithFSSpec(&spec);
        if (NS_FAILED(rv)) return rv;
        file = do_QueryInterface(psmAppMacFile, &rv);
    }
#else
    // XP_MACOSX: do this for OS X to preserve long filenames
    rv = file->InitWithNativePath(nsDependentCString(fileSpec->GetNativePathCString()));
#endif
    if (NS_FAILED(rv)) return rv;

    *result = file;
    NS_ADDREF(*result);
    return NS_OK;
}




