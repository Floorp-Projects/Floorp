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
//    Completely rewritten to integrate with nsIInputStream and nsIOutputStream (the
//        xpcom stream objects.

//========================================================================================
//
//  Classes defined:
//
//          nsInputStream, nsOutputStream
//                These are the lightweight STATICALLY LINKED wrappers for
//                the xpcom objects nsIInputStream and nsIOutputstream.
//             Possible uses:
//                If you are implementing a function that accepts one of these xpcom
//                streams, just make one of these little jobbies on the stack, and
//                the handy << or >> notation can be yours.
//                
//          nsInputFileStream, nsOutputFileStream
//                These are the STATICALLY LINKED wrappers for the file-related
//                versions of the above.
//          nsIOFileStream
//                An input and output file stream attached to the same file.
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
//              nsFileSpec myPath("/Development/iotest.txt");
//
//	            nsOutputFileStream testStream(myPath);
//				testStream << "Hello World" << nsEndl;
//
//      4.  Requires streams to be constructed using typesafe nsFileSpec specifier
//          (not the notorious and bug prone const char*), namely nsFileSpec.  See
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

#include "nsCOMPtr.h"
#include "nsIFileStream.h"

// Defined elsewhere
class nsFileSpec;
class nsString;
class nsIInputStream;
class nsIOutputStream;

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

#ifndef XP_MAC
// PR_STDOUT and PR_STDIN are fatal on Macintosh.  So for console i/o, we must use the std
// stream stuff instead.  However, we have to require that cout and cin are passed in
// to the constructor because in the current build, there is a copy in the base.shlb,
// and another in the caller's file.  Passing it in as a parameter ensures that the
// caller and this library are using the same global object.  Groan.
//
// Unix currently does not support iostreams at all.  Their compilers do not support
// ANSI C++, or even ARM C++.
//
// Windows supports them, but only if you turn on the -GX compile flag, to support
// exceptions.

// Catch 22.
#define NS_USE_PR_STDIO
#endif

#ifdef NS_USE_PR_STDIO
class istream;
class ostream;
#define CONSOLE_IN 0
#define CONSOLE_OUT 0
#else
#include <iostream>
using std::istream;
using std::ostream;
#define CONSOLE_IN &std::cin
#define CONSOLE_OUT &std::cout
#endif

//=========================== End Compiler-specific macros ===============================

//========================================================================================
class NS_BASE nsInputStream
// This is a convenience class, for use on the STACK ("new" junkies: get detoxed first).
// Given a COM-style stream, this allows you to use the >> operators.  It also acquires and
// reference counts its stream.
// Please read the comments at the top of this file
//========================================================================================
{
public:
                                      nsInputStream(nsIInputStream* inStream)
                                      :   mInputStream(do_QueryInterface(inStream))
                                      ,   mEOF(PR_FALSE)
                                      {}
    virtual                           ~nsInputStream();
 
    nsCOMPtr<nsIInputStream>          GetIStream() const
                                      {
                                          return mInputStream;
                                      }
    PRBool                            eof() const { return get_at_eof(); }
    char                              get();
    void                              close()
                                      {
                                          mInputStream->Close();
                                      }
    PRInt32                           read(void* s, PRInt32 n);

    // Input streamers.  Add more as needed (int&, unsigned int& etc). (but you have to
    // add delegators to the derived classes, too, because these operators don't inherit).
    nsInputStream&                    operator >> (char& ch);

    // Support manipulators
    nsInputStream&                    operator >> (nsInputStream& (*pf)(nsInputStream&))
                                      {
                                           return pf(*this);
                                      }

protected:

   // These certainly need to be overridden, they give the best shot we can at detecting
   // eof in a simple nsIInputStream.
   virtual void                       set_at_eof(PRBool atEnd)
                                      {
                                         mEOF = atEnd;
                                      }
   virtual PRBool                     get_at_eof() const
                                      {
                                          return mEOF;
                                      }
private:

    nsInputStream&                    operator >> (char* buf); // TOO DANGEROUS. DON'T DEFINE.

// DATA
protected:
    nsCOMPtr<nsIInputStream>          mInputStream;
    PRBool                            mEOF;
}; // class nsInputStream

