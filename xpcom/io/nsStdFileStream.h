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

#error "Do not use this file.  The unix compilers do not support standard C++.  Use nsFileStream"

//    First checked in on 98/11/20 by John R. McMullen in the wrong directory.
//    Checked in again 98/12/04.
//    Polished version 98/12/08.

//========================================================================================
//
//  Classes defined:
//
//      single-byte char:
//
//          nsInputFileStream, nsOutputFileStream, nsIOFileStream
//
//      wide char:
//
//          nsWideInputFileStream, nsWideOutputFileStream, nsWideIOFileStream
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
//      3.  Allows all the power of the ansi stream syntax: these streams
//          ARE derived classes of ostream, istream, and iostream.  ALL METHODS OF
//          istream, ostream, AND iostream ARE AVAILABLE!
//
//          Basic example:
//
//              nsFilePath myPath("/Development/iotest.txt");
//
//	            nsOutputFileStream testStream(myPath);
//				testStream << "Hello World" << endl;
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

#ifdef NS_USING_STL

// Macintosh and Windows use this section.
//
// Here's where the party is.  When Unix wants to join in (by supporting
// a build system with STL headers), what fun we'll have!  Meanwhile, I've used
// macros to make this build on all our platforms.  Unix doesn't have support for
// STL, and therefore we could not use the template forms of these classes on Unix.
// (it's a long story).  Therefore, Unix supports no stream char types except 1-byte
// characters, and therefore nobody else does now either, until Unix catches up.


#define DEFINING_FILE_STREAM // templateers define this when this file is included.
#define IOS_BASE ios_base
#include <istream>
	using std::ios_base;
	using std::basic_streambuf;
	using std::codecvt_base;
	using std::codecvt;
	using std::streamsize;
	using std::locale;
	using std::basic_istream;
	using std::basic_ostream;
	using std::basic_iostream;
	using std::char_traits;
#define TEMPLATE_DEF template<class charT, class traits>
#define FILE_BUFFER_TYPE nsFileBufferT<charT, traits>
#define INPUT_FILE_STREAM nsInputFileStreamT<charT, traits>
#define OUTPUT_FILE_STREAM nsOutputFileStreamT<charT, traits>
#define IO_FILE_STREAM nsIOFileStreamT<charT, traits>
#define BASIC_STREAMBUF basic_streambuf<charT, traits>
#define BASIC_ISTREAM basic_istream<charT, traits>
#define BASIC_OSTREAM basic_ostream<charT, traits>
#define BASIC_IOSTREAM basic_iostream<charT, traits>
#define INT_TYPE FILE_BUFFER_TYPE::int_type
#define POS_TYPE FILE_BUFFER_TYPE::pos_type
#define OFF_TYPE FILE_BUFFER_TYPE::off_type
#define SEEK_DIR IOS_BASE::seekdir
#define EOF_VALUE traits::eof()

#else

// Unix uses this section until it supports STL.  This means no locales, no traits,
// no wide chars, etc.  Also, the stream classes are the original ARM-style ones,
// and are not templatized.

#define IOS_BASE ios
#include <istream.h>
#define TEMPLATE_DEF
#define FILE_BUFFER_TYPE nsFileBufferT
#define INPUT_FILE_STREAM nsInputFileStreamT
#define OUTPUT_FILE_STREAM nsOutputFileStreamT
#define IO_FILE_STREAM nsIOFileStreamT
#define BASIC_STREAMBUF streambuf
#define BASIC_ISTREAM istream
#define BASIC_OSTREAM ostream
#define BASIC_IOSTREAM iostream
#define INT_TYPE int
#define POS_TYPE long
#define OFF_TYPE long
#define SEEK_DIR ios::seek_dir
#define int_type int
#define pos_type long
#define off_type long
#define char_type char
#define EOF_VALUE EOF

#endif // NS_USING_STL

#ifdef __MWERKS__

#ifdef MSIPL_WCHART
#define NS_USING_WIDE_CHAR
#endif
#ifdef MSIPL_EXPLICIT_FUNC_TEMPLATE_ARG
#define NS_EXPLICIT_FUNC_TEMPLATE_ARG
#endif
#define NS_READ_LOCK(mut) READ_LOCK(mut)
#define NS_WRITE_LOCK(mut) WRITE_LOCK(mut)

#else

// Fix me, if necessary, for thread-safety.
#define NS_READ_LOCK(mut)
#define NS_WRITE_LOCK(mut)

#endif // __MWERKS__

