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

//	First checked in on 98/11/20 by John R. McMullen.  Checked in again 98/12/04.

#ifndef _STREAM_H_
#define _STREAM_H_

#include <istream> 
#include "prio.h"
#include "nsFileSpec.h"

#ifdef __MWERKS__
#ifdef MSIPL_WCHART
#define NS_USING_WIDE_CHAR
#endif
#ifdef MSIPL_EXPLICIT_FUNC_TEMPLATE_ARG
#define NS_EXPLICIT_FUNC_TEMPLATE_ARG
#endif
#endif

//========================================================================================
template<class charT, class traits>
class nsFileBufferT
//========================================================================================
:     public basic_streambuf<charT, traits>
{
     typedef codecvt_base::result result;

public:
     typedef charT                     char_type;
     typedef typename traits::pos_type pos_type;
     typedef typename traits::off_type off_type;
     typedef typename traits::int_type int_type;
     typedef traits                    traits_type;
     typedef typename traits::state_type state_type;

     typedef nsFileBufferT<charT, traits>     filebuf_type;
     typedef codecvt<charT, char, state_type> ofacet_type;
     typedef codecvt<char, charT, state_type> ifacet_type;

									     nsFileBufferT();
									     nsFileBufferT(PRFileDesc* pfile_arg);
     virtual							~nsFileBufferT();
     bool								is_open() const;
     filebuf_type*						open(
									     	const nsUnixFilePath& inFile,
									     	ios_base::openmode mode,
									     	PRIntn accessMode);
     filebuf_type*						close();
 
protected:
     virtual							int_type overflow(int_type c=traits::eof());
     virtual							int_type pbackfail(int_type c=traits::eof());
     virtual							int_type underflow();
     virtual							pos_type seekoff(
     										off_type off, ios_base::seekdir way, 
              								ios_base::openmode which=ios_base::in|ios_base::out);
     virtual							pos_type seekpos(pos_type sp,
     										ios_base::openmode which=ios_base::in|ios_base::out);
     virtual							basic_streambuf<charT, traits>* setbuf(char_type* s, streamsize n);
     virtual							int sync();
     virtual							int_type uflow();
     virtual							void imbue(const locale& loc);
     virtual							streamsize showmanyc();
     virtual							streamsize xsgetn(char_type* s, streamsize n);
     virtual							streamsize xsputn(const char_type* s, streamsize n);
 
private:
     PRFileDesc*						mFileDesc;    
     ios_base::openmode					mode_;
}; // class nsFileBufferT

//========================================================================================
template<class charT, class traits>
class nsInputFileStreamT
//========================================================================================
:     public basic_istream<charT, traits>
{
     typedef nsFileBufferT<charT, traits> filebuf_type;

public:
     typedef charT                     char_type;
     typedef typename traits::pos_type pos_type;
     typedef typename traits::off_type off_type;
     typedef typename traits::int_type int_type;
     typedef traits                    traits_type;
 
									     nsInputFileStreamT();
									     explicit nsInputFileStreamT(
									     	const nsUnixFilePath& inFile,
									     	ios_base::openmode mode=ios_base::in,
									     	PRIntn accessMode = 0x00400);

     virtual							~nsInputFileStreamT();

     filebuf_type*						rdbuf() const;
     inline bool						is_open();
     inline void						open(
									     	const nsUnixFilePath& inFile,
									     	ios_base::openmode mode=ios_base::in,
									     	PRIntn accessMode = 0x00400);
     inline void						close();

private:
     filebuf_type						mBuffer;
}; // class nsInputFileStreamT