typedef nsInputStream nsBasicInStream; // historic support for this name

//========================================================================================
class NS_BASE nsOutputStream
// This is a convenience class, for use on the STACK ("new" junkies, get detoxed first).
// Given a COM-style stream, this allows you to use the << operators.  It also acquires and
// reference counts its stream.
// Please read the comments at the top of this file
//========================================================================================
{
public:
                                      nsOutputStream() {}
                                      nsOutputStream(nsIOutputStream* inStream)
                                      :   mOutputStream(do_QueryInterface(inStream))
                                          {}

    virtual                          ~nsOutputStream();

    nsCOMPtr<nsIOutputStream>         GetIStream() const
                                      {
                                          return mOutputStream;
                                      }
    void                              close()
                                      {
                                          mOutputStream->Close();
                                      }
    void                              put(char c);
    PRInt32                           write(const void* s, PRInt32 n);
    virtual void                      flush();

    // Output streamers.  Add more as needed (but you have to add delegators to the derived
    // classes, too, because these operators don't inherit).
    nsOutputStream&                   operator << (const char* buf);
    nsOutputStream&                   operator << (char ch);
    nsOutputStream&                   operator << (short val);
    nsOutputStream&                   operator << (unsigned short val);
    nsOutputStream&                   operator << (long val);
    nsOutputStream&                   operator << (unsigned long val);
    nsOutputStream&                   operator << (int val);
    nsOutputStream&                   operator << (unsigned int val);

    // Support manipulators
    nsOutputStream&                   operator << (nsOutputStream& (*pf)(nsOutputStream&))
                                      {
                                           return pf(*this);
                                      }

// DATA
protected:
    nsCOMPtr<nsIOutputStream>          mOutputStream;
}; // class nsOutputStream

typedef nsOutputStream nsBasicOutStream; // Historic support for this name

//========================================================================================
class NS_BASE nsErrorProne
// Common (virtual) base class for remembering errors on demand
//========================================================================================
{
public:
                                      nsErrorProne() // for delayed opening
                                      :   mResult(NS_OK)
                                      {
                                      }
    PRBool                            failed() const
                                      {
                                          return NS_FAILED(mResult);
                                      }

// DATA
protected:
    nsresult                          mResult;
}; // class nsErrorProne

//========================================================================================
class NS_BASE nsFileClient
// Because COM does not allow us to write functions which return a boolean value etc,
// this class is here to take care of the tedious "declare variable then call with
// the address of the variable" chores.
//========================================================================================
:    public virtual nsErrorProne
{
public:
                                      nsFileClient(const nsCOMPtr<nsIFile>& inFile)
                                      :   mFile(do_QueryInterface(inFile))
                                      {
                                      }
    virtual                           ~nsFileClient() {}

    void                              open(
                                          const nsFileSpec& inFile,
                                          int nsprMode,
                                          PRIntn accessMode)
                                      {
                                          if (mFile)
                                              mResult = mFile->Open(inFile, nsprMode, accessMode);
                                      }
    PRBool                            is_open() const
                                      {
                                          PRBool result = PR_FALSE;
                                          if (mFile)
                                              mFile->GetIsOpen(&result);
                                          return result;
                                      }
    PRBool                            is_file() const
                                      {
                                          return mFile ? PR_TRUE : PR_FALSE;
                                      }

protected:

                                      nsFileClient() // for delayed opening
                                      {
                                      }
// DATA
protected:
    nsCOMPtr<nsIFile>                 mFile;
}; // class nsFileClient

