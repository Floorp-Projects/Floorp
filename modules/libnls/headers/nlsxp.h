/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef _NLSNLS_H
#define _NLSNLS_H

/***********************************************************************
 **
 ** Type definitions are provided by NSPR20, using prtypes.h
 **
 ************************************************************************/
#include "prtypes.h"

/** need to have nil and NULL defined regardless of system **/
#ifndef nil
#define nil 0
#endif

#ifndef NULL
#define NULL 0
#endif

/* which system are we on, get the base macro defined */



#if defined(macintosh) || defined(__MWERKS__) || defined(applec)
#ifndef macintosh
#define macintosh 1
#endif
#endif

#if defined(__unix) || defined(unix) || defined(UNIX) || defined(XP_UNIX)
#ifndef unix
#define unix 1
#endif
#endif

#if !defined(macintosh) && !defined(_WINDOWS) && !defined(_CONSOLE) && !defined(unix)
  /* #error nls library can't determine system type */
#endif

/* flush out all the system macros */

#ifdef macintosh
# ifndef NLS_MAC
# define NLS_MAC 1
# endif
#if defined(powerc) || defined(__powerc)
#ifndef NLS_MAC_PPC
#define NLS_MAC_PPC 1
#endif /* NLS_MAC_PPC */
#else /* powerpc */
#ifndef NLS_MAC_68K
#define NLS_MAC_68K 1
#endif /* NLS_MAC_68K */
#endif /* else powerpc */
# define NLS_IS_MAC 1
# define NLS_MAC_ARG(x) x
#else
# define NLS_IS_MAC 0
# define NLS_MAC_ARG(x)
#endif

#if defined(_WINDOWS) || defined(XP_PC) || defined(_CONSOLE)
#ifndef NLS_HAVE_DLL
# define NLS_HAVE_DLL
#endif /* NLS_HAVE_DLL */
#ifndef NLS_PC
# define NLS_PC
#endif /* ifndef NLS_PC */
#ifndef NLS_WIN
# define NLS_WIN
#endif /* ifndef NLS_WIN */
# define NLS_IS_WIN 1
# define NLS_WIN_ARG(x) x
#if defined(_WIN32) || defined(WIN32)
#ifndef NLS_WIN32
# define NLS_WIN32
#endif /* NLS_WIN32 */
#else /* defined(_WIN32) || defined(WIN32) */
#ifndef NLS_WIN16
# define NLS_WIN16
#endif /*  NLS_WIN16 */
#endif /* defined(_WIN32) || defined(WIN32) */
#else /* defined(_WINDOWS) || defined(XP_PC) */
# define NLS_IS_WIN 0
# define NLS_WIN_ARG(x)
#endif /* defined(_WINDOWS) || defined(XP_PC) */

#ifdef unix

#include <sys/types.h>
#include <sys/param.h>

#if defined(HPUX)
#ifndef NLS_HAVE_DLL
# define NLS_HAVE_DLL
#endif /* NLS_HAVE_DLL */
#ifndef NLS_USE_HPSHL
# define NLS_USE_HPSHL
#endif /* NLS_USE_HPSHL */
#endif /* defined(HPUX) */

/* Figure out if we are on AIX, and Set NLS variable  */
#if defined(_AIX42) || defined (AIX4_2)
#define NLS_AIX42 1
#undef NLS_AIX41
#else
#if defined (_AIX41) || defined (AIX4_1)
#define NLS_AIX41 1
#undef NLS_AIX42
#endif
#endif


/* UNIX Platforms with DLL capibalities */

#if defined(IRIX) || defined(LINUX) ||  defined(SOLARIS) || defined(SUNOS4) || defined(SNI) || defined(SONY) || defined(NECSVR4) || defined(SCO) || defined(UNIXWARE) || defined (AIX) || defined (OSF1) || defined(NLS_AIX41) || defined (NLS_AIX42)
#ifndef NLS_HAVE_DLL
# define NLS_HAVE_DLL
#endif /* NLS_HAVE_DLL */
#ifndef NLS_USE_DLFCN
# define NLS_USE_DLFCN
#endif /* NLS_USE_DLFCN */
#endif /* defined(HPUX) */

