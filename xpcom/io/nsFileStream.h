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
class nsIFileSpec;

//========================================================================================
//                          Compiler-specific macros, as needed
//========================================================================================
#if !defined(NS_USING_NAMESPACE) && (defined(__MWERKS__) || defined(XP_WIN))
#define NS_USING_NAMESPACE
#endif

#if !defined(NS_USING_STL) && (defined(__MWERKS__) || defined(XP_WIN))
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

#if !defined(XP_MAC) && !defined(__KCC)
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
class NS_COM nsInputStream
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
    nsresult                          close()
                                      {
    					NS_ASSERTION(mInputStream, "mInputStream is null!");
					if (mInputStream) {
						return mInputStream->Close();                        
					}
                    return NS_OK;
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

    // private and unimplemented to disallow copies and assigns
                                      nsInputStream(const nsInputStream& rhs);
    nsInputStream&                    operator=(const nsInputStream& rhs);

// DATA
protected:
    nsCOMPtr<nsIInputStream>          mInputStream;
    PRBool                            mEOF;
}; // class nsInputStream

typedef nsInputStream nsBasicInStream; // historic support for this name

//========================================================================================
class NS_COM nsOutputStream
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
    nsresult                          close()
                                      {
                                          if (mOutputStream)
                                            return mOutputStream->Close();
                                          return NS_OK;
                                      }
    void                              put(char c);
    PRInt32                           write(const void* s, PRInt32 n);
    virtual nsresult                  flush();

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

private:

    // private and unimplemented to disallow copies and assigns
                                      nsOutputStream(const nsOutputStream& rhs);
    nsOutputStream&                   operator=(const nsOutputStream& rhs);

// DATA
protected:
    nsCOMPtr<nsIOutputStream>          mOutputStream;
}; // class nsOutputStream

typedef nsOutputStream nsBasicOutStream; // Historic support for this name

//========================================================================================
class NS_COM nsErrorProne
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
    nsresult                          error() const
                                      {
                                          return mResult;
                                      }

// DATA
protected:
    nsresult                          mResult;
}; // class nsErrorProne

//========================================================================================
class NS_COM nsFileClient
// Because COM does not allow us to write functions which return a boolean value etc,
// this class is here to take care of the tedious "declare variable then call with
// the address of the variable" chores.
//========================================================================================
:    public virtual nsErrorProne
{
public:
                                      nsFileClient(const nsCOMPtr<nsIOpenFile>& inFile)
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
    nsCOMPtr<nsIOpenFile>                 mFile;
}; // class nsFileClient

//========================================================================================
class NS_COM nsRandomAccessStoreClient
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

private:

    // private and unimplemented to disallow copies and assigns
                                      nsRandomAccessStoreClient(const nsRandomAccessStoreClient& rhs);
    nsRandomAccessStoreClient&        operator=(const nsRandomAccessStoreClient& rhs);

// DATA
protected:
    nsCOMPtr<nsIRandomAccessStore>    mStore;
}; // class nsRandomAccessStoreClient

//========================================================================================
class NS_COM nsRandomAccessInputStream
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

private:

    // private and unimplemented to disallow copies and assigns
                                      nsRandomAccessInputStream(const nsRandomAccessInputStream& rhs);
    nsRandomAccessInputStream&        operator=(const nsRandomAccessInputStream& rhs);

}; // class nsRandomAccessInputStream

//========================================================================================
class NS_COM nsInputStringStream
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


private:

    // private and unimplemented to disallow copies and assigns
                                      nsInputStringStream(const nsInputStringStream& rhs);
    nsInputStringStream&              operator=(const nsInputStringStream& rhs);


}; // class nsInputStringStream

//========================================================================================
class NS_COM nsInputFileStream
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
                                          PRIntn accessMode = 00666);
                                      nsInputFileStream(nsIFileSpec* inFile);
    virtual                           ~nsInputFileStream();

    void                              Open(
                                          const nsFileSpec& inFile,
                                          int nsprMode = kDefaultMode,
                                          PRIntn accessMode = 00666)
                                      {
                                          if (mFile)
                                              mFile->Open(inFile, nsprMode, accessMode);
                                      }

    // Input streamers.  Unfortunately, they don't inherit!
    nsInputStream&                    operator >> (char& ch)
                                         { return nsInputStream::operator >>(ch); }
    nsInputStream&                    operator >> (nsInputStream& (*pf)(nsInputStream&))
                                         { return nsInputStream::operator >>(pf); }

protected:
    void                              AssignFrom(nsISupports* stream);

private:

    // private and unimplemented to disallow copies and assigns
                                      nsInputFileStream(const nsInputFileStream& rhs);
    nsInputFileStream&                operator=(const nsInputFileStream& rhs);

// DATA
protected:
    nsCOMPtr<nsIFileSpecInputStream>      mFileInputStream;
}; // class nsInputFileStream

//========================================================================================
class NS_COM nsRandomAccessOutputStream
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

