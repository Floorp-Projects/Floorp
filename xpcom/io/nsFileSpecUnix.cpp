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
 
//    This file is included by nsFileSpec.cpp, and includes the Unix-specific
//    implementations.

#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::Canonify(char*& ioPath, bool inMakeDirs)
// Canonify, make absolute, and check whether directories exist
//----------------------------------------------------------------------------------------
{
    if (!ioPath)
        return;
    if (inMakeDirs)
    {
        const mode_t mode = 0700;
        nsFileSpecHelpers::MakeAllDirectories(ioPath, mode);
    }
    char buffer[MAXPATHLEN];
    errno = 0;
    *buffer = '\0';
    char* canonicalPath = realpath(ioPath, buffer);
    if (!canonicalPath)
    {
        // Linux's realpath() is pathetically buggy.  If the reason for the nil
        // result is just that the leaf does not exist, strip the leaf off,
        // process that, and then add the leaf back.
        char* allButLeaf = nsFileSpecHelpers::StringDup(ioPath);
        if (!allButLeaf)
            return;
        char* lastSeparator = strrchr(allButLeaf, '/');
        if (lastSeparator)
        {
            *lastSeparator = '\0';
            canonicalPath = realpath(allButLeaf, buffer);
            strcat(buffer, "/");
            // Add back the leaf
            strcat(buffer, ++lastSeparator);
        }
        delete [] allButLeaf;
    }
    if (!canonicalPath && *ioPath != '/' && !inMakeDirs)
    {
        // Well, if it's a relative path, hack it ourselves.
        canonicalPath = realpath(".", buffer);
        if (canonicalPath)
        {
            strcat(canonicalPath, "/");
            strcat(canonicalPath, ioPath);
        }
    }
    if (canonicalPath)
        nsFileSpecHelpers::StringAssign(ioPath, canonicalPath);
} // nsFileSpecHelpers::Canonify

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::SetLeafName(const char* inLeafName)
//----------------------------------------------------------------------------------------
{
    nsFileSpecHelpers::LeafReplace(mPath, '/', inLeafName);
} // nsNativeFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsNativeFileSpec::GetLeafName() const
//----------------------------------------------------------------------------------------
{
    return nsFileSpecHelpers::GetLeaf(mPath, '/');
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
    struct stat st;
    return 0 == stat(mPath, &st) && S_ISREG(st.st_mode); 
} // nsNativeFileSpec::IsFile

//----------------------------------------------------------------------------------------
bool nsNativeFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
    struct stat st;
    return 0 == stat(mPath, &st) && S_ISDIR(st.st_mode); 
} // nsNativeFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::GetParent(nsNativeFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
    nsFileSpecHelpers::StringAssign(outSpec.mPath, mPath);
    char* cp = strrchr(outSpec.mPath, '/');
    if (cp)
        *cp = '\0';
} // nsNativeFileSpec::GetParent

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator += (const char* inRelativePath)
//----------------------------------------------------------------------------------------
{
    if (!inRelativePath || !mPath)
        return;
    
    if (mPath[strlen(mPath) - 1] != '/')
        char* newPath = nsFileSpecHelpers::ReallocCat(mPath, "/");
    SetLeafName(inRelativePath);
} // nsNativeFileSpec::operator +=

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::CreateDirectory(int mode)
//----------------------------------------------------------------------------------------
{
    // Note that mPath is canonical!
    mkdir(mPath, mode);
} // nsNativeFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::Delete(bool inRecursive)
// To check if this worked, call Exists() afterwards, see?
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
        rmdir(mPath);
    }
    else
        remove(mPath);
} // nsNativeFileSpec::Delete

//========================================================================================
//                                nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(
    const nsNativeFileSpec& inDirectory
,    int inIterateDirection)
//----------------------------------------------------------------------------------------
    : mCurrent(inDirectory)
    , mDir(nsnull)
    , mExists(false)
{
    mCurrent += "sysygy"; // prepare the path for SetLeafName
    mDir = opendir((const char*)nsFilePath(inDirectory));
    ++(*this);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator::~nsDirectoryIterator()
//----------------------------------------------------------------------------------------
{
    if (mDir)
        closedir(mDir);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator ++ ()
//----------------------------------------------------------------------------------------
{
    mExists = false;
    if (!mDir)
        return *this;
    char* dot    = ".";
    char* dotdot = "..";
    struct dirent* entry = readdir(mDir);
    if (entry && strcmp(entry->d_name, dot) == 0)
        entry = readdir(mDir);
    if (entry && strcmp(entry->d_name, dotdot) == 0)
        entry = readdir(mDir);
    if (entry)
    {
        mExists = true;
        mCurrent.SetLeafName(entry->d_name);
    }
    return *this;
} // nsDirectoryIterator::operator ++

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator -- ()
//----------------------------------------------------------------------------------------
{
    return ++(*this); // can't do it backwards.
} // nsDirectoryIterator::operator --
