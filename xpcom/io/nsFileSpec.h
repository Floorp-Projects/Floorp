/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//    First checked in on 98/11/20 by John R. McMullen in the wrong directory.
//    Checked in again 98/12/04.
//    Polished version 98/12/08.

//========================================================================================
//
//  Classes defined:
//
//      nsFilePath, nsFileURL, nsNativeFileSpec.
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
//          and readability of the ansi stream syntax.  ALL METHODS OF istream, ostream,
//          AND iostream ARE AVAILABLE!
//
//          Basic example:
//
//              nsFilePath myPath("/Development/iotest.txt");
//
//              nsOutputFileStream testStream(myPath);
//              testStream << "Hello World" << endl;
//
//      4.  Handy methods for manipulating file specifiers safely, e.g. MakeUnique(),
//          SetLeafName(), Exists().
//
//      5.  Easy cross-conversion.
//
//          Examples:
//
//              Initialize a URL from a string without suffix
//
//                  nsFileURL fileURL("file:///Development/MPW/MPW%20Shell");
//
//              Initialize a Unix path from a URL
//
//                  nsFilePath filePath(fileURL);
//
//              Initialize a native file spec from a URL
//
//                  nsNativeFileSpec fileSpec(fileURL);
//
//              Make the spec unique (this one has no suffix).
//
//                  fileSpec.MakeUnique();
//
//              Assign the spec to a URL
//
//                  fileURL = fileSpec;
//
//              Assign a unix path using a string with a suffix.
//
//                  filePath = "/Development/MPW/SysErrs.err";
//
//              Assign to a file spec using a unix path.
//
//                  fileSpec = filePath;
//
//              Make this unique (this one has a suffix).
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
//      Not yet done:
//
//          Equality operators... much more.
//
//========================================================================================

#ifndef _FILESPEC_H_
#define _FILESPEC_H_

//========================================================================================
//                          Compiler-specific macros, as needed
//========================================================================================
#if !defined(NS_USING_NAMESPACE) && (defined(__MWERKS__) || defined(XP_PC))
#define NS_USING_NAMESPACE
#endif

#ifdef NS_USING_NAMESPACE
#define NS_NAMESPACE_PROTOTYPE
#define NS_NAMESPACE namespace
#define NS_NAMESPACE_END
#include <ostream>
	using std::ostream;
#else
#define NS_NAMESPACE_PROTOTYPE static
#define NS_NAMESPACE struct
#define NS_NAMESPACE_END ;
#include <ostream.h>
#endif
//=========================== End Compiler-specific macros ===============================

#include "nsDebug.h"

#ifdef XP_MAC
#include <Files.h>
#endif

//========================================================================================
// Here are the allowable ways to describe a file.
//========================================================================================

class nsFilePath;                 // This can be passed to NSPR file I/O routines.
class nsFileURL;
class nsNativeFileSpec;

#define kFileURLPrefix "file://"
#define kFileURLPrefixLength (7)

//========================================================================================
class nsNativeFileSpec
//    This is whatever each platform really prefers to describe files as.  Declared first
//  because the other two types have an embeded nsNativeFileSpec object.
//========================================================================================
{
    public:
                                nsNativeFileSpec();
        explicit                nsNativeFileSpec(const char* inString);
        explicit                nsNativeFileSpec(const nsFilePath& inPath);
        explicit                nsNativeFileSpec(const nsFileURL& inURL);
                                nsNativeFileSpec(const nsNativeFileSpec& inPath);
        virtual                 ~nsNativeFileSpec();

        void                    operator = (const char* inPath);
        void                    operator = (const nsFilePath& inPath);
        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const nsNativeFileSpec& inOther);

#ifdef XP_MAC
        // For Macintosh people, this is meant to be useful in its own right as a C++ version
        // of the FSSpec struct.        
                                nsNativeFileSpec(
                                    short vRefNum,
                                    long parID,
                                    ConstStr255Param name);
                                nsNativeFileSpec(const FSSpec& inSpec)
                                    : mSpec(inSpec), mError(noErr) {}

                                operator FSSpec* () { return &mSpec; }
                                operator const FSSpec* const () { return &mSpec; }
                                operator  FSSpec& () { return mSpec; }
                                operator const FSSpec& () const { return mSpec; }
        OSErr                   Error() const { return mError; }
        void                    MakeAliasSafe();
        							// Called for the spec of an alias.  Copies the alias to
        							// a secret temp directory and modifies the spec to point
        							// to it.  Sets mError.
        void                    ResolveAlias(bool& wasAliased);
        							// Called for the spec of an alias.  Modifies the spec to
        							// point to the original.  Sets mError.
        void                    MakeUnique(ConstStr255Param inSuggestedLeafName);
        StringPtr               GetLeafPName() { return mSpec.name; }
        ConstStr255Param        GetLeafPName() const { return mSpec.name; }