//=========================== End Compiler-specific macros ===============================

//========================================================================================
NS_NAMESPACE nsFileStreamHelpers
// Prototypes for common (non-template) implementations in the .cpp file which do not
// need the template args (charT, traits).
//========================================================================================
{
	NS_NAMESPACE_PROTOTYPE NS_BASE PRFileDesc* open(
		const nsFilePath& inFile,
	    IOS_BASE::openmode mode,
	    PRIntn accessMode);
} NS_NAMESPACE_END // nsFileStreamHelpers

//========================================================================================
//	Template declarations
//========================================================================================

//========================================================================================
TEMPLATE_DEF
class nsFileBufferT
//========================================================================================
:    public BASIC_STREAMBUF
{
#ifdef NS_USING_STL
    typedef codecvt_base::result result;

public:
    typedef charT              char_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
    typedef typename traits::int_type int_type;
    typedef traits             traits_type;
    typedef typename traits::state_type state_type;

    typedef codecvt<charT, char, state_type> ofacet_type;
    typedef codecvt<char, charT, state_type> ifacet_type;
#endif

public:
                                       nsFileBufferT();
                                       nsFileBufferT(PRFileDesc* pfile_arg);
    virtual                            ~nsFileBufferT();
    bool                               is_open() const;
    FILE_BUFFER_TYPE*                  open(
                                           const nsFilePath& inFile,
                                           IOS_BASE::openmode mode,
                                           PRIntn accessMode);
    FILE_BUFFER_TYPE*                  close();
 
protected:
    virtual                            int_type overflow(int_type c=EOF_VALUE);
    virtual                            int_type pbackfail(int_type c=EOF_VALUE);
    virtual                            int_type underflow();
    virtual                            pos_type seekoff(
                                           off_type off, SEEK_DIR way, 
                                           IOS_BASE::openmode which=IOS_BASE::in|IOS_BASE::out);
    virtual                            pos_type seekpos(pos_type sp,
                                           IOS_BASE::openmode which=IOS_BASE::in|IOS_BASE::out);
    virtual                            BASIC_STREAMBUF* setbuf(char_type* s, streamsize n);
    virtual                            int sync();
    virtual                            int_type uflow();
#ifdef NS_USING_STL
    virtual                            void imbue(const locale& loc);
#endif
    virtual                            streamsize showmanyc();
    virtual                            streamsize xsgetn(char_type* s, streamsize n);
    virtual                            streamsize xsputn(const char_type* s, streamsize n);
 
private:
    PRFileDesc*                        mFileDesc;    
    IOS_BASE::openmode                 mode_;
}; // class nsFileBufferT

//========================================================================================
TEMPLATE_DEF
class nsInputFileStreamT
//========================================================================================
:    public BASIC_ISTREAM
{
#ifdef NS_USING_STL
public:
    typedef charT              char_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
    typedef typename traits::int_type int_type;
    typedef traits             traits_type;
#endif
 
public:
                                      nsInputFileStreamT();
                                      explicit nsInputFileStreamT(
                                          const nsFilePath& inFile,
                                          IOS_BASE::openmode mode=IOS_BASE::in,
                                          PRIntn accessMode = 0x00400);

    virtual                           ~nsInputFileStreamT();

    FILE_BUFFER_TYPE*                 rdbuf() const;
    inline bool                       is_open();
    inline void                       open(
                                           const nsFilePath& inFile,
                                           IOS_BASE::openmode mode=IOS_BASE::in,
                                           PRIntn accessMode = 0x00400);
    inline void                       close();

private:
    FILE_BUFFER_TYPE                  mBuffer;
}; // class nsInputFileStreamT

//========================================================================================
TEMPLATE_DEF
class nsOutputFileStreamT
//========================================================================================
:    public BASIC_OSTREAM
{
#ifdef NS_USING_STL
public:
    typedef charT              char_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
    typedef typename traits::int_type int_type;
    typedef traits             traits_type;
#endif
 
public:

                                      nsOutputFileStreamT();
                                      explicit nsOutputFileStreamT(
                                           const nsFilePath& inFile,
                                           IOS_BASE::openmode mode = IOS_BASE::out|IOS_BASE::trunc,
                                           PRIntn accessMode = 0x00200);
 
    virtual                           ~nsOutputFileStreamT();

    FILE_BUFFER_TYPE*                 rdbuf() const;
    inline bool                       is_open();
    inline void                       open(
                                          const nsFilePath& inFile,
                                          IOS_BASE::openmode mode = IOS_BASE::out|IOS_BASE::trunc,
                                          PRIntn accessMode = 0x00200);
    inline void                       close();

private:
    FILE_BUFFER_TYPE                  mBuffer;
}; // class nsOutputFileStreamT

