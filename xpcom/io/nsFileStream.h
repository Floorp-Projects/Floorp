/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0(the "NPL"); you may not use this file except in
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
 * Copyright(C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//    First checked in on 98/11/20 by John R. McMullen in the wrong directory.
//    Checked in again 98/12/04.
//    Polished version 98/12/08.

//========================================================================================
//
//  Classes defined:
//
//      single-byte char:
//
//          nsInputFileStream, nsOutputFileStream
//
//  This suite provide the following services:
//
//      1.  Encapsulates all platform-specific file details, so that file i/o
//          can be done correctly without any platform #ifdefs
//
//      2.  Uses NSPR file services (NOT ansi file I/O), in order to get best
//          native performance.  This performance difference is especially large on
//          macintosh.
//
//      3.  Allows all the power of the ansi stream syntax.
//
//          Basic example:
//
//              nsFilePath myPath("/Development/iotest.txt");
//
//	            nsOutputFileStream testStream(myPath);
//				testStream << "Hello World" << nsEndl;
//
//      4.  Requires streams to be constructed using typesafe nsFilePath specifier
//          (not the notorious and bug prone const char*), namely nsFilePath.  See
//          nsFileSpec.h for more details.
//
//      5.  Fixes a bug that have been there for a long time, and
//          is inevitable if you use NSPR alone:
//
//               The problem on platforms (Macintosh) in which a path does not fully
//               specify a file, because two volumes can have the same name.
//
//  Not yet provided:
//
//      Endian-awareness for reading and writing crossplatform binary files.  At this
//      time there seems to be no demand for this.
//
//========================================================================================

#ifndef _FILESTREAM_H_
#define _FILESTREAM_H_

#include "nscore.h"

#ifdef XP_MAC
#include "pprio.h" // To get PR_ImportFile
#else
#include "prio.h"
#endif
#include "nsFileSpec.h"

//========================================================================================
//                          Compiler-specific macros, as needed
//========================================================================================
#if !defined(NS_USING_NAMESPACE) && (defined(__MWERKS__) || defined(XP_PC))
#define NS_USING_NAMESPACE
#endif

#if !defined(NS_USING_STL) && (defined(__MWERKS__) || defined(XP_PC))
#define NS_USING_STL
#endif

#ifdef NS_USING_NAMESPACE

#define NS_NAMESPACE_PROTOTYPE
#define NS_NAMESPACE namespace
#define NS_NAMESPACE_END
	
#else

#define NS_NAMESPACE_PROTOTYPE static
#define NS_NAMESPACE struct
#define NS_NAMESPACE_END ;

#endif // NS_USING_NAMESPACE

#ifdef NS_USING_STL
#include <iostream>
using std::istream;
using std::ostream;
#else
#include <iostream.h>
#endif

//=========================== End Compiler-specific macros ===============================

//========================================================================================
class NS_BASE nsBasicFileStream
//========================================================================================
{
public:
                                      nsBasicFileStream();
                                      nsBasicFileStream(PRFileDesc* desc, int nsprMode);
                                      nsBasicFileStream(
                                          const nsFilePath& inFile,
                                          int nsprMode,
                                          PRIntn accessMode);
    virtual                           ~nsBasicFileStream();


    inline bool                       is_open() const { return mFileDesc != 0; }
    void                              open(
                                           const nsFilePath& inFile,
                                           int nsprMode,
                                           PRIntn accessMode);
    void                              close();
    PRIntn                            tell() const;
    void                              seek(PRInt32 offset) { seek(PR_SEEK_SET, offset); }
    void                              seek(PRSeekWhence whence, PRInt32 offset);
    bool                              eof() const { return mEOF; }
    bool                              failed() const { return mFailed; }
                                          // call PR_GetError() for details
    
protected:
    PRFileDesc*                       mFileDesc;
    int                               mNSPRMode;
    bool                              mFailed;
    bool                              mEOF;
}; // class nsBasicFileStream

//========================================================================================
class NS_BASE nsInputFileStream
//========================================================================================
:	public nsBasicFileStream
{
public:
	enum  { kDefaultMode = PR_RDONLY };
                                      nsInputFileStream()
                                      : nsBasicFileStream()
                                      , mStdStream(0) {}
                                      nsInputFileStream(
                                          istream& stream);
                                      nsInputFileStream(
                                          const nsFilePath& inFile,
                                          int nsprMode = kDefaultMode,
                                          PRIntn accessMode = 0x00400)
                                      : nsBasicFileStream(inFile, nsprMode, accessMode)
                                      , mStdStream(0) {}

    nsInputFileStream&                operator >> (nsInputFileStream& (*pf)(nsInputFileStream&))
                                      {
                                           return pf(*this);
                                       }
    void                              get(char& c);
    PRInt32                           read(void* s, PRInt32 n);
    bool                              readline(char* s,  PRInt32 n);
                                          // Result always null-terminated
                                          // false result indicates line was truncated
                                          // to fit buffer, or an error occurred.

    // Output streamers.  Add more as needed
    nsInputFileStream&                operator >> (char& ch);
    
    void                              open(
                                           const nsFilePath& inFile,
                                           int nsprMode = kDefaultMode,
                                           PRIntn accessMode = 0x00400)
                                      {
                                          nsBasicFileStream::open(inFile, nsprMode, accessMode);
                                      }
private:

    nsInputFileStream&                operator >> (char* buf); // TOO DANGEROUS. DON'T DEFINE.

protected:

	istream*                          mStdStream;
}; // class nsInputFileStream

//========================================================================================
class NS_BASE nsOutputFileStream
//========================================================================================
:	public nsBasicFileStream
{
public:
	enum  { kDefaultMode = (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE) };

                                      nsOutputFileStream()
                                      : nsBasicFileStream()
                                      , mStdStream(0) {}
                                      nsOutputFileStream(
                                          ostream& stream);
                                      nsOutputFileStream(
                                           const nsFilePath& inFile,
                                           int nsprMode = kDefaultMode,
                                           PRIntn accessMode = 0x00200)
                                      : nsBasicFileStream(inFile, nsprMode, accessMode)
                                      , mStdStream(0) {}
 
    ostream*                          GetStandardStream() const { return mStdStream; }
    inline void                       open(
                                          const nsFilePath& inFile,
                                          int nsprMode = kDefaultMode,
                                          PRIntn accessMode = 0x00200)
                                      {
                                          nsBasicFileStream::open(inFile, nsprMode, accessMode);
                                      }
    nsOutputFileStream&               operator << (nsOutputFileStream& (*pf)(nsOutputFileStream&))
                                      {
                                           return pf(*this);
                                      }
    void                              put(char c);
    PRInt32                           write(const void* s, PRInt32 n);
    void                              flush();
    
    // Output streamers.  Add more as needed
    nsOutputFileStream&               operator << (const char* buf);
    nsOutputFileStream&               operator << (char ch);
    nsOutputFileStream&               operator << (short val);
    nsOutputFileStream&               operator << (unsigned short val);
    nsOutputFileStream&               operator << (long val);
    nsOutputFileStream&               operator << (unsigned long val);

protected:
	ostream*                          mStdStream;
}; // class nsOutputFileStream

//========================================================================================
//        Manipulators
//========================================================================================
NS_BASE nsOutputFileStream& nsEndl(nsOutputFileStream& os);
 

#endif /* _FILESTREAM_H_ */