/* UNIX Platforms without DLL capabilities */

#if defined(BSDI) || defined(NCR)
#undef NLS_HAVE_DLL
#undef NLS_USE_DLFCN
#endif

# ifndef NLS_UNIX
# define NLS_UNIX
# endif
# define NLS_IS_UNIX 1
# define NLS_UNIX_ARG(x) x
#else
# define NLS_IS_UNIX 0
# define NLS_UNIX_ARG(x)
#endif

/* what language do we have? */

#if defined(__cplusplus)
# define NLS_CPLUSPLUS
# define NLS_IS_CPLUSPLUS 1
#else
# define NLS_IS_CPLUSPLUS 0
#endif

#if defined (NLS_MAC)
#include <stdio.h>
#include <ConditionalMacros.h>
#include <CodeFragments.h>
#include <TextUtils.h>
#include <Types.h>
#include <Strings.h>
#include <Errors.h>
#include <Files.h>
#include <StandardFile.h>
#include <Resources.h>
#endif


/*
    language macros
    
    If C++ code includes a prototype for a function *compiled* in C, it needs to be
    wrapped in extern "C" for the linking to work properly. On the Mac, all code is
    being compiled in C++ so this isn't necessary, and Unix compiles it all in C. So
    only Windows actually will make use of the defined macros.
*/

#if defined(NLS_CPLUSPLUS)
# define NLS_BEGIN_PROTOS extern "C" {
# define NLS_END_PROTOS }
#else
# define NLS_BEGIN_PROTOS
# define NLS_END_PROTOS
#endif

#ifndef MIN
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y)	(((x) > (y)) ? (x) : (y))
#endif

#if defined(_DEBUG) || defined(DEBUG)
#if !defined (NLS_WIN16)
#define NLS_DEBUG 1
#endif
#endif


#if defined (NLS_DEBUG_INTERNAL)
#if defined(NLS_WIN32)
#include <assert.h>
#include <crtdbg.h>
#define NLS_ASSERT(a)		if (!(a)) {_ASSERT(a);}
#define NLS_ASSERTION(a,b)  if (!(a)) {_ASSERT(a);goto b;}
#else
#if defined (NLS_WIN16)
#define NLS_ASSERT(a) ((void)0)
#define NLS_ASSERTION(a,b) if (!(a)) { goto b; }
#else
#if defined (NLS_UNIX)
#define NLS_ASSERT(a)  ((a) ? ((void)0) :  (void)fprintf(stderr, "assert: line %d, file %s%c\n", __LINE__, __FILE__, 7))
#define NLS_ASSERTION(a,b)  if (!(a)) { (void)fprintf(stderr, "assert: line %d, file %s%c\n", __LINE__, __FILE__, 7) ; goto b; }
#else
#if defined (NLS_MAC)
#define NLS_ASSERT(a)  ((void)0)
#define NLS_ASSERTION(a,b)   if (!(a)) {goto b;}
#else
#define NLS_ASSERT(a)  ((void)0)
#define NLS_ASSERTION(a,b)  if (!(a)) {goto b;}
#endif /* defined (NLS_MAC) */
#endif /* defined (NLS_UNIX) */
#endif /* defined (NLS_WIN16) */
#endif /* defined (NLS_WIN32) */
#else  /* defined (NLS_DEBUG) */
#define NLS_ASSERT(a)  ((void)0)
#define NLS_ASSERTION(a,b)  if (!(a)) {goto b;}
#endif /* defined (NLS_DEBUG_INTERNAL) */

typedef char nlsBool;
enum _nlsBool {
        nlsTrue		= 1,
        nlsFalse	= 0
     };

