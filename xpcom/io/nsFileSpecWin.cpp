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
 
//	This file is included by nsFileSpec.cp, and includes the Windows-specific
//	implementations.

#include <sys/stat.h>
#include <direct.h>

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::Canonify(char*& ioPath, bool inMakeDirs)
// Canonify, make absolute, and check whether directories exist
//----------------------------------------------------------------------------------------
{
	NS_NOTYETIMPLEMENTED("Please, some Winhead please write this!");
}

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
:	mPath(NULL)
{
	*this = inPath;
}

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	// Strip the leading slash, which should 
	// leave us pointing to a drive letter.
	nsFileSpecHelpers::StringAssign(mPath, (const char*)inPath + 1);

	// Convert the vertical slash to a colon
	char* cp = mPath + 1;
	if (strstr(cp, "|/") == cp)
		*cp = ':';
	
	// Convert '/' to '\'.
	while (*++cp)
	{
		if (*cp == '/')
			*cp = '\\';
	}
} // nsNativeFileSpec::operator =

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:	mPath(NULL)
{
	*this = inSpec;
} // nsFilePath::nsFilePath

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	// Convert '\' to '/'
	nsFileSpecHelpers::StringAssign(mPath, inSpec.mPath);
	for (char* cp = mPath; *cp; cp++)
	{
		if (*cp == '\\')
			*cp = '/';
	}
} // nsFilePath::operator =

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::SetLeafName(const char* inLeafName)
//----------------------------------------------------------------------------------------
{
	nsFileSpecHelpers::LeafReplace(mPath, '\\', inLeafName);
} // nsNativeFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsNativeFileSpec::GetLeafName() const
//----------------------------------------------------------------------------------------
{
	return nsFileSpecHelpers::GetLeaf(mPath, '\\');
} // nsNativeFileSpec::GetLeafName

//----------------------------------------------------------------------------------------
bool nsNativeFileSpec::Exists() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
	return 0 == stat(mPath, &st); 
} // nsNativeFileSpec::Exists

//----------------------------------------------------------------------------------------
bool nsNativeFileSpec::IsFile() const
//----------------------------------------------------------------------------------------
{
//	struct stat st;
//	return 0 == stat(mPath, &st) && S_ISREG(st.st_mode); 
  return 0;
} // nsNativeFileSpec::IsFile

//----------------------------------------------------------------------------------------
bool nsNativeFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
//	struct stat st;
//	return 0 == stat(mPath, &st) && S_ISDIR(st.st_mode); 
  return 0;
} // nsNativeFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::GetParent(nsNativeFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
	nsFileSpecHelpers::StringAssign(outSpec.mPath, mPath);
	char* cp = strrchr(outSpec.mPath, '\\');
	if (cp)
		*cp = '\0';
} // nsNativeFileSpec::GetParent

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator += (const char* inRelativePath)
//----------------------------------------------------------------------------------------
{
	if (!inRelativePath || !mPath)
		return;
	
	if (mPath[strlen(mPath) - 1] != '\\')
		char* newPath = nsFileSpecHelpers::ReallocCat(mPath, "\\");
	SetLeafName(inRelativePath);
} // nsNativeFileSpec::operator +=

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::CreateDirectory(int mode)
//----------------------------------------------------------------------------------------
{
	// Note that mPath is canonical!
	NS_NOTYETIMPLEMENTED("Please, some Winhead please fix this!");
	mkdir(mPath);
} // nsNativeFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::Delete(bool inRecursive)
//----------------------------------------------------------------------------------------
{
    if (IsDirectory())
    {
	    if (inRecursive)
	    {
			for (nsDirectoryIterator i(*this); i; i++)
			{
				nsNativeFileSpec& child = (nsNativeFileSpec&)i;
				child.Delete(inRecursive);
			}		
	    }
		NS_NOTYETIMPLEMENTED("Please, some Winhead please fix this!");
	    rmdir(mPath);
	}
	else
	{
		NS_NOTYETIMPLEMENTED("Please, some Winhead please write this!");
		remove(mPath);
	}
} // nsNativeFileSpec::Delete

//========================================================================================
//								nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(
	const nsNativeFileSpec& inDirectory
,	int inIterateDirection)
//----------------------------------------------------------------------------------------
	: mCurrent(inDirectory)
#if 0 // Some kind winhead please build, test, and remove when ready.
	, mDir(nsnull)
#endif // XP_PC -- ifdef to be removed.
	, mExists(false)
{
NS_NOTYETIMPLEMENTED("Please, some kind Winhead please check this!");
    mCurrent += "foo";
#if 0 // Some kind winhead please build, test, and remove when ready.
    mDir = opendir(mPath);
#endif // XP_PC -- ifdef to be removed.
    ++(*this);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator::~nsDirectoryIterator()
//----------------------------------------------------------------------------------------
{
NS_NOTYETIMPLEMENTED("Please, some kind Winhead please check this!");
#if 0 // Some kind winhead please build, test, and remove when ready.
    if (mDir)
	    closedir(mDir);
#endif // XP_PC -- ifdef to be removed.
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator ++ ()
//----------------------------------------------------------------------------------------
{
	mExists = false;
NS_NOTYETIMPLEMENTED("Please, some kind Winhead please check this!");
#if 0 // Some kind winhead please build, test, and remove when ready.
	if (!mDir)
		return *this;
    struct dirent* entry = readdir(mDir);
	if (entry)
	{
		mExists = true;
		mCurrent.SetLeafName(entry->d_name);
	}
#endif // XP_PC -- ifdef to be removed.
	return *this;
} // nsDirectoryIterator::operator ++

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator -- ()
//----------------------------------------------------------------------------------------
{
	return ++(*this); // can't do it backwards.
} // nsDirectoryIterator::operator --