//========================================================================================
//    Implementation of nsFileBufferT
//========================================================================================

//----------------------------------------------------------------------------------------
TEMPLATE_DEF
inline FILE_BUFFER_TYPE::nsFileBufferT()
    :    BASIC_STREAMBUF(), mFileDesc(NULL)
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline FILE_BUFFER_TYPE::nsFileBufferT(PRFileDesc* pfarg)
    :    BASIC_STREAMBUF(), mFileDesc(pfarg)
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline FILE_BUFFER_TYPE* FILE_BUFFER_TYPE::close()
// Must precede the destructor because both are inline.
//----------------------------------------------------------------------------------------
{
    if (mFileDesc==PR_STDIN || mFileDesc==PR_STDOUT || mFileDesc==PR_STDERR) 
       return this;
    NS_WRITE_LOCK(_mutex);
    return (mFileDesc && PR_Close(mFileDesc) == PR_SUCCESS) ? mFileDesc = 0, this : 0;
}

#if defined(DEFINING_FILE_STREAM)
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
FILE_BUFFER_TYPE::~nsFileBufferT()
//----------------------------------------------------------------------------------------
{
    close();
}
#endif // #if defined(DEFINING_FILE_STREAM)

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline bool
FILE_BUFFER_TYPE::is_open() const 
//----------------------------------------------------------------------------------------
{
    NS_READ_LOCK(_mutex);
    return bool(mFileDesc);  // in case it is typedefed to int
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline FILE_BUFFER_TYPE* FILE_BUFFER_TYPE::open(
    const nsFilePath& inFile,
    IOS_BASE::openmode mode,
    PRIntn accessMode) 
//----------------------------------------------------------------------------------------
{
    if (mFileDesc) 
       return 0;
    NS_WRITE_LOCK(_mutex);
    mFileDesc = nsFileStreamHelpers::open(inFile, mode, accessMode);
    if (!mFileDesc)
    	return 0;
    mode_ = mode;
    return this;
}    //    FILE_BUFFER_TYPE::open

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline int FILE_BUFFER_TYPE:: sync()
//----------------------------------------------------------------------------------------
{
    return (mFileDesc ? (int)PR_Sync(mFileDesc) : 0);
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline BASIC_STREAMBUF* FILE_BUFFER_TYPE::setbuf(char_type*, streamsize)
//----------------------------------------------------------------------------------------
{
    return (!mFileDesc) ? 0 : this;
}

#if defined(DEFINING_FILE_STREAM)
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
INT_TYPE FILE_BUFFER_TYPE::overflow(int_type c)
//----------------------------------------------------------------------------------------
{
#ifndef NS_USING_STL
	char ch = c;
    PRInt32 bytesWrit1 = PR_Write(mFileDesc, &ch, sizeof(ch));
    return bytesWrit1 < sizeof(ch) ? EOF_VALUE : c;
#else
#ifdef NS_EXPLICIT_FUNC_TEMPLATE_ARG    
    const ofacet_type& ft=use_facet<ofacet_type>(getloc());
#elif defined(XP_WIN) || defined(XP_OS2)
	const ofacet_type& ft=use_facet(getloc(), (ofacet_type*)0, false);
#else
    const ofacet_type& ft=use_facet(getloc(), (ofacet_type*)0);
#endif
    char_type ch = traits_type::to_char_type(c);
    if (!mFileDesc)
        return EOF_VALUE;
    if (traits_type::eq_int_type(c, EOF_VALUE))
        return traits_type::not_eof(c);
    if (ft.always_noconv())
    {
        PRInt32 bytesWrit1 = PR_Write(mFileDesc, &ch, sizeof(ch));
        return bytesWrit1 < sizeof(ch) ? EOF_VALUE : c;
    }
    { // <- sic!
        state_type fst;
         const char_type* end;
        char buf[4];
        char* ebuf;
        result conv;
        if ((conv=ft.out(fst, &ch, &ch+1, end, buf, buf+3, ebuf))==
                codecvt_base::noconv)
        {
             PRInt32 bytesWrit2 = PR_Write(mFileDesc, &ch, sizeof(ch));
             return bytesWrit2 < sizeof(ch) ? EOF_VALUE : c;
        }
        if ((conv==codecvt_base::partial)||(conv==codecvt_base::error))
            return EOF_VALUE;
        *ebuf=0;
        PRInt32 bytesWrit3 = strlen(buf);
        return PR_Write(mFileDesc, buf, bytesWrit3) < bytesWrit3 ? EOF_VALUE : c;
    }
#endif
}
#endif // #if defined(DEFINING_FILE_STREAM)

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline INT_TYPE FILE_BUFFER_TYPE::underflow()
//----------------------------------------------------------------------------------------
{
    if (!mFileDesc)
       return EOF_VALUE;
    char_type s;
    PRInt32 request = 1; 
    if (1 != PR_Read(mFileDesc, &s, request))
       return EOF_VALUE;
    PR_Seek(mFileDesc, -1, PR_SEEK_CUR);
    return (int_type)s;
}

#if defined(DEFINING_FILE_STREAM)
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
streamsize FILE_BUFFER_TYPE::xsputn(const char_type* s, streamsize n)
//----------------------------------------------------------------------------------------
{
#ifndef NS_USING_STL
    PRInt32 bytesWrit1 = PR_Write(mFileDesc, s, sizeof(char) * size_t(n));
    return bytesWrit1 < 0 ? 0 : (streamsize)bytesWrit1;
#else
#ifdef NS_EXPLICIT_FUNC_TEMPLATE_ARG
    const ofacet_type& ft=use_facet<ofacet_type>(loc);
#elif defined(XP_WIN) || defined(XP_OS2)
	const ofacet_type& ft=use_facet(getloc(), (ofacet_type*)0, false);
#else
    const ofacet_type& ft=use_facet(getloc(), (ofacet_type*)0);
#endif
    if (!mFileDesc || !n) 
         return 0;
    if (ft.always_noconv())
    {
         PRInt32 bytesWrit1 = PR_Write(mFileDesc, s, sizeof(char) * size_t(n));
         return bytesWrit1 < 0 ? 0 : (streamsize)bytesWrit1;
    }
    { // <- sic!
         state_type fst;
         const char_type* end;
         char buf[8];
         char* ebuf;
         result conv;
#ifdef NS_EXPLICIT_FUNC_TEMPLATE_ARG     
         if ((conv=use_facet<ofacet_type>(getloc()).
#elif defined(XP_WIN) || defined(XP_OS2)
	     if ((conv=use_facet(getloc(), (ofacet_type*)0, false).
#else
         if ((conv=use_facet(getloc(), (ofacet_type*)0).
#endif
             out(fst, s, s+n, end, buf, buf+7, ebuf))==codecvt_base::noconv)
             return (streamsize)PR_Write(mFileDesc, s, sizeof(char) * size_t(n));
         if ((conv==codecvt_base::partial) ||(conv==codecvt_base::error)) 
             return 0;
         *ebuf=0;
         PRInt32 bytesWrit2 = strlen(buf);
         bytesWrit2 = PR_Write(mFileDesc, buf, bytesWrit2);
         return bytesWrit2 < 0 ? 0 : streamsize(bytesWrit2 / sizeof(char_type));
    }
#endif
}   //  FILE_BUFFER_TYPE::xsputn
#endif // #if defined(DEFINING_FILE_STREAM)

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline INT_TYPE FILE_BUFFER_TYPE::pbackfail(int_type c)
//----------------------------------------------------------------------------------------
{
    if (!mFileDesc)
        return EOF_VALUE;
    if (PR_Seek(mFileDesc, -1, PR_SEEK_CUR) < 0)
        return EOF_VALUE;
 #ifdef NS_USING_STL
    return (traits::eq_int_type(c, EOF_VALUE)) ? traits::not_eof(c) : c;
 #else
 	return c;
 #endif
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline INT_TYPE FILE_BUFFER_TYPE::uflow()
//----------------------------------------------------------------------------------------
{
    if (!mFileDesc)
        return EOF_VALUE;
    char_type s;
    if (1 != PR_Read(mFileDesc, &s, 1)) // attempt to read 1 byte, confirm 1 byte
        return EOF_VALUE;
    return (int_type)s;
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline streamsize FILE_BUFFER_TYPE::xsgetn(char_type* s, streamsize n)
//----------------------------------------------------------------------------------------
{
    return mFileDesc ? (streamsize)PR_Read(mFileDesc, s, sizeof(char) * size_t(n)) : 0;
}

#ifdef NS_USING_STL
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline void FILE_BUFFER_TYPE::imbue(const locale& loc_arg)
//----------------------------------------------------------------------------------------
{
#ifdef XP_MAC
   loc = loc_arg;
#endif
}
#endif

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline streamsize FILE_BUFFER_TYPE::showmanyc()
//----------------------------------------------------------------------------------------
{
    return (streamsize)PR_Available(mFileDesc);
}

#if defined(DEFINING_FILE_STREAM)
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
POS_TYPE FILE_BUFFER_TYPE::seekoff(
    OFF_TYPE  off, 
    SEEK_DIR way,
    IOS_BASE::openmode /* which */)
//----------------------------------------------------------------------------------------
{
    if (!mFileDesc ||
#ifdef NS_USING_STL
    ((way&IOS_BASE::beg) && off<0) || ((way&IOS_BASE::end) && off > 0)
#else
    ((way == IOS_BASE::beg) && off<0) || ((way == IOS_BASE::end) && off > 0)
#endif
    )
        return pos_type(-1);
    PRSeekWhence  poseek = PR_SEEK_CUR;
    switch (way)
    {
        case IOS_BASE::beg : poseek= PR_SEEK_SET; 
                        break;
        case IOS_BASE::end : poseek= PR_SEEK_END; 
                        break;
    }
    PRInt32 position = PR_Seek(mFileDesc, off, poseek);
    if (position < 0) 
        return pos_type(-1);
    return pos_type(position);
}
#endif // #if defined(DEFINING_FILE_STREAM)

#if defined(DEFINING_FILE_STREAM)
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
POS_TYPE FILE_BUFFER_TYPE::seekpos(pos_type sp, IOS_BASE::openmode)
//----------------------------------------------------------------------------------------
{
    if (!mFileDesc || sp==pos_type(-1))
        return -1;
#if defined(XP_WIN) || defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
    PRInt32 position = sp;
#else
    PRInt32 position = sp.offset();
#endif
    position = PR_Seek(mFileDesc, position, PR_SEEK_SET);
    if (position < 0)
        return pos_type(-1);
    return position;
}
#endif // #if defined(DEFINING_FILE_STREAM)

//========================================================================================
//    Implementation of nsInputFileStreamT
//========================================================================================

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline INPUT_FILE_STREAM::nsInputFileStreamT()
    : BASIC_ISTREAM(&mBuffer)
//----------------------------------------------------------------------------------------
{
    // already inited
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline INPUT_FILE_STREAM::nsInputFileStreamT(
    const nsFilePath& inFile, 
    IOS_BASE::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
    : BASIC_ISTREAM(&mBuffer)
{
    // already inited
    if (!mBuffer.open(inFile, openmode(mode|in), accessMode))
      setstate(failbit);
}

#if defined(DEFINING_FILE_STREAM)
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
INPUT_FILE_STREAM::~nsInputFileStreamT()
//----------------------------------------------------------------------------------------
{
}
#endif // #if defined(DEFINING_FILE_STREAM)

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline FILE_BUFFER_TYPE* INPUT_FILE_STREAM::rdbuf() const 
//----------------------------------------------------------------------------------------
{
    return (FILE_BUFFER_TYPE*)&mBuffer;
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline bool INPUT_FILE_STREAM:: is_open()
//----------------------------------------------------------------------------------------
{ 
    return mBuffer.is_open();
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline void INPUT_FILE_STREAM::open(
    const nsFilePath& inFile,
    IOS_BASE::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.open(inFile, openmode(mode|in), accessMode)) 
       setstate(failbit);
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline void INPUT_FILE_STREAM::close()
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.close()) 
       setstate(failbit);
}

//========================================================================================
//    Implementation of nsOutputFileStreamT
//========================================================================================

//----------------------------------------------------------------------------------------
TEMPLATE_DEF
inline OUTPUT_FILE_STREAM::nsOutputFileStreamT()
    : BASIC_OSTREAM(&mBuffer)
//----------------------------------------------------------------------------------------
{
    // already inited
}

#if defined(DEFINING_FILE_STREAM)
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
OUTPUT_FILE_STREAM::nsOutputFileStreamT(
    const nsFilePath& inFile, 
    IOS_BASE::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
:   BASIC_OSTREAM(&mBuffer)
{
    // already inited
    if (!mBuffer.open(inFile, openmode(mode|out), accessMode))
       setstate(failbit);
}
#endif // #if defined(DEFINING_FILE_STREAM)

#if defined(DEFINING_FILE_STREAM)
//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline OUTPUT_FILE_STREAM::~nsOutputFileStreamT()
//----------------------------------------------------------------------------------------
{
}
#endif // #if defined(DEFINING_FILE_STREAM)

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline FILE_BUFFER_TYPE* 
OUTPUT_FILE_STREAM::rdbuf() const
//----------------------------------------------------------------------------------------
{
    return (FILE_BUFFER_TYPE*)&mBuffer;
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline bool OUTPUT_FILE_STREAM:: is_open()
//----------------------------------------------------------------------------------------
{
    return mBuffer.is_open();
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline void OUTPUT_FILE_STREAM::open(
    const nsFilePath& inFile,
    IOS_BASE::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.open(inFile, openmode(mode | out), accessMode))
       setstate(failbit);
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline void OUTPUT_FILE_STREAM:: close()
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.close())
       setstate(failbit);
}

//========================================================================================
TEMPLATE_DEF
class nsIOFileStreamT : public BASIC_IOSTREAM
//========================================================================================
{
#ifdef NS_USING_STL
public:
    typedef charT                 char_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
    typedef typename traits::int_type int_type;
    typedef traits                traits_type;
#endif

public:
                                        nsIOFileStreamT();
                                        explicit nsIOFileStreamT(
                                            const nsFilePath& inFile, 
                                            IOS_BASE::openmode mode = IOS_BASE::in|IOS_BASE::out,
                                            PRIntn accessMode = 0x00600);

    virtual                             ~nsIOFileStreamT();

    FILE_BUFFER_TYPE*                   rdbuf() const;
    inline bool                         is_open();
    inline void                         open(
                                            const nsFilePath& inFile,
                                            IOS_BASE::openmode mode = IOS_BASE::in|IOS_BASE::out,
                                            PRIntn accessMode = 0x00600);
    inline void                         close();

private:
    FILE_BUFFER_TYPE                    mBuffer;
}; // class nsIOFileStreamT

//========================================================================================
//    Implementation of nsIOFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline IO_FILE_STREAM::nsIOFileStreamT()
//----------------------------------------------------------------------------------------
    : mBuffer(), BASIC_IOSTREAM(&mBuffer)
{
    // already inited
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline IO_FILE_STREAM::nsIOFileStreamT(
    const nsFilePath& inFile, 
    IOS_BASE::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
    : mBuffer(), BASIC_IOSTREAM(&mBuffer)
{
    // already inited
    if (!mBuffer.open(inFile, mode, accessMode))
      setstate(failbit);
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline IO_FILE_STREAM::~nsIOFileStreamT()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline FILE_BUFFER_TYPE* 
IO_FILE_STREAM::rdbuf() const
//----------------------------------------------------------------------------------------
{
    return (FILE_BUFFER_TYPE*)&mBuffer;
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline bool IO_FILE_STREAM::is_open() 
//----------------------------------------------------------------------------------------
{
    return mBuffer.is_open();
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline void IO_FILE_STREAM::open(
    const nsFilePath& inFile,
    IOS_BASE::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.open(inFile, mode, accessMode)) 
      setstate(failbit);
}

//----------------------------------------------------------------------------------------
TEMPLATE_DEF 
inline void IO_FILE_STREAM::close() 
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.close()) 
      setstate(failbit);
}

//========================================================================================
//    Specializations of the stream templates
//========================================================================================

#ifdef NS_USING_STL
typedef nsFileBufferT<char, char_traits<char> > nsFileBuffer;
typedef nsInputFileStreamT<char, char_traits<char> > nsInputFileStream;
typedef nsOutputFileStreamT<char, char_traits<char> > nsOutputFileStream;
typedef nsIOFileStreamT<char, char_traits<char> > nsIOFileStream;

#ifdef NS_USING_WIDE_CHAR
typedef nsFileBufferT<wchar_t, char_traits<wchar_t> > nsWideFileBuffer;
typedef nsInputFileStreamT<wchar_t, char_traits<wchar_t> > nsWideInputFileStream;
typedef nsOutputFileStreamT<wchar_t, char_traits<wchar_t> > nsWideOutputFileStream;
typedef nsIOFileStreamT<wchar_t, char_traits<wchar_t> > nsWideIOFileStream;
#endif // NS_USING_WIDE_CHAR

#else

typedef nsFileBufferT nsFileBuffer;
typedef nsInputFileStreamT nsInputFileStream;
typedef nsOutputFileStreamT nsOutputFileStream;
typedef nsIOFileStreamT nsIOFileStream;

#endif

#endif /* _FILESTREAM_H_ */

