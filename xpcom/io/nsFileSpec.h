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

//	First checked in on 98/11/20 by John R. McMullen.  Checked in again 98/12/04.

#ifndef _FILESPEC_H_
#define _FILESPEC_H_

//========================================================================================
//	This is intended to be part of the API for all C++ in the mozilla code base from now on.
//	It provides
//		*	Type-safe ways of describing files (no more char* parameters)
//		*	Conversions between these
//		*	Methods for testing existence and for forcing uniqueness.
//
//		A file specification can come from two outside sources:
//			1.	A file:// URL, or
//			2.	A native spec (such as an OS-based save/open dialog and the like).
//		Therefore, these are the only ingredients one can use to make a file specification.
//
//		Once one of our spec types has been made, conversions are provided between them
//
//		In addition, string accessors are provided, because people like to manipulate
//		nsFileURL and nsUnixFilePath strings directly.
//========================================================================================

#include <string>
#include "nsDebug.h"
#ifdef XP_MAC
#include <Files.h>
#endif

//========================================================================================
// Here are the allowable ways to describe a file.
//========================================================================================

class nsUnixFilePath; 				// This can be passed to NSPR file I/O routines.
class nsFileURL;
class nsNativeFileSpec;

#define kFileURLPrefix "file://"
#define kFileURLPrefixLength (7)

//========================================================================================
class nsNativeFileSpec
//	This is whatever each platform really prefers to describe files as. 
//========================================================================================
{
	public:
								nsNativeFileSpec();
								nsNativeFileSpec(const std::string& inString);
								nsNativeFileSpec(const nsUnixFilePath& inPath);
								nsNativeFileSpec(const nsFileURL& inURL);
								nsNativeFileSpec(const nsNativeFileSpec& inPath);

		void					operator = (const std::string& inPath);
		void					operator = (const nsUnixFilePath& inPath);
		void					operator = (const nsFileURL& inURL);
		void					operator = (const nsNativeFileSpec& inOther);

#ifdef XP_MAC
		// For Macintosh people, this is meant to be useful in its own right as a C++ version
		// of the FSSPec class.		
								nsNativeFileSpec(
									short vRefNum,
									long parID,
									ConstStr255Param name);
								nsNativeFileSpec(const FSSpec& inSpec)
									: mSpec(inSpec), mError(noErr) {}

								operator FSSpec* () { return &mSpec; }
								operator const FSSpec* const () { return &mSpec; }
								operator FSSpec& () { return mSpec; }
								operator const FSSpec& () const { return mSpec; }
		bool					Valid() const { return mError == noErr; }
		OSErr					Error() const { return mError; }
		void					MakeUnique(ConstStr255Param inSuggestedLeafName);
		StringPtr				GetLeafPName() { return mSpec.name; }
		ConstStr255Param		GetLeafPName() const { return mSpec.name; }
#else
		bool					Valid() const { return TRUE; } // Fixme.
#endif

#if DEBUG
		friend					ostream& operator << (ostream& s, const nsNativeFileSpec& spec);
#endif
		string					GetLeafName() const;
		void					SetLeafName(const std::string& inLeafName);
		bool					Exists() const;
		void					MakeUnique();
		void					MakeUnique(const std::string& inSuggestedLeafName);
	
	private:
								friend class nsUnixFilePath;
#ifdef XP_MAC
		FSSpec					mSpec;
		OSErr					mError;
#elif defined(XP_UNIX) || defined(XP_WIN)
		std::string				mPath;
#endif
}; // class nsNativeFileSpec

//========================================================================================
class nsFileURL
//	This is an escaped string that looks like "file:///foo/bar/mumble%20fish".  Since URLs
//	are the standard way of doing things in mozilla, this allows a string constructor,
//	which just stashes the string with no conversion.
//========================================================================================
{
	public:
								nsFileURL(const nsFileURL& inURL);
								nsFileURL(const std::string& inString);
								nsFileURL(const nsUnixFilePath& inPath);
								nsFileURL(const nsNativeFileSpec& inPath);
									
								operator std::string& () { return mURL; }
									// This is the only automatic conversion to string
									// that is provided, because a naked string should
									// only mean a file URL.

//		std::string 			GetString() const { return mPath; }
									// may be needed for implementation reasons,
									// but should not provide a conversion constructor.

		void					operator = (const nsFileURL& inURL);
		void					operator = (const std::string& inString);
		void					operator = (const nsUnixFilePath& inOther);
		void					operator = (const nsNativeFileSpec& inOther);

#ifdef XP_MAC
								// Accessor to allow quick assignment to a mNativeFileSpec
		const nsNativeFileSpec&	GetNativeSpec() const { return mNativeFileSpec; }
#endif
	private:
	
		std::string				mURL;
#ifdef XP_MAC
		// Since the path on the macintosh does not uniquely specify a file (volumes
		// can have the same name), stash the secret nsNativeFileSpec, too.
		nsNativeFileSpec		mNativeFileSpec;
#endif
}; // class nsFileURL

