/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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




/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
    THESE CLASSES ARE DEPRECIATED AND UNSUPPORTED!  USE |nsIFile| and |nsILocalFile|.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */










//    First checked in on 98/11/20 by John R. McMullen in the wrong directory.
//    Checked in again 98/12/04.
//    Polished version 98/12/08.

//========================================================================================
//
//  Classes defined:
//
//      nsFilePath, nsFileURL, nsFileSpec, nsPersistentFileDescriptor
//      nsDirectoryIterator.
//
//  Q.  How should I represent files at run time?
//  A.  Use nsFileSpec.  Using char* will lose information on some platforms.
//
//  Q.  Then what are nsFilePath and nsFileURL for?
//  A.  Only when you need a char* parameter for legacy code.
//
//  Q.  How should I represent files in a persistent way (eg, in a disk file)?
//  A.  Use nsPersistentFileDescriptor.  Convert to and from nsFileSpec at run time.
//
//  This suite provides the following services:
//
//      1.  Encapsulates all platform-specific file details, so that files can be
//          described correctly without any platform #ifdefs
//
//      2.  Type safety.  This will fix the problems that used to occur because people
//          confused file paths.  They used to use const char*, which could mean three
//          or four different things.  Bugs were introduced as people coded, right up
//          to the moment Communicator 4.5 shipped.
//
//      3.  Used in conjunction with nsFileStream.h (q.v.), this supports all the power
//          and readability of the ansi stream syntax.
//
//          Basic example:
//
//              nsFilePath myPath("/Development/iotest.txt");
//
//              nsOutputFileStream testStream(nsFileSpec(myPath));
//              testStream << "Hello World" << nsEndl;
//
//      4.  Handy methods for manipulating file specifiers safely, e.g. MakeUnique(),
//          SetLeafName(), Exists().
//
//      5.  Easy cross-conversion.
//
//          Examples:
//
//              Initialize a URL from a string
//
//                  nsFileURL fileURL("file:///Development/MPW/MPW%20Shell");
//
//              Initialize a Unix-style path from a URL
//
//                  nsFilePath filePath(fileURL);
//
//              Initialize a file spec from a URL
//
//                  nsFileSpec fileSpec(fileURL);
//
//              Make the spec unique.
//
//                  fileSpec.MakeUnique();
//
//              Assign the spec to a URL (causing conversion)
//
//                  fileURL = fileSpec;
//
//              Assign a unix path using a string
//
//                  filePath = "/Development/MPW/SysErrs.err";
//
//              Assign to a file spec using a unix path (causing conversion).
//
//                  fileSpec = filePath;
//
//              Make this unique.
//
//                  fileSpec.MakeUnique();
//
//      6.  Fixes a bug that have been there for a long time, and
//          is inevitable if you use NSPR alone, where files are described as paths.
//
//          The problem affects platforms (Macintosh) in which a path does not fully
//          specify a file, because two volumes can have the same name.  This
//          is solved by holding a "private" native file spec inside the
//          nsFilePath and nsFileURL classes, which is used when appropriate.
//
//========================================================================================

#ifndef _FILESPEC_H_
#define _FILESPEC_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prtypes.h"

//========================================================================================
//                          Compiler-specific macros, as needed
//========================================================================================
#if !defined(NS_USING_NAMESPACE) && (defined(__MWERKS__) || defined(XP_WIN))
#define NS_USING_NAMESPACE
#endif

#ifdef NS_USING_NAMESPACE

#define NS_NAMESPACE_PROTOTYPE
#define NS_NAMESPACE namespace
#define NS_NAMESPACE_END
#define NS_EXPLICIT explicit
#else

#define NS_NAMESPACE_PROTOTYPE static
#define NS_NAMESPACE struct
#define NS_NAMESPACE_END ;
#define NS_EXPLICIT

#endif
//=========================== End Compiler-specific macros ===============================

#include "nsILocalFile.h"
#include "nsCOMPtr.h"