//========================================================================================
class NS_BASE nsRandomAccessStoreClient
// Because COM does not allow us to write functions which return a boolean value etc,
// this class is here to take care of the tedious "declare variable then call with
// the address of the variable" chores.
//========================================================================================
:    public virtual nsErrorProne
{
public:
                                      nsRandomAccessStoreClient() // for delayed opening
                                      {
                                      }
                                      nsRandomAccessStoreClient(const nsCOMPtr<nsIRandomAccessStore>& inStore)
                                      :   mStore(do_QueryInterface(inStore))
                                      {
                                      }
    virtual                           ~nsRandomAccessStoreClient() {}
    
    void                              seek(PRInt32 offset)
                                      {
                                          seek(PR_SEEK_SET, offset);
                                      }

    void                              seek(PRSeekWhence whence, PRInt32 offset)
                                      {
                                          set_at_eof(PR_FALSE);
                                          if (mStore)
                                              mResult = mStore->Seek(whence, offset);
                                      }
    PRIntn                            tell()
                                      {
                                          PRIntn result = -1;
                                          if (mStore)
                                              mResult = mStore->Tell(&result);
                                          return result;
                                      }

protected:

   virtual PRBool                     get_at_eof() const
                                      {
                                          PRBool result = PR_TRUE;
                                          if (mStore)
                                              mStore->GetAtEOF(&result);
                                          return result;
                                      }

   virtual void                       set_at_eof(PRBool atEnd)
                                      {
                                          if (mStore)
                                              mStore->SetAtEOF(atEnd);
                                      }

// DATA
protected:
    nsCOMPtr<nsIRandomAccessStore>    mStore;
}; // class nsRandomAccessStoreClient

//========================================================================================
class NS_BASE nsRandomAccessInputStream
// Please read the comments at the top of this file
//========================================================================================
:	public nsRandomAccessStoreClient
,	public nsInputStream
{
public:
                                      nsRandomAccessInputStream(nsIInputStream* inStream)
                                      :   nsRandomAccessStoreClient(do_QueryInterface(inStream))
                                      ,   nsInputStream(inStream)
                                      {
                                      }
    PRBool                            readline(char* s,  PRInt32 n);
                                          // Result always null-terminated.
                                          // Check eof() before each call.
                                          // CAUTION: false result only indicates line was truncated
                                          // to fit buffer, or an error occurred (OTHER THAN eof).

    // Input streamers.  Unfortunately, they don't inherit!
    nsInputStream&                    operator >> (char& ch)
                                         { return nsInputStream::operator >>(ch); }
    nsInputStream&                    operator >> (nsInputStream& (*pf)(nsInputStream&))
                                         { return nsInputStream::operator >>(pf); }

protected:
                                      nsRandomAccessInputStream()
                                      :  nsInputStream(nsnull)
                                      {
                                      }

   virtual PRBool                     get_at_eof() const
                                      {
                                          return nsRandomAccessStoreClient::get_at_eof();
                                      }

   virtual void                       set_at_eof(PRBool atEnd)
                                      {
                                          nsRandomAccessStoreClient::set_at_eof(atEnd);
                                      }

}; // class nsRandomAccessInputStream

//========================================================================================
class NS_BASE nsInputStringStream
//========================================================================================
: public nsRandomAccessInputStream
{
public:
                                      nsInputStringStream(const char* stringToRead);
                                      nsInputStringStream(const nsString& stringToRead);

    // Input streamers.  Unfortunately, they don't inherit!
    nsInputStream&                    operator >> (char& ch)
                                         { return nsInputStream::operator >>(ch); }
    nsInputStream&                    operator >> (nsInputStream& (*pf)(nsInputStream&))
                                         { return nsInputStream::operator >>(pf); }

}; // class nsInputStringStream