typedef unsigned char byte;


/*
  Define an XPlatform Type for an C function 
   -- on WIN16 using void (*fp)void casts away runtime info....
*/

/*
 * Define standard types we need if we are being built standalone
 */




/* standard system headers */

#if !defined(RC_INVOKED)
#include <assert.h>
#include <ctype.h>
#ifdef __sgi
# include <float.h>
# include <sys/bsd_types.h>
#endif
#ifdef NLS_UNIX
#include <stdarg.h>
#endif
#include <limits.h>
#include <locale.h>
#if defined(NLS_WIN) && defined(NLS_CPLUSPLUS) && defined(_MSC_VER) && _MSC_VER >= 1020
/* math.h under MSVC 4.2 needs C++ linkage when C++. */
extern "C++"    {
#include <math.h>
}
#elif (defined(__hpux) || defined(SCO)) && defined(__cplusplus)
extern "C++"    {
#include <math.h>
}
#else
#include <math.h>
#endif
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(AIX)
#include <strings.h>          /* strcasecmp, etc. */
#endif
#include <time.h>
#endif

#if defined(NLS_PC)

#if defined(_WIN32)

#define NLSAPI_EXPORT(__x)      _declspec(dllexport) __x
#define NLSAPI_IMPORT(__x)      _declspec(dllimport) __x
#define NLSAPI_CLASS_EXPORT		_declspec(dllexport)
#define NLSAPI_CLASS_IMPORT		_declspec(dllimport)

#else  /* !_WIN32 */

#if defined(_WINDLL)
#define NLSAPI_EXPORT(__x)      __x _cdecl _loadds _export
#define NLSAPI_IMPORT(__x)      __x
#define NLSAPI_CLASS_EXPORT		_export
#define NLSAPI_CLASS_IMPORT	

#else   /* !_WINDLL */

#define NLSAPI_EXPORT(__x)      __x _cdecl _export
#define NLSAPI_IMPORT(__x)      __x
#define NLSAPI_CLASS_EXPORT		_export
#define NLSAPI_CLASS_IMPORT	
#endif  /* ! WINDLL */

#endif  /* !_WIN32 */

#elif defined(NLS_MAC)
  /* Mac */
#define NLSAPI_EXPORT(__x)      __declspec(export) __x
#define NLSAPI_IMPORT(__x)      __x
#define NLSAPI_CLASS_EXPORT		
#define NLSAPI_CLASS_IMPORT		

#else /* Unix */  

#define NLSAPI_EXPORT(__x)      __x
#define NLSAPI_IMPORT(__x)      __x
#define NLSAPI_CLASS_EXPORT		
#define NLSAPI_CLASS_IMPORT		

#endif /* Unix */

#if defined(NLS_LIBCNV_IMP)
#define NLSCNVAPI_PUBLIC(__x)      NLSAPI_EXPORT(__x)
#define NLSCNVAPI_PUBLIC_CLASS	   NLSAPI_CLASS_EXPORT	
#else
#define NLSCNVAPI_PUBLIC(__x)      NLSAPI_IMPORT(__x)
#define NLSCNVAPI_PUBLIC_CLASS	   NLSAPI_CLASS_IMPORT	
#endif

#if defined(NLS_LIBUNI_IMP)
#define T_UTILITY_IMPLEMENTATION 1
#define NLSUNIAPI_PUBLIC(__x)      NLSAPI_EXPORT(__x)
#define NLSUNIAPI_PUBLIC_CLASS	   NLSAPI_CLASS_EXPORT	
#else
#define NLSUNIAPI_PUBLIC(__x)      NLSAPI_IMPORT(__x)
#define NLSUNIAPI_PUBLIC_CLASS	   NLSAPI_CLASS_IMPORT	
#endif