#if defined(XP_MAC)
#include <Files.h>
#include "nsILocalFileMac.h"
#elif defined(XP_UNIX) || defined (XP_OS2) || defined(XP_BEOS)
#if defined(XP_OS2)
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <sys/types.h> // required for dirent.h
#include "prio.h"
#endif
#include <dirent.h>
#elif defined(XP_WIN)

// This clashes with some of the Win32 system headers (specifically,
// winbase.h). Hopefully they'll have been included first; else we may
// have problems. We could include winbase.h before doing this;
// unfortunately, it's bring in too much crap and'd slow stuff down
// more than it's worth doing.
#ifdef CreateDirectory
#undef CreateDirectory
#endif

#include "prio.h"
#endif

//========================================================================================
// Here are the allowable ways to describe a file.
//========================================================================================

class nsFileSpec;             // Preferred.  For i/o use nsInputFileStream, nsOutputFileStream
class nsFilePath;
class nsFileURL;
class nsNSPRPath;             // This can be passed to NSPR file I/O routines, if you must.
class nsPersistentFileDescriptor; // Used for storage across program launches.

#define kFileURLPrefix "file://"
#define kFileURLPrefixLength (7)

class nsOutputStream;
class nsInputStream;
class nsIOutputStream;
class nsIInputStream;
class nsOutputFileStream;
class nsInputFileStream;
class nsOutputConsoleStream;

class nsString;

class nsIUnicodeEncoder;
class nsIUnicodeDecoder;

//========================================================================================
// Conversion of native file errors to nsresult values. These are really only for use
// in the file module, clients of this interface shouldn't really need them.
// Error results returned from this interface have, in the low-order 16 bits,
// native errors that are masked to 16 bits.  Assumption: a native error of 0 is success
// on all platforms. Note the way we define this using an inline function.  This
// avoids multiple evaluation if people go NS_FILE_RESULT(function_call()).
#define NS_FILE_RESULT(x) ns_file_convert_result((PRInt32)x)
nsresult ns_file_convert_result(PRInt32 nativeErr);
#define NS_FILE_FAILURE NS_FILE_RESULT(-1)

//========================================================================================
class NS_COM nsSimpleCharString
//  An envelope for char*: reference counted. Used internally by all the nsFileSpec
//  classes below.
//========================================================================================
{
public:
                                 nsSimpleCharString();
                                 nsSimpleCharString(const char*);
                                 nsSimpleCharString(const nsString&);
                                 nsSimpleCharString(const nsSimpleCharString&);
                                 nsSimpleCharString(const char* inData, PRUint32 inLength);
                                 
                                 ~nsSimpleCharString();
                                 
    void                         operator = (const char*);
    void                         operator = (const nsString&);
    void                         operator = (const nsSimpleCharString&);
                                 
                                 operator const char*() const { return mData ? mData->mString : 0; }
                                 operator char* ()
                                 {
                                     ReallocData(Length()); // requires detaching if shared...
                                     return mData ? mData->mString : 0;
                                 }
    PRBool                       operator == (const char*);
    PRBool                       operator == (const nsString&);
    PRBool                       operator == (const nsSimpleCharString&);

    void                         operator += (const char* inString);
    nsSimpleCharString           operator + (const char* inString) const;
    
    char                         operator [](int i) const { return mData ? mData->mString[i] : '\0'; }
    char&                        operator [](int i)
                                 {
                                     if (i >= (int)Length())
                                         ReallocData((PRUint32)i + 1);
                                     return mData->mString[i]; // caveat appelator
                                 }
    char&                        operator [](unsigned int i) { return (*this)[(int)i]; }
    
    void                         Catenate(const char* inString1, const char* inString2);
   
    void                         SetToEmpty(); 
    PRBool                       IsEmpty() const { return Length() == 0; }
    
    PRUint32                     Length() const { return mData ? mData->mLength : 0; }
    void                         SetLength(PRUint32 inLength) { ReallocData(inLength); }
    void                         CopyFrom(const char* inData, PRUint32 inLength);
    void                         LeafReplace(char inSeparator, const char* inLeafName);
    char*                        GetLeaf(char inSeparator) const; // use PR_Free()
    void                         Unescape();

protected:

    void                         AddRefData();
    void                         ReleaseData();
    void                         ReallocData(PRUint32 inLength);

    //--------------------------------------------------
    // Data
    //--------------------------------------------------

protected:

    struct Data {
        int         mRefCount;
        PRUint32    mLength;
        char        mString[1];
        };
    Data*                        mData;
}; // class nsSimpleCharString