private:

    // private and unimplemented to disallow copies and assigns
                                      nsRandomAccessOutputStream(const nsRandomAccessOutputStream& rhs);
    nsRandomAccessOutputStream&       operator=(const nsRandomAccessOutputStream& rhs);

}; // class nsRandomAccessOutputStream

//========================================================================================
class NS_COM nsOutputStringStream
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

private:

    // private and unimplemented to disallow copies and assigns
                                      nsOutputStringStream(const nsOutputStringStream& rhs);
    nsOutputStringStream&             operator=(const nsOutputStringStream& rhs);

}; // class nsOutputStringStream

//========================================================================================
class NS_COM nsOutputFileStream
// Please read the comments at the top of this file
//========================================================================================
:	public nsRandomAccessOutputStream
,	public nsFileClient
{
public:
	enum  { kDefaultMode = (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE) };

                                      nsOutputFileStream() {}
                                      nsOutputFileStream(nsIOutputStream* inStream)
                                      {
                                          AssignFrom(inStream);
                                      }
                                      nsOutputFileStream(
                                           const nsFileSpec& inFile,
                                           int nsprMode = kDefaultMode,
                                           PRIntn accessMode = 00666) 
                                      {
                                          nsISupports* stream;
                                          if (NS_FAILED(NS_NewIOFileStream(
                                              &stream,
                                              inFile, nsprMode, accessMode)))
                                              return;
                                          AssignFrom(stream);
                                          NS_RELEASE(stream);
                                      }
                                      nsOutputFileStream(nsIFileSpec* inFile);
    virtual                           ~nsOutputFileStream();
 
    virtual nsresult                  flush();
    virtual void					  abort();

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
    void                              AssignFrom(nsISupports* stream);

private:
    
    // private and unimplemented to disallow copies and assigns
                                      nsOutputFileStream(const nsOutputFileStream& rhs);
    nsOutputFileStream&               operator=(const nsOutputFileStream& rhs);

// DATA
protected:
    nsCOMPtr<nsIFileSpecOutputStream>     mFileOutputStream;
}; // class nsOutputFileStream

//========================================================================================
class NS_COM nsOutputConsoleStream
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
                                          mFile = do_QueryInterface(stream);
                                          mOutputStream = do_QueryInterface(stream);
                                          mFileOutputStream = do_QueryInterface(stream);
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


private:

    // private and unimplemented to disallow copies and assigns
                                      nsOutputConsoleStream(const nsOutputConsoleStream& rhs);
    nsOutputConsoleStream&            operator=(const nsOutputConsoleStream& rhs);


}; // class nsOutputConsoleStream

//========================================================================================
class NS_COM nsIOFileStream
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
                                           PRIntn accessMode = 00666) 
                                      :  nsInputFileStream((nsIInputStream*)nsnull)
                                      ,  nsOutputStream(nsnull)
                                      {
                                          nsISupports* stream;
                                          if (NS_FAILED(NS_NewIOFileStream(
                                              &stream,
                                              inFile, nsprMode, accessMode)))
                                              return;
                                          mFile = do_QueryInterface(stream);
                                          mStore = do_QueryInterface(stream);
                                          mInputStream = do_QueryInterface(stream);
                                          mOutputStream = do_QueryInterface(stream);
                                          mFileInputStream = do_QueryInterface(stream);
                                          mFileOutputStream = do_QueryInterface(stream);
                                          NS_RELEASE(stream);
                                      }
 
    virtual nsresult                  close()
                                      {
                                          // Doesn't matter which of the two we close:
                                          // they're hooked up to the same file.
                                          return nsInputFileStream::close();
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

	virtual nsresult flush() {if (mFileOutputStream) mFileOutputStream->Flush(); return error(); }


private:

    // private and unimplemented to disallow copies and assigns
                                      nsIOFileStream(const nsIOFileStream& rhs);
    nsIOFileStream&                   operator=(const nsIOFileStream& rhs);

    // DATA
protected:
    nsCOMPtr<nsIFileSpecOutputStream>     mFileOutputStream;
}; // class nsIOFileStream
 
//========================================================================================
//        Manipulators
//========================================================================================

NS_COM nsOutputStream&     nsEndl(nsOutputStream& os); // outputs and FLUSHES.

//========================================================================================
class NS_COM nsSaveViaTempStream
// The interface looks like a stream class, but in fact it streams to a temp file, and
// finally renames if the output succeeded.
//========================================================================================
 : public nsOutputFileStream
{
private:

	typedef nsOutputFileStream Inherited;

public:
									nsSaveViaTempStream(const nsFileSpec& inFileToSave);
									~nsSaveViaTempStream();
	virtual void					close();

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

private:

    // private and unimplemented to disallow copies and assigns
                                      nsSaveViaTempStream(const nsSaveViaTempStream& rhs);
    nsSaveViaTempStream&              operator=(const nsSaveViaTempStream& rhs);


protected:

	const nsFileSpec&					mFileToSave;
	nsFileSpec*							mTempFileSpec;
}; // class nsSaveViaTempStream
 
#endif /* _FILESTREAM_H_ */