#if defined(NLS_LIBCOL_IMP)
#define T_COLLATE_IMPLEMENTATION 1
#define NLSCOLAPI_PUBLIC(__x)      NLSAPI_EXPORT(__x)
#define NLSCOLAPI_PUBLIC_CLASS	   NLSAPI_CLASS_EXPORT	
#else
#define NLSCOLAPI_PUBLIC(__x)      NLSAPI_IMPORT(__x)
#define NLSCOLAPI_PUBLIC_CLASS	   NLSAPI_CLASS_IMPORT	
#endif

#if defined(NLS_LIBBRK_IMP)
#define T_FINDWORD_IMPLEMENTATION 1
#define NLSBRKAPI_PUBLIC(__x)      NLSAPI_EXPORT(__x)
#define NLSBRKAPI_PUBLIC_CLASS	   NLSAPI_CLASS_EXPORT	
#else
#define NLSBRKAPI_PUBLIC(__x)      NLSAPI_IMPORT(__x)
#define NLSBRKAPI_PUBLIC_CLASS	   NLSAPI_CLASS_IMPORT	
#endif

#if defined(NLS_LIBFMT_IMP)
#define T_FORMAT_IMPLEMENTATION 1
#define NLSFMTAPI_PUBLIC(__x)      NLSAPI_EXPORT(__x)
#define NLSFMTAPI_PUBLIC_CLASS	   NLSAPI_CLASS_EXPORT	
#else
#define NLSFMTAPI_PUBLIC(__x)      NLSAPI_IMPORT(__x)
#define NLSFMTAPI_PUBLIC_CLASS	   NLSAPI_CLASS_IMPORT	
#endif

#if defined(NLS_LIBRES_IMP)
#define NLSRESAPI_PUBLIC(__x)      NLSAPI_EXPORT(__x)
#define NLSRESAPI_PUBLIC_CLASS	   NLSAPI_CLASS_EXPORT	
#else
#define NLSRESAPI_PUBLIC(__x)      NLSAPI_IMPORT(__x)
#define NLSRESAPI_PUBLIC_CLASS	   NLSAPI_CLASS_IMPORT	
#endif

#if defined(NLS_LIBPRS_IMP)
#define NLSPRSAPI_PUBLIC(__x)      NLSAPI_EXPORT(__x)
#define NLSPRSAPI_PUBLIC_CLASS	   NLSAPI_CLASS_EXPORT	
#else
#define NLSPRSAPI_PUBLIC(__x)      NLSAPI_IMPORT(__x)
#define NLSPRSAPI_PUBLIC_CLASS	   NLSAPI_CLASS_IMPORT	
#endif
                      
/*
 * huge memory model for Win16, yes we do have some huge arrays
 */
 
#ifdef NLS_WIN16   
#define NLS_HUGE __huge
#define HUGEP __huge
#else         
#define NLS_HUGE
#define HUGEP 
#endif

/**
 ** farproc for non-windows platforms
 **/

#ifndef NLS_WIN

#ifndef XP_UNIX
#define FARPROC void*
#else
#include "md/_unixos.h"
#endif /* XP_UNIX */

#endif /* NLS_WIN */

#ifdef NLS_WIN
#define MODULE_PATH_LENGTH 255	/* maximum path+filename length for getting DLL names */
#endif /* NLS_WIN */

#ifdef NLS_WIN16
NLS_BEGIN_PROTOS


/*
** The following RTL routines are unavailable for WIN16 DLLs
*/
#ifdef _WINDLL

#include "windows.h"

/* XXX:  Need to include all of the winsock calls as well... */
NLSCNVAPI_PUBLIC(int)    sscanf(const char *, const char *, ...);
NLSCNVAPI_PUBLIC(int)    fscanf(FILE *stream, const char *, ...);
#endif /* _WINDLL */

NLS_END_PROTOS

#endif /* NLS_WIN16 */



/** Thread info for NLS libary to use
 *    lock:      lock variable
 *    fn_lock:   thread lock API
 *    fn_unlock: thread unlock API
 */