//========================================================================================
class NS_BASE nsInputFileStream
// Please read the comments at the top of this file
//========================================================================================
:	public nsRandomAccessInputStream
,   public nsFileClient
{
public:
	enum  { kDefaultMode = PR_RDONLY };
                                      nsInputFileStream(nsIInputStream* inStream)
                                      :   nsRandomAccessInputStream(inStream)
                                      ,   nsFileClient(do_QueryInterface(inStream))
                                      ,   mFileInputStream(do_QueryInterface(inStream))
                                      {
                                      }
                                      nsInputFileStream(
                                          const nsFileSpec& inFile,
                                          int nsprMode = kDefaultMode,
                                          PRIntn accessMode = 00700); // <- OCTAL

    void                              Open(
                                          const nsFileSpec& inFile,
                                          int nsprMode = kDefaultMode,
                                          PRIntn accessMode = 00700) // <- OCTAL
                                      {
                                          if (mFile)
                                              mFile->Open(inFile, nsprMode, accessMode);
                                      }

    // Input streamers.  Unfortunately, they don't inherit!
    nsInputStream&                    operator >> (char& ch)
                                         { return nsInputStream::operator >>(ch); }
    nsInputStream&                    operator >> (nsInputStream& (*pf)(nsInputStream&))
                                         { return nsInputStream::operator >>(pf); }

// DATA
protected:
    nsCOMPtr<nsIFileInputStream>      mFileInputStream;
}; // class nsInputFileStream