//========================================================================================
template<class charT, class traits>
class nsOutputFileStreamT
//========================================================================================
:    public basic_ostream<charT, traits>
{
     typedef nsFileBufferT<charT, traits> filebuf_type;

public:
     typedef charT                     char_type;
     typedef typename traits::pos_type pos_type;
     typedef typename traits::off_type off_type;
     typedef typename traits::int_type int_type;
     typedef traits                    traits_type;

									     nsOutputFileStreamT();
									     explicit nsOutputFileStreamT(
									     	const nsUnixFilePath& inFile,
									     	ios_base::openmode mode = ios_base::out|ios_base::trunc,
									     	PRIntn accessMode = 0x00200);
 
     virtual							~nsOutputFileStreamT();

     filebuf_type*						rdbuf() const;
     inline bool						is_open();
     inline void						open(
									        const nsUnixFilePath& inFile,
									     	ios_base::openmode mode = ios_base::out|ios_base::trunc,
									     	PRIntn accessMode = 0x00200);
     inline void						close();

private:
     filebuf_type						mBuffer;
}; // class nsOutputFileStreamT

//========================================================================================
//	Implementation of nsFileBufferT
//========================================================================================

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsFileBufferT<charT, traits>::nsFileBufferT()
	:	basic_streambuf<charT, traits>(), mFileDesc(NULL)
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsFileBufferT<charT, traits>::nsFileBufferT(PRFileDesc* pfarg)
	:	basic_streambuf<charT, traits>(), mFileDesc(pfarg)
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsFileBufferT<charT, traits>::~nsFileBufferT()
//----------------------------------------------------------------------------------------
{
     close();
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline bool
nsFileBufferT<charT, traits>::is_open() const 
//----------------------------------------------------------------------------------------
{
     READ_LOCK(_mutex);
     return bool(mFileDesc);  // in case it is typedefed to int
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
nsFileBufferT<charT, traits>::filebuf_type* 
nsFileBufferT<charT, traits>::open(
	const nsUnixFilePath& inFile,
	ios_base::openmode mode,
	PRIntn accessMode
	) 
//----------------------------------------------------------------------------------------
{
     if (mFileDesc) 
          return 0;
     const ios_base::openmode valid_modes[]=
     {
          ios_base::out, 
          ios_base::out | ios_base::app, 
          ios_base::out | ios_base::trunc, 
          ios_base::in, 
          ios_base::in  | ios_base::out, 
          ios_base::in  | ios_base::out    | ios_base::trunc, 
//        ios_base::out | ios_base::binary, 
//         ios_base::out | ios_base::app    | ios_base::binary, 
//        ios_base::out | ios_base::trunc  | ios_base::binary, 
//        ios_base::in  | ios_base::binary, 
//        ios_base::in  | ios_base::out    | ios_base::binary, 
//        ios_base::in  | ios_base::out    | ios_base::trunc | ios_base::binary,
          0 
     };
     const int nspr_modes[]={
     	PR_WRONLY | PR_CREATE_FILE,
     	PR_WRONLY | PR_CREATE_FILE | PR_APPEND,
     	PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
     	PR_RDONLY,
     	PR_RDONLY | PR_APPEND,
     	PR_RDWR | PR_CREATE_FILE,
     	PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
//     	"wb",
//   	"ab", 
//      "wb",
//      "rb",
//      "r+b",
//      "w+b",
		0 };
     int ind=0;
     while (valid_modes[ind] && valid_modes[ind] != (mode&~ios_base::ate))
          ++ind;
     if (!nspr_modes[ind]) 
          return 0;

     WRITE_LOCK(_mutex);
     mode_ = mode;
     if ((mFileDesc = PR_Open(inFile, nspr_modes[ind], accessMode)) != 0)
          if (mode&ios_base::ate && PR_Seek(mFileDesc, 0, PR_SEEK_END) >= 0) 
               close();
         else 
               return this;
     return 0;
}	//	nsFileBufferT<charT, traits>::open

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
nsFileBufferT<charT, traits>::filebuf_type* nsFileBufferT<charT, traits>::close()
//----------------------------------------------------------------------------------------
{
     if (mFileDesc==PR_STDIN || mFileDesc==PR_STDOUT || mFileDesc==PR_STDERR) 
          return this;
     WRITE_LOCK(_mutex);
     return (mFileDesc && PR_Close(mFileDesc) == PR_SUCCESS) ? mFileDesc = 0, this : 0;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline int 
nsFileBufferT<charT, traits>:: sync()
//----------------------------------------------------------------------------------------
{
     return (mFileDesc ? (int)PR_Sync(mFileDesc) : 0);
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline basic_streambuf<charT, traits>*
nsFileBufferT<charT, traits>::setbuf(char_type*, streamsize)
//----------------------------------------------------------------------------------------
{
     return (!mFileDesc) ? 0 : this;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
nsFileBufferT<charT, traits>::int_type nsFileBufferT<charT, traits>::overflow(int_type c)
//----------------------------------------------------------------------------------------
{
#ifdef NS_EXPLICIT_FUNC_TEMPLATE_ARG      
     const ofacet_type& ft=use_facet<ofacet_type>(loc);
#else
     const ofacet_type& ft=use_facet(loc, (ofacet_type*)0);
#endif
     char_type ch = traits_type::to_char_type(c);
     if (!mFileDesc)
          return traits_type::eof();
     if (traits_type::eq_int_type(c, traits::eof()))
          return traits_type::not_eof(c);
     if (ft.always_noconv())
     {
          PRInt32 bytesWrit1 = PR_Write(mFileDesc, &ch, sizeof(ch));
          return bytesWrit1 < sizeof(ch) ? traits::eof() : c;
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
	           return bytesWrit2 < sizeof(ch) ? traits::eof() : c;
          }
          if ((conv==codecvt_base::partial)||(conv==codecvt_base::error))
               return traits::eof();
          *ebuf=0;
          PRInt32 bytesWrit3 = strlen(buf);
          return PR_Write(mFileDesc, buf, bytesWrit3) < bytesWrit3 ? traits_type::eof() : c;
     }
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsFileBufferT<charT, traits>::int_type nsFileBufferT<charT, traits>::underflow()
//----------------------------------------------------------------------------------------
{
     if (!mFileDesc)
        return traits_type::eof();
   	 char_type s;
   	 PRInt32 request = 1; 
   	 if (1 != PR_Read(mFileDesc, &s, request))
   	 	return traits_type::eof();
   	 PR_Seek(mFileDesc, -1, PR_SEEK_CUR);
     return (int_type)s;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
streamsize nsFileBufferT<charT, traits>::xsputn(const char_type* s, streamsize n)
//----------------------------------------------------------------------------------------
{
#ifdef NS_EXPLICIT_FUNC_TEMPLATE_ARG
     const ofacet_type& ft=use_facet<ofacet_type>(loc);
#else
     const ofacet_type& ft=use_facet(loc, (ofacet_type*)0);
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
          if ((conv=use_facet<ofacet_type>(loc).
               out(fst, s, s+n, end, buf, buf+7, ebuf))==codecvt_base::noconv)
#else
          if ((conv=use_facet(loc, (ofacet_type*)0).
               out(fst, s, s+n, end, buf, buf+7, ebuf))==codecvt_base::noconv)
#endif
               return (streamsize)PR_Write(mFileDesc, s, sizeof(char) * size_t(n));
          if ((conv==codecvt_base::partial) ||(conv==codecvt_base::error)) 
               return 0;
          *ebuf=0;
          PRInt32 bytesWrit2 = strlen(buf);
          bytesWrit2 = PR_Write(mFileDesc, buf, bytesWrit2);
          return bytesWrit2 < 0 ? 0 : streamsize(bytesWrit2 / sizeof(char_type));
     }
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsFileBufferT<charT, traits>::int_type 
	nsFileBufferT<charT, traits>::pbackfail(int_type c)
//----------------------------------------------------------------------------------------
{
   	 if (!mFileDesc)
   	 	return traits_type::eof();
   	 if (PR_Seek(mFileDesc, -1, PR_SEEK_CUR) < 0)
   	 	return traits_type::eof();
     return (traits::eq_int_type(c, traits_type::eof())) ? traits::not_eof(c) : c;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsFileBufferT<charT, traits>::int_type nsFileBufferT<charT, traits>::uflow()
//----------------------------------------------------------------------------------------
{
   	 if (!mFileDesc)
   	 	return traits_type::eof();
   	 char_type s;
   	 if (1 != PR_Read(mFileDesc, &s, 1)) // attempt to read 1 byte, confirm 1 byte
   	 	return traits_type::eof();
     return (int_type)s;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline streamsize nsFileBufferT<charT, traits>::xsgetn(char_type* s, streamsize n)
//----------------------------------------------------------------------------------------
{
     return mFileDesc ? (streamsize)PR_Read(mFileDesc, s, sizeof(char) * size_t(n)) : 0;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline void nsFileBufferT<charT, traits>::imbue(const locale& loc_arg)
//----------------------------------------------------------------------------------------
{
     loc = loc_arg;
}

template<class charT, class traits> 
inline streamsize
nsFileBufferT<charT, traits>::showmanyc()
{
     return (streamsize)PR_Available(mFileDesc);
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
nsFileBufferT<charT, traits>::pos_type nsFileBufferT<charT, traits>::seekoff(
     off_type  off, 
     ios_base::seekdir way,
     ios_base::openmode /* which */)
//----------------------------------------------------------------------------------------
{
     if (!mFileDesc || ((way&ios_base::beg) && off<0) || ((way&ios_base::end) && off > 0))
         return pos_type(-1);
     PRSeekWhence  poseek = PR_SEEK_CUR;
     switch (way)
     {
         case ios_base::beg : poseek= PR_SEEK_SET; 
                              break;
         case ios_base::end : poseek= PR_SEEK_END; 
                              break;
     }
     PRInt32 position = PR_Seek(mFileDesc, off, poseek);
     if (position < 0) 
          return pos_type(-1);
     return pos_type(position);
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
nsFileBufferT<charT, traits>::pos_type
nsFileBufferT<charT, traits>::seekpos(pos_type sp, ios_base::openmode)
//----------------------------------------------------------------------------------------
{
	if (!mFileDesc || sp==pos_type(-1))
		return -1;
	PRInt32 position = PR_Seek(mFileDesc, sp.offset(), PR_SEEK_SET);
	if (position < 0)
		return pos_type(-1);
	return position;
}

//========================================================================================
//	Implementation of nsInputFileStreamT
//========================================================================================

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsInputFileStreamT<charT, traits>::nsInputFileStreamT()
     : basic_istream<charT, traits>(&mBuffer)
//----------------------------------------------------------------------------------------
{
	// already inited
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsInputFileStreamT<charT, traits>::nsInputFileStreamT(
	const nsUnixFilePath& inFile, 
    ios_base::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
     : basic_istream<charT, traits>(&mBuffer)
{
	// already inited
    if (!mBuffer.open(inFile, openmode(mode|in), accessMode))
       setstate(failbit);
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsInputFileStreamT<charT, traits>::~nsInputFileStreamT()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsInputFileStreamT<charT, traits>::filebuf_type* 
nsInputFileStreamT<charT, traits>::rdbuf() const 
//----------------------------------------------------------------------------------------
{
    return (filebuf_type*)&mBuffer;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline bool 
nsInputFileStreamT<charT, traits>:: is_open()
//----------------------------------------------------------------------------------------
{ 
    return mBuffer.is_open();
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline void 
nsInputFileStreamT<charT, traits>::open(
	const nsUnixFilePath& inFile,
	ios_base::openmode mode,
	PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.open(inFile, openmode(mode|in), accessMode)) 
        setstate(failbit);
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline void nsInputFileStreamT<charT, traits>::close()
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.close()) 
        setstate(failbit);
}

//========================================================================================
//	Implementation of nsOutputFileStreamT
//========================================================================================

//----------------------------------------------------------------------------------------
template<class charT, class traits>
inline nsOutputFileStreamT<charT, traits>::nsOutputFileStreamT()
     : basic_ostream<charT, traits>(&mBuffer)
//----------------------------------------------------------------------------------------
{
	// already inited
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
nsOutputFileStreamT<charT, traits>::nsOutputFileStreamT(
	const nsUnixFilePath& inFile, 
    ios_base::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
:   basic_ostream<charT, traits>(&mBuffer)
{
	// already inited
    if (!mBuffer.open(inFile, openmode(mode|out), accessMode))
        setstate(failbit);
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsOutputFileStreamT<charT, traits>::~nsOutputFileStreamT()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsOutputFileStreamT<charT, traits>::filebuf_type* 
nsOutputFileStreamT<charT, traits>::rdbuf() const
//----------------------------------------------------------------------------------------
{
    return (filebuf_type*)&mBuffer;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline bool nsOutputFileStreamT<charT, traits>:: is_open()
//----------------------------------------------------------------------------------------
{
    return mBuffer.is_open();
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline void nsOutputFileStreamT<charT, traits>::open(
	const nsUnixFilePath& inFile,
	ios_base::openmode mode,
	PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.open(inFile, openmode(mode | out), accessMode))
        setstate(failbit);
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline void nsOutputFileStreamT<charT, traits>:: close()
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.close())
        setstate(failbit);
}

//========================================================================================
template<class charT, class traits>
class nsIOFileStreamT : public basic_iostream<charT, traits>
//========================================================================================
{
    typedef nsFileBufferT<charT, traits> filebuf_type;

public:
    typedef charT                     char_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
    typedef typename traits::int_type int_type;
    typedef traits                    traits_type;

									  	nsIOFileStreamT();
								    	explicit nsIOFileStreamT(
								    		const nsUnixFilePath& inFile, 
									        ios_base::openmode mode = ios_base::in|ios_base::out,
									        PRIntn accessMode = 0x00600);

    virtual								~nsIOFileStreamT();

    filebuf_type*						rdbuf() const;
    inline bool							is_open();
    inline void							open(
								    		const nsUnixFilePath& inFile,
									    	ios_base::openmode mode = ios_base::in|ios_base::out,
									    	PRIntn accessMode = 0x00600);
    inline void							close();

private:
    filebuf_type						mBuffer;
}; // class nsIOFileStreamT

//========================================================================================
//	Implementation of nsIOFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsIOFileStreamT<charT, traits>::nsIOFileStreamT()
//----------------------------------------------------------------------------------------
     : mBuffer(), basic_iostream<charT, traits>(&mBuffer)
{
	// already inited
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsIOFileStreamT<charT, traits>::nsIOFileStreamT(
	const nsUnixFilePath& inFile, 
    ios_base::openmode mode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
     : mBuffer(), basic_iostream<charT, traits>(&mBuffer)
{
	// already inited
    if (!mBuffer.open(inFile, mode, accessMode))
        setstate(failbit);
}

template<class charT, class traits> 
inline nsIOFileStreamT<charT, traits>::~nsIOFileStreamT()
{
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline nsIOFileStreamT<charT, traits>::filebuf_type* 
nsIOFileStreamT<charT, traits>::rdbuf() const
//----------------------------------------------------------------------------------------
{
    return (filebuf_type*)&mBuffer;
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline bool 
nsIOFileStreamT<charT, traits>::is_open() 
//----------------------------------------------------------------------------------------
{
    return mBuffer.is_open();
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline void 
nsIOFileStreamT<charT, traits>::open(
	const nsUnixFilePath& inFile,
	ios_base::openmode mode,
	PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.open(inFile, mode, accessMode)) 
        setstate(failbit);
}

//----------------------------------------------------------------------------------------
template<class charT, class traits> 
inline void 
nsIOFileStreamT<charT, traits>::close() 
//----------------------------------------------------------------------------------------
{
    if (!mBuffer.close()) 
        setstate(failbit);
}

//========================================================================================
//	Specializations of the stream templates
//========================================================================================

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

#endif /* _STREAM_H_ */