//========================================================================================
class nsUnixFilePath
//	This is a string that looks like "/foo/bar/mumble%20fish".  Same as nsFileURL, but
//	without the "file:// prefix".
//========================================================================================
{
	public:
								nsUnixFilePath(const nsUnixFilePath& inPath);
								nsUnixFilePath(const std::string& inString);
								nsUnixFilePath(const nsFileURL& inURL);
								nsUnixFilePath(const nsNativeFileSpec& inPath);

								
								operator const char* () const { return mPath.c_str(); }
									// This is the only automatic conversion to const char*
									// that is provided, and it allows the
									// path to be "passed" to NSPR file routines.

		void					operator = (const nsUnixFilePath& inPath);
		void					operator = (const std::string& inString);
		void					operator = (const nsFileURL& inURL);
		void					operator = (const nsNativeFileSpec& inOther);

#ifdef XP_MAC
	public:
								// Accessor to allow quick assignment to a mNativeFileSpec
		const nsNativeFileSpec&	GetNativeSpec() const { return mNativeFileSpec; }
#endif

	private:
		// Should not be defined (only file URLs are to be treated as strings.
								operator std::string& ();
	private:

		std::string				mPath;
#ifdef XP_MAC
		// Since the path on the macintosh does not uniquely specify a file (volumes
		// can have the same name), stash the secret nsNativeFileSpec, too.
		nsNativeFileSpec		mNativeFileSpec;
#endif
}; // class nsUnixFilePath

#ifdef XP_UNIX
//========================================================================================
//								UNIX nsUnixFilePath implementation
//========================================================================================

//----------------------------------------------------------------------------------------
inline nsUnixFilePath::nsUnixFilePath(const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
:	mPath((std::string&)inOther)
{
}
//----------------------------------------------------------------------------------------
inline void nsUnixFilePath::operator = (const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
	mPath = (std::string&)inOther;
}
#endif // XP_UNIX

//========================================================================================
//								COMMON nsNativeFileSpec implementation
//========================================================================================

//----------------------------------------------------------------------------------------
inline nsNativeFileSpec::nsNativeFileSpec(const nsFileURL& inURL)
//----------------------------------------------------------------------------------------
{
	*this = nsUnixFilePath(inURL); // convert to unix path first
}

//----------------------------------------------------------------------------------------
inline void nsNativeFileSpec::operator = (const nsFileURL& inURL)
//----------------------------------------------------------------------------------------
{
	*this = nsUnixFilePath(inURL); // convert to unix path first
}

//========================================================================================
//								UNIX & WIN nsNativeFileSpec implementation
//========================================================================================

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
inline nsNativeFileSpec::nsNativeFileSpec(const nsUnixFilePath& inPath)
//----------------------------------------------------------------------------------------
:	mPath((std::string&)inPath)
{
}
#endif // XP_UNIX

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
inline void nsNativeFileSpec::operator = (const nsUnixFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	mPath = (std::string&)inPath;
}
#endif //XP_UNIX

#if defined(XP_UNIX) || defined(XP_WIN)
//----------------------------------------------------------------------------------------
inline nsNativeFileSpec::nsNativeFileSpec(const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:	mPath((std::string&)inSpec)
{
}
#endif //XP_UNIX

#if defined(XP_UNIX) || defined(XP_WIN)
//----------------------------------------------------------------------------------------
inline nsNativeFileSpec::nsNativeFileSpec(const std::string& inString)
//----------------------------------------------------------------------------------------
:	mPath(inString)
{
}
#endif //XP_UNIX

#if defined(XP_UNIX) || defined(XP_WIN)
//----------------------------------------------------------------------------------------
inline void nsNativeFileSpec::operator = (const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	mPath = (std::string&)inSpec;
}
#endif //XP_UNIX


#if defined(XP_UNIX) || defined(XP_WIN)
//----------------------------------------------------------------------------------------
inline nsNativeFileSpec::operator = (const std::string& inString)
//----------------------------------------------------------------------------------------
{
	mPath = inString;
}
#endif //XP_UNIX

#if (defined(XP_UNIX) || defined(XP_WIN)) && DEBUG
//----------------------------------------------------------------------------------------
inline ostream& operator << (ostream& s, const nsNativeFileSpec& spec)
//----------------------------------------------------------------------------------------
{
	return (s << (std::string&)spec.mPath);
}
#endif // DEBUG && XP_UNIX

#endif //  _FILESPEC_H_