//========================================================================================
class NS_COM nsFileSpec
//    This is whatever each platform really prefers to describe files as.  Declared first
//  because the other two types have an embedded nsFileSpec object.
//========================================================================================
{
    public:
                                nsFileSpec();
                                
                                // These two meathods take *native* file paths.
        NS_EXPLICIT             nsFileSpec(const char* inNativePath, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFileSpec(const nsString& inNativePath, PRBool inCreateDirs = PR_FALSE);
                                
        
        NS_EXPLICIT             nsFileSpec(const nsFilePath& inPath);
        NS_EXPLICIT             nsFileSpec(const nsFileURL& inURL);
        NS_EXPLICIT             nsFileSpec(const nsPersistentFileDescriptor& inURL);
                                nsFileSpec(const nsFileSpec& inPath);
        virtual                 ~nsFileSpec();

                                // These two operands take *native* file paths.
        void                    operator = (const char* inNativePath);
        void                    operator = (const nsString& inNativePath);

        void                    operator = (const nsFilePath& inPath);
        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const nsFileSpec& inOther);
        void                    operator = (const nsPersistentFileDescriptor& inOther);

        PRBool                  operator ==(const nsFileSpec& inOther) const;
        PRBool                  operator !=(const nsFileSpec& inOther) const;


                                // Returns a native path, and allows the
                                // path to be "passed" to legacy code.  This practice
                                // is VERY EVIL and should only be used to support legacy
                                // code.  Using it guarantees bugs on Macintosh.
                                // The path is cached and freed by the nsFileSpec destructor
                                // so do not delete (or free) it. See also nsNSPRPath below,
                                // if you really must pass a string  to PR_OpenFile().
                                // Doing so will introduce two automatic bugs.
       const char*              GetCString() const;

                                // Same as GetCString (please read the comments).
                                // Do not try to free this!
                                operator const char* () const { return GetCString(); }

                                // Same as GetCString (please read the comments).
                                // Do not try to free this!
       const char*              GetNativePathCString() const { return GetCString(); }

                                // Returns a path in unicode 
                                // converted from a file system charset.
       void                     GetNativePathString(nsString &nativePathString);

       PRBool                   IsChildOf(nsFileSpec &possibleParent);

#if defined(XP_MAC)
        // For Macintosh people, this is meant to be useful in its own right as a C++ version
        // of the FSSpec struct.        
                                nsFileSpec(
                                    short vRefNum,
                                    long parID,
                                    ConstStr255Param name,
                                    PRBool resolveAlias = PR_TRUE);

                                nsFileSpec(const FSSpec& inSpec, PRBool resolveAlias = PR_TRUE);
        void                    operator = (const FSSpec& inSpec);

                                operator FSSpec* () { return &mSpec; }
                                operator const FSSpec* const () { return &mSpec; }
                                operator  FSSpec& () { return mSpec; }
                                operator const FSSpec& () const { return mSpec; }
                                
        const FSSpec&           GetFSSpec() const { return mSpec; }
        FSSpec&                 GetFSSpec() { return mSpec; }
        ConstFSSpecPtr          GetFSSpecPtr() const { return &mSpec; }
        FSSpecPtr               GetFSSpecPtr() { return &mSpec; }
        void                    MakeAliasSafe();
        void                    MakeUnique(ConstStr255Param inSuggestedLeafName);
        StringPtr               GetLeafPName() { return mSpec.name; }
        ConstStr255Param        GetLeafPName() const { return mSpec.name; }

        OSErr                   GetCatInfo(CInfoPBRec& outInfo) const;

        OSErr                   SetFileTypeAndCreator(OSType type, OSType creator);
        OSErr                   GetFileTypeAndCreator(OSType* type, OSType* creator);

#endif // end of Macintosh utility methods.

        PRBool                  Valid() const { return NS_SUCCEEDED(Error()); }
        nsresult                Error() const
                                {
#if !defined(XP_MAC)
                                    if (mPath.IsEmpty() && NS_SUCCEEDED(mError)) 
                                        ((nsFileSpec*)this)->mError = NS_ERROR_NOT_INITIALIZED; 
#endif 
                                    return mError;
                                }
        PRBool                  Failed() const { return (PRBool)NS_FAILED(Error()); }

    //--------------------------------------------------
    // Queries and path algebra.  These do not modify the disk.
    //--------------------------------------------------

        char*                   GetLeafName() const; // Allocated.  Use nsCRT::free().

				// Unicode version of GetLeafName()
	void			GetLeafName(nsString &nativePathString);

#if 0
// needs implementing
                                // copy the leaf name into the supplied buffer, thus
                                // getting a copy without allocation. Buffer should be
                                // 64 chars big.
        void                    GetLeafNameCopy(char* destBuffer, PRInt32 bufferSize) const;
#endif        
                                // inLeafName can be a relative path, so this allows
                                // one kind of concatenation of "paths".
        void                    SetLeafName(const char* inLeafName);
        void                    SetLeafName(const nsString& inLeafName);

                                // Return the filespec of the parent directory. Used
                                // in conjunction with GetLeafName(), this lets you
                                // parse a path into a list of node names.  Beware,
                                // however, that the top node is still not a name,
                                // but a spec.  Volumes on Macintosh can have identical
                                // names.  Perhaps could be used for an operator --() ?
        void                    GetParent(nsFileSpec& outSpec) const;


                                // ie nsFileSpec::TimeStamp.  This is 32 bits now,
                                // but might change, eg, to a 64-bit class.  So use the
                                // typedef, and use a streaming operator to convert
                                // to a string, so that your code won't break.  It's
                                // none of your business what the number means.  Don't
                                // rely on the implementation.
        typedef PRUint32        TimeStamp;

                                // This will return different values on different
                                // platforms, even for the same file (eg, on a server).
                                // But if the platform is constant, it will increase after
                                // every file modification.
        void                    GetModDate(TimeStamp& outStamp) const;

        PRBool                  ModDateChanged(const TimeStamp& oldStamp) const
                                {
                                    TimeStamp newStamp;
                                    GetModDate(newStamp);
                                    return newStamp != oldStamp;
                                }
        
        PRUint32                GetFileSize() const;
        PRInt64                 GetDiskSpaceAvailable() const;
        
        nsFileSpec              operator + (const char* inRelativeUnixPath) const;
        nsFileSpec              operator + (const nsString& inRelativeUnixPath) const;

                                // Concatenate the relative path to this directory.
                                // Used for constructing the filespec of a descendant.
                                // This must be a directory for this to work.  This differs
                                // from SetLeafName(), since the latter will work
                                // starting with a sibling of the directory and throws
                                // away its leaf information, whereas this one assumes
                                // this is a directory, and the relative path starts
                                // "below" this.
        void                    operator += (const char* inRelativeUnixPath);

        void                    operator += (const nsString& inRelativeUnixPath);

        void                    MakeUnique();
        void                    MakeUnique(const char* inSuggestedLeafName);
        void                    MakeUnique(const nsString& inSuggestedLeafName);
    
                               
        PRBool                  IsDirectory() const;          // More stringent than Exists()                               
        PRBool                  IsFile() const;               // More stringent than Exists()
        PRBool                  Exists() const;

        PRBool                  IsHidden() const;
        
        PRBool                  IsSymlink() const;

    //--------------------------------------------------
    // Creation and deletion of objects.  These can modify the disk.
    //--------------------------------------------------

                                // Called for the spec of an alias.  Modifies the spec to
                                // point to the original.  Sets mError.
        nsresult                ResolveSymlink(PRBool& wasSymlink);

        void                    CreateDirectory(int mode = 0775 /* for unix */);
        void                    CreateDir(int mode = 0775) { CreateDirectory(mode); }
                                   // workaround for yet another VC++ bug with long identifiers.
        void                    Delete(PRBool inRecursive) const;
        nsresult                Truncate(PRInt32 aNewLength) const;
        void                    RecursiveCopy(nsFileSpec newDir) const;
        
        nsresult                Rename(const char* inNewName); // not const: gets updated
        nsresult                Rename(const nsString& inNewName);
        nsresult                CopyToDir(const nsFileSpec& inNewParentDirectory) const;
        nsresult                MoveToDir(const nsFileSpec& inNewParentDirectory);
        nsresult                Execute(const char* args) const;
        nsresult                Execute(const nsString& args) const;

    // Internal routine
    //--------------------------------------------------

    // Convert's routine from Native charset to Unicode.
    protected:

                                // use delete [] to free the returned buffer
       PRUnichar*               ConvertFromFileSystemCharset(const char *inString);
       static void              GetFileSystemCharset(nsString & fileSystemCharset);

    //--------------------------------------------------
    // Data
    //--------------------------------------------------

    protected:
    
                                // Clear the nsFileSpec contents, resetting it
                                // to the uninitialized state;
       void                     Clear();
       
                                friend class nsFilePath;
                                friend class nsFileURL;
                                friend class nsDirectoryIterator;
#if defined(XP_MAC)
        FSSpec                  mSpec;
#endif
        nsSimpleCharString      mPath;
        nsresult                mError;

}; // class nsFileSpec