#include "muteximp.h"

typedef MutexImplementation  NLS_ThreadInfo;

/* X-platform stuff for directories */
#ifdef NLS_MAC
#define NLS_DIRECTORY_SEPARATOR				'/'
#define NLS_DIRECTORY_SEPARATOR_STR				"/"
#endif

#ifdef NLS_PC
#define NLS_DIRECTORY_SEPARATOR         '\\'
#define NLS_DIRECTORY_SEPARATOR_STR     "\\"
#endif

#ifdef NLS_UNIX
#define NLS_DIRECTORY_SEPARATOR	    '/'
#define NLS_DIRECTORY_SEPARATOR_STR	"/"
#endif

/********************* case insensitive comparison *******************************/
#if defined(NLS_MAC) || defined(NLS_WIN16)
NLS_BEGIN_PROTOS
int __strcmpi(const char* s1, const char* s2);
NLS_END_PROTOS
#define strcasecmp(s1,s2) (__strcmpi(s1,s2))
#elif defined (NLS_WIN32)
#define strcasecmp(s1,s2) (_stricmp(s1,s2))
#endif

/********************* Unicode Type Names *******************************/

#if !defined (NLS_MAC) && !defined(AIXV4)/* Defined in Types.h on MAC */
typedef wchar_t UniChar; 
#endif

/** UCS4 Unicode Char */
typedef unsigned long UniCharUCS4;


/********************* Libnls Version Numbers ******************************/

#define kLibnlsVersion20			"20"
#define kLibnlsVersion30                        "30"
#define kLibnlsVersionCurrent			kLibnlsVersion30

#define kLibnlsCollationVersion300		"3.00"
#define kLibnlsCollationVersionCurrent	kLibnlsCollationVersion300

/********************* UnicodeConverter Names *******************************/


#define			NLS_MAX_ENCODING_NAME_SIZE			64

/*				Constant name, preferred MIME name	IANA standard encoding name */
/*				---------------------------------	--------------------------- */
#define			NLS_ENCODING_US_ASCII				"ANSI_X3.4-1968"
#define			NLS_ENCODING_ISO_8859_1				"ISO_8859-1:1987"
#define			NLS_ENCODING_ISO_8859_2				"ISO_8859-2:1987"
#define			NLS_ENCODING_ISO_8859_3				"ISO_8859-3:1988"
#define			NLS_ENCODING_ISO_8859_4				"ISO_8859-4:1988"
#define			NLS_ENCODING_ISO_8859_5				"ISO_8859-5:1988"
#define			NLS_ENCODING_ISO_8859_6				"ISO_8859-6:1987"
#define			NLS_ENCODING_ISO_8859_7				"ISO_8859-7:1987"
#define			NLS_ENCODING_ISO_8859_8				"ISO_8859-8:1988"
#define			NLS_ENCODING_ISO_8859_9				"ISO_8859-9:1989"
#define			NLS_ENCODING_ISO_2022_JP			"ISO-2022-JP"
#define			NLS_ENCODING_SHIFT_JIS				"Shift_JIS"
#define			NLS_ENCODING_EUC_JP					"Extended_UNIX_Code_Packed_Format_for_Japanese"
#define			NLS_ENCODING_ISO_2022_JP			"ISO-2022-JP"
#define			NLS_ENCODING_ISO_2022_JP_2			"ISO-2022-JP-2"
#define			NLS_ENCODING_JIS_X0208_1983			"JIS_C6226-1983"
#define			NLS_ENCODING_JIS_X0201				"JIS_X0201"
#define			NLS_ENCODING_GB2312					"GB2312"
#define			NLS_ENCODING_EUC_KR					"EUC-KR"
#define			NLS_ENCODING_ISO_2022_KR			"ISO-2022-KR"
#define			NLS_ENCODING_KOI8_R					"KOI8-R"
#define			NLS_ENCODING_BIG5					"Big5"
#define			NLS_ENCODING_CNS11643_1				"cns11643_1"
#define			NLS_ENCODING_KSC5601				"KS_C_5601-1987"
#define			NLS_ENCODING_WINDOWS_1250			"windows-1250"
#define			NLS_ENCODING_WINDOWS_1251			"windows-1251"
#define			NLS_ENCODING_UTF_8					"UTF-8"
#define			NLS_ENCODING_UTF_7					"UNICODE-1-1-UTF-7"
#define                 NLS_ENCODING_MODIFIED_UTF_7                     "IMAP-MODIFIED-UTF-7"
#define			NLS_ENCODING_ISO_10646_UCS_2		"ISO-10646-UCS-2"
#define			NLS_ENCODING_ISO_10646_UCS_4		"ISO-10646-UCS-4"
#define			NLS_ENCODING_ESCAPED_UNICODE		"ESCAPED_UNICODE"
#define			NLS_ENCODING_MAC_ROMAN				"x-mac-roman" 
#define			NLS_ENCODING_DINGBATS				"x-mac-dingbats"
#define			NLS_ENCODING_SYMBOL					"x-mac-symbol"