//========================================================================================
class NS_BASE nsRandomAccessOutputStream
// Please read the comments at the top of this file
//========================================================================================
:	public nsRandomAccessStoreClient
,	public nsOutputStream
{
public:
                                      nsRandomAccessOutputStream(nsIOutputStream* inStream)
                                      :   nsRandomAccessStoreClient(do_QueryInterface(inStream))
                                      ,   nsOutputStream(inStream)
                                      {
                                      }

    // Output streamers.  Unfortunately, they don't inherit!
    nsOutputStream&                   operator << (const char* buf)
                                        { return nsOutputStream::operator << (buf); }
    nsOutputStream&                   operator << (char ch)
                                        { return nsOutputStream::operator << (ch); }
    nsOutputStream&                   operator << (short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (nsOutputStream& (*pf)(nsOutputStream&))
                                        { return nsOutputStream::operator << (pf); }

protected:
                                      nsRandomAccessOutputStream()
                                      :  nsOutputStream(nsnull)
                                      {
                                      }
}; // class nsRandomAccessOutputStream

//========================================================================================
class NS_BASE nsOutputStringStream
//========================================================================================
: public nsRandomAccessOutputStream
{
public:
                                      nsOutputStringStream(char*& stringToChange);
                                      nsOutputStringStream(nsString& stringToChange);

    // Output streamers.  Unfortunately, they don't inherit!
    nsOutputStream&                   operator << (const char* buf)
                                        { return nsOutputStream::operator << (buf); }
    nsOutputStream&                   operator << (char ch)
                                        { return nsOutputStream::operator << (ch); }
    nsOutputStream&                   operator << (short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (nsOutputStream& (*pf)(nsOutputStream&))
                                        { return nsOutputStream::operator << (pf); }

}; // class nsOutputStringStream

//========================================================================================
class NS_BASE nsOutputFileStream
// Please read the comments at the top of this file
//========================================================================================
:	public nsRandomAccessOutputStream
,	public nsFileClient
{
public:
	enum  { kDefaultMode = (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE) };

                                      nsOutputFileStream() {}
                                      nsOutputFileStream(
                                           const nsFileSpec& inFile,
                                           int nsprMode = kDefaultMode,
                                           PRIntn accessMode = 00700) // <- OCTAL
                                       {
                                          nsISupports* stream;
                                          if (NS_FAILED(NS_NewIOFileStream(
                                              &stream,
                                              inFile, nsprMode, accessMode)))
                                              return;
                                          mFile = nsQueryInterface(stream);
                                          mOutputStream = nsQueryInterface(stream);
                                          mStore = nsQueryInterface(stream);
                                          mFileOutputStream = nsQueryInterface(stream);
                                          NS_RELEASE(stream);
                                      }
 
    virtual void                      flush();

    // Output streamers.  Unfortunately, they don't inherit!
    nsOutputStream&                   operator << (const char* buf)
                                        { return nsOutputStream::operator << (buf); }
    nsOutputStream&                   operator << (char ch)
                                        { return nsOutputStream::operator << (ch); }
    nsOutputStream&                   operator << (short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (nsOutputStream& (*pf)(nsOutputStream&))
                                        { return nsOutputStream::operator << (pf); }

// DATA
protected:
    nsCOMPtr<nsIFileOutputStream>     mFileOutputStream;
}; // class nsOutputFileStream

//========================================================================================
class NS_BASE nsOutputConsoleStream
// Please read the comments at the top of this file
//========================================================================================
:	public nsOutputFileStream
{
public:

                                      nsOutputConsoleStream()
                                      {
                                          nsISupports* stream;
                                          if (NS_FAILED(NS_NewOutputConsoleStream(&stream)))
                                              return;
                                          mFile = nsQueryInterface(stream);
                                          mOutputStream = nsQueryInterface(stream);
                                          mFileOutputStream = nsQueryInterface(stream);
                                          NS_RELEASE(stream);
                                      }

    // Output streamers.  Unfortunately, they don't inherit!
    nsOutputStream&                   operator << (const char* buf)
                                        { return nsOutputStream::operator << (buf); }
    nsOutputStream&                   operator << (char ch)
                                        { return nsOutputStream::operator << (ch); }
    nsOutputStream&                   operator << (short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (nsOutputStream& (*pf)(nsOutputStream&))
                                        { return nsOutputStream::operator << (pf); }

}; // class nsOutputConsoleStream

//========================================================================================
class NS_BASE nsIOFileStream
// Please read the comments at the top of this file
//========================================================================================
:	public nsInputFileStream
,	public nsOutputStream
{
public:
	enum  { kDefaultMode = (PR_RDWR | PR_CREATE_FILE) };

                                      nsIOFileStream(
                                          nsIInputStream* inInputStream
                                      ,   nsIOutputStream* inOutputStream)
                                      :   nsInputFileStream(inInputStream)
                                      ,   nsOutputStream(inOutputStream)
                                      ,   mFileOutputStream(do_QueryInterface(inOutputStream))
                                      {
                                      }
                                      nsIOFileStream(
                                           const nsFileSpec& inFile,
                                           int nsprMode = kDefaultMode,
                                           PRIntn accessMode = 00700) // <- OCTAL
                                      :  nsInputFileStream(nsnull)
                                      ,  nsOutputStream(nsnull)
                                      {
                                          nsISupports* stream;
                                          if (NS_FAILED(NS_NewIOFileStream(
                                              &stream,
                                              inFile, nsprMode, accessMode)))
                                              return;
                                          mFile = nsQueryInterface(stream);
                                          mStore = nsQueryInterface(stream);
                                          mInputStream = nsQueryInterface(stream);
                                          mOutputStream = nsQueryInterface(stream);
                                          mFileInputStream = nsQueryInterface(stream);
                                          mFileOutputStream = nsQueryInterface(stream);
                                          NS_RELEASE(stream);
                                      }
 
    virtual void                      close()
                                      {
                                          // Doesn't matter which of the two we close:
                                          // they're hooked up to the same file.
                                          nsInputFileStream::close();
                                      }

     // Output streamers.  Unfortunately, they don't inherit!
    nsOutputStream&                   operator << (const char* buf)
                                        { return nsOutputStream::operator << (buf); }
    nsOutputStream&                   operator << (char ch)
                                        { return nsOutputStream::operator << (ch); }
    nsOutputStream&                   operator << (short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned short val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned long val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (unsigned int val)
                                        { return nsOutputStream::operator << (val); }
    nsOutputStream&                   operator << (nsOutputStream& (*pf)(nsOutputStream&))
                                        { return nsOutputStream::operator << (pf); }

    // Input streamers.  Unfortunately, they don't inherit!
    nsInputStream&                    operator >> (char& ch)
                                         { return nsInputStream::operator >>(ch); }
    nsInputStream&                    operator >> (nsInputStream& (*pf)(nsInputStream&))
                                         { return nsInputStream::operator >>(pf); }

    // DATA
protected:
    nsCOMPtr<nsIFileOutputStream>     mFileOutputStream;
}; // class nsIOFileStream
 
//========================================================================================
//        Manipulators
//========================================================================================

NS_BASE nsOutputStream&     nsEndl(nsOutputStream& os); // outputs and FLUSHES.


#endif /* _FILESTREAM_H_ */