// FOR HISTORICAL REASONS:

typedef nsFileSpec nsNativeFileSpec;

//========================================================================================
class NS_COM nsFileURL
//    This is an escaped string that looks like "file:///foo/bar/mumble%20fish".  Since URLs
//    are the standard way of doing things in mozilla, this allows a string constructor,
//    which just stashes the string with no conversion.
//========================================================================================
{
    public:
                                nsFileURL(const nsFileURL& inURL);
        NS_EXPLICIT             nsFileURL(const char* inURLString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFileURL(const nsString& inURLString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFileURL(const nsFilePath& inPath);
        NS_EXPLICIT             nsFileURL(const nsFileSpec& inPath);
        virtual                 ~nsFileURL();

//        nsString             GetString() const { return mPath; }
                                    // may be needed for implementation reasons,
                                    // but should not provide a conversion constructor.

        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const char* inURLString);
        void                    operator = (const nsString& inURLString)
                                {
                                    *this = NS_LossyConvertUCS2toASCII(inURLString).get();
                                }
        void                    operator = (const nsFilePath& inOther);
        void                    operator = (const nsFileSpec& inOther);

        void                    operator +=(const char* inRelativeUnixPath);
        nsFileURL               operator +(const char* inRelativeUnixPath) const;
                                operator const char* () const { return (const char*)mURL; } // deprecated.
        const char*             GetURLString() const { return (const char*)mURL; }
        							// Not allocated, so don't free it.
        const char*             GetAsString() const { return (const char*)mURL; }
        							// Not allocated, so don't free it.

#if defined(XP_MAC)
                                // Accessor to allow quick assignment to a mFileSpec
        const nsFileSpec&       GetFileSpec() const { return mFileSpec; }
#endif

    //--------------------------------------------------
    // Data
    //--------------------------------------------------

    protected:
                                friend class nsFilePath; // to allow construction of nsFilePath
        nsSimpleCharString      mURL;

#if defined(XP_MAC)
        // Since the path on the macintosh does not uniquely specify a file (volumes
        // can have the same name), stash the secret nsFileSpec, too.
        nsFileSpec              mFileSpec;
#endif
}; // class nsFileURL

//========================================================================================
class NS_COM nsFilePath
//    This is a string that looks like "/foo/bar/mumble fish".  Same as nsFileURL, but
//    without the "file:// prefix", and NOT %20 ENCODED! Strings passed in must be
//    valid unix-style paths in this format.
//========================================================================================
{
    public:
                                nsFilePath(const nsFilePath& inPath);
        NS_EXPLICIT             nsFilePath(const char* inUnixPathString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFilePath(const nsString& inUnixPathString, PRBool inCreateDirs = PR_FALSE);
        NS_EXPLICIT             nsFilePath(const nsFileURL& inURL);
        NS_EXPLICIT             nsFilePath(const nsFileSpec& inPath);
        virtual                 ~nsFilePath();

                                
                                operator const char* () const { return mPath; }
                                    // This will return a UNIX string.  If you
                                    // need a string that can be passed into
                                    // NSPR, take a look at the nsNSPRPath class.

        void                    operator = (const nsFilePath& inPath);
        void                    operator = (const char* inUnixPathString);
        void                    operator = (const nsString& inUnixPathString)
                                {
                                    *this = NS_LossyConvertUCS2toASCII(inUnixPathString).get();
                                }
        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const nsFileSpec& inOther);

        void                    operator +=(const char* inRelativeUnixPath);
        nsFilePath              operator +(const char* inRelativeUnixPath) const;

#if defined(XP_MAC)
    public:
                                // Accessor to allow quick assignment to a mFileSpec
        const nsFileSpec&       GetFileSpec() const { return mFileSpec; }
#endif

    //--------------------------------------------------
    // Data
    //--------------------------------------------------

    private:

        nsSimpleCharString       mPath;
#if defined(XP_MAC)
        // Since the path on the macintosh does not uniquely specify a file (volumes
        // can have the same name), stash the secret nsFileSpec, too.
        nsFileSpec               mFileSpec;
#endif
}; // class nsFilePath

//========================================================================================
class NS_COM nsPersistentFileDescriptor
// To save information about a file's location in another file, initialize
// one of these from your nsFileSpec, and then write this out to your output stream.
// To retrieve the info, create one of these, read its value from an input stream.
// and then make an nsFileSpec from it.
//========================================================================================
{
    public:
                                nsPersistentFileDescriptor() {}
                                    // For use prior to reading in from a stream
                                nsPersistentFileDescriptor(const nsPersistentFileDescriptor& inEncodedData);
        virtual                 ~nsPersistentFileDescriptor();
        void					operator = (const nsPersistentFileDescriptor& inEncodedData);
        
        // Conversions
        NS_EXPLICIT             nsPersistentFileDescriptor(const nsFileSpec& inSpec);
        void					operator = (const nsFileSpec& inSpec);
        
		// The following four functions are declared here (as friends). Their implementations
		// are in mozilla/base/src/nsFileSpecStreaming.cpp.
	
    	friend nsresult         Read(nsIInputStream* aStream, nsPersistentFileDescriptor&);
    	friend nsresult         Write(nsIOutputStream* aStream, const nsPersistentFileDescriptor&);
    	    // writes the data to a file
    	friend NS_COM nsInputStream& operator >> (nsInputStream&, nsPersistentFileDescriptor&);
    		// reads the data from a file
    	friend NS_COM nsOutputStream& operator << (nsOutputStream&, const nsPersistentFileDescriptor&);
    	    // writes the data to a file
        friend class nsFileSpec;

        void                    GetData(nsSimpleCharString& outData) const;
        void                    SetData(const nsSimpleCharString& inData);
        void                    GetData(nsSimpleCharString& outData, PRInt32& outSize) const;
        void                    SetData(const nsSimpleCharString& inData, PRInt32 inSize);
        void                    SetData(const char* inData, PRInt32 inSize);

    //--------------------------------------------------
    // Data
    //--------------------------------------------------

    protected:

        nsSimpleCharString      mDescriptorString;

}; // class nsPersistentFileDescriptor

//========================================================================================
class NS_COM nsDirectoryIterator
//  Example:
//
//       nsFileSpec parentDir(...); // directory over whose children we shall iterate
//       for (nsDirectoryIterator i(parentDir, PR_FALSE); i.Exists(); i++)
//       {
//              // do something with i.Spec()
//       }
//
//            - or -
//
//       for (nsDirectoryIterator i(parentDir, PR_TRUE); i.Exists(); i--)
//       {
//              // do something with i.Spec()
//       }
//       This one passed the PR_TRUE flag which will resolve any symlink encountered.
//========================================================================================
{
	public:
	                            nsDirectoryIterator( const nsFileSpec& parent,
	                            	                 PRBool resoveSymLinks);
#if !defined(XP_MAC)
	// Macintosh currently doesn't allocate, so needn't clean up.
	    virtual                 ~nsDirectoryIterator();
#endif
	    PRBool                  Exists() const { return mExists; }
	    nsDirectoryIterator&    operator ++(); // moves to the next item, if any.
	    nsDirectoryIterator&    operator ++(int) { return ++(*this); } // post-increment.
	    nsDirectoryIterator&    operator --(); // moves to the previous item, if any.
	    nsDirectoryIterator&    operator --(int) { return --(*this); } // post-decrement.
	                            operator nsFileSpec&() { return mCurrent; }
	    
	    nsFileSpec&             Spec() { return mCurrent; }
	     
	private:

#if defined(XP_MAC)
        OSErr                   SetToIndex();
#endif

    //--------------------------------------------------
    // Data
    //--------------------------------------------------

	private:

	    nsFileSpec              mCurrent;
	    PRBool                  mExists;
        PRBool                  mResoveSymLinks;

#if (defined(XP_UNIX) || defined(XP_BEOS) || defined (XP_WIN) || defined(XP_OS2))
	    nsFileSpec		        mStarting;
#endif
        
#if defined(XP_MAC)
           short                                       mVRefNum;
           long                                        mParID;
           short         mIndex;
           short         mMaxIndex;
#elif defined(XP_UNIX) || defined(XP_BEOS)
	    DIR*                    mDir;
#elif defined(XP_WIN) || defined(XP_OS2)
        PRDir*                  mDir; // XXX why not use PRDir for Unix too?
#endif
}; // class nsDirectoryIterator

//========================================================================================
class NS_COM nsNSPRPath
//  This class will allow you to pass any one of the nsFile* classes directly into NSPR
//  without the need to worry about whether you have the right kind of filepath or not.
//  It will also take care of cleaning up any allocated memory. 
//========================================================================================
{
public:
    NS_EXPLICIT                  nsNSPRPath(const nsFileSpec& inSpec)
                                     : mFilePath(inSpec), modifiedNSPRPath(nsnull) {}
    NS_EXPLICIT                  nsNSPRPath(const nsFileURL& inURL)
                                     : mFilePath(inURL), modifiedNSPRPath(nsnull) {}
    NS_EXPLICIT                  nsNSPRPath(const nsFilePath& inUnixPath)
                                     : mFilePath(inUnixPath), modifiedNSPRPath(nsnull) {}
    
    virtual                      ~nsNSPRPath();    
 
                                 operator const char*() const;
                                    // Returns the path
                                    // that NSPR file routines expect on each platform.
                                    // Concerning constness, this can modify
                                    // modifiedNSPRPath, but it's really just "mutable". 

    //--------------------------------------------------
    // Data
    //--------------------------------------------------

private:

    nsFilePath                   mFilePath;
    char*                        modifiedNSPRPath; // Currently used only on XP_WIN,XP_OS2
}; // class nsNSPRPath


NS_COM nsresult NS_FileSpecToIFile(nsFileSpec* fileSpec, nsILocalFile* *result);

#endif //  _FILESPEC_H_