/* Error codes */

typedef long NLS_ErrorCode;
enum _NLS_ErrorCode {
    NLS_SUCCESS								= 1,
    NLS_RESULT_TRUNCATED					= 2,
	NLS_USING_DEFAULT_LOCALE				= 3,
	NLS_USING_FALLBACK_LOCALE				= 4,
    NLS_NEW_UNICODESTRING_FAILED			= -1001,
    NLS_MEMORY_ALLOCATION_FAILED			= -1002,
    NLS_NEW_COLLATION_FAILED				= -1004,
    NLS_NEW_SORTKEY_FAILED					= -1005,
    NLS_NEW_LOCALE_FAILED					= -1006,
    NLS_NO_FROM_ENCODING					= -1007,
    NLS_NO_TO_ENCODING						= -1008,
    NLS_PARAM_ERROR							= -1009,
	NLS_MISSING_RESOURCE_ERROR				= -1010,
	NLS_INVALID_FORMAT_ERROR				= -1011,
	NLS_FILE_ACCESS_ERROR					= -1012,
	NLS_BUFFER_TO_SMALL_ERROR				= -1013,
    NLS_NEW_CONVERTER_FAILED				= -1014,
    NLS_NEW_CONVERTER_LIST_FAILED			= -1015,
	NLS_AUTO_DETECTION_ERROR				= -1016,
    NLS_NEW_TEXT_BOUNDARY_FAILED			= -1017,
    NLS_NEW_TIMEZONE_FAILED					= -1018,
	NLS_MESSAGE_PARSE_ERROR					= -1019,
	NLS_INTERNAL_PROGRAM_ERROR				= -1020,
    NLS_FUNCTION_FAILED						= -1100,

    NLS_RESOURCE_OPEN_ERROR                 = -1201,
    NLS_RESOURCE_NOT_FOUND					= -1202,
	NLS_RESOURCE_IN_CACHE					= -1203,
	NLS_JAR_OPEN_FAILURE					= -1204,
	NLS_BAD_RESOURCE_PATH					= -1205,

	NLS_LIB_NOT_FOUND						= -1300,
	NLS_ENCODING_NOT_FOUND					= -1301,
	NLS_BAD_CONVERSION_SOURCE				= -1302,
	NLS_CONVERSION_BUFFER_OVERFLOW			= -1303,
	NLS_CONVERSION_SOURCE_EXHAUSTED			= -1304,
	NLS_ENCODING_REGISTRY_NOT_INITIALIZED	= -1305,
	NLS_LIBRARY_SYMBOL_NOT_FOUND			= -1306,
	NLS_ENCODING_CONVERTER_NOT_INITIALIZED	= -1307
};


typedef char *NLS_StaticConverterRegistry;


#endif /* _NLSNLS_H */