#endif // end of Macintosh utility methods.

#ifdef XP_MAC
        bool                    Valid() const { return mError == noErr; }
#else
        bool                    Valid() const { return true; } // Fixme.
#endif // XP_MAC

        friend                  ostream& operator << (ostream& s, const nsNativeFileSpec& spec);

        char*                   GetLeafName() const; // Allocated.  Use delete [].
        void                    SetLeafName(const char* inLeafName);
        bool                    Exists() const;
        void                    MakeUnique();
        void                    MakeUnique(const char* inSuggestedLeafName);
    
    private:
                                friend class nsFilePath;
#ifdef XP_MAC
        FSSpec                  mSpec;
        OSErr                   mError;
#else
        char*                   mPath;
#endif
}; // class nsNativeFileSpec

//========================================================================================
class nsFileURL
//    This is an escaped string that looks like "file:///foo/bar/mumble%20fish".  Since URLs
//    are the standard way of doing things in mozilla, this allows a string constructor,
//    which just stashes the string with no conversion.
//========================================================================================
{
    public:
                                nsFileURL(const nsFileURL& inURL);
        explicit                nsFileURL(const char* inString);
        explicit                nsFileURL(const nsFilePath& inPath);
        explicit                nsFileURL(const nsNativeFileSpec& inPath);
        virtual                 ~nsFileURL();

//        nsString             GetString() const { return mPath; }
                                    // may be needed for implementation reasons,
                                    // but should not provide a conversion constructor.

        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const char* inString);
        void                    operator = (const nsFilePath& inOther);
        void                    operator = (const nsNativeFileSpec& inOther);

        friend                  ostream& operator << (ostream& s, const nsFileURL& spec);

#ifdef XP_MAC
                                // Accessor to allow quick assignment to a mNativeFileSpec
        const nsNativeFileSpec& GetNativeSpec() const { return mNativeFileSpec; }
#endif
    private:
        // Should not be defined (only nsFilePath is to be treated as strings.
                                operator char* ();
                                operator const char* const ();
    private:
    							friend class nsFilePath; // to allow construction of nsFilePath
        char*                   mURL;
#ifdef XP_MAC
        // Since the path on the macintosh does not uniquely specify a file (volumes
        // can have the same name), stash the secret nsNativeFileSpec, too.
        nsNativeFileSpec        mNativeFileSpec;
#endif
}; // class nsFileURL

//========================================================================================
class nsFilePath
//    This is a string that looks like "/foo/bar/mumble%20fish".  Same as nsFileURL, but
//    without the "file:// prefix".
//========================================================================================
{
    public:
                                nsFilePath(const nsFilePath& inPath);
        explicit                nsFilePath(const char* inString);
        explicit                nsFilePath(const nsFileURL& inURL);
        explicit                nsFilePath(const nsNativeFileSpec& inPath);
        virtual                 ~nsFilePath();

                                
                                operator const char* () const { return mPath; }
                                    // This is the only automatic conversion to const char*
                                    // that is provided, and it allows the
                                    // path to be "passed" to NSPR file routines.
                                operator char* () { return mPath; }
                                    // This is the only automatic conversion to string
                                    // that is provided, because a naked string should
                                    // only mean a standard file path.

        void                    operator = (const nsFilePath& inPath);
        void                    operator = (const char* inString);
        void                    operator = (const nsFileURL& inURL);
        void                    operator = (const nsNativeFileSpec& inOther);

#ifdef XP_MAC
    public:
                                // Accessor to allow quick assignment to a mNativeFileSpec
        const nsNativeFileSpec& GetNativeSpec() const { return mNativeFileSpec; }
#endif

    private:

        char*	                mPath;
#ifdef XP_MAC
        // Since the path on the macintosh does not uniquely specify a file (volumes
        // can have the same name), stash the secret nsNativeFileSpec, too.
        nsNativeFileSpec        mNativeFileSpec;
#endif
}; // class nsFilePath

#endif //  _FILESPEC_H_
