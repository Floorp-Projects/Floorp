/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1996, 1997                                           *
*   (C) Copyright International Business Machines Corporation,  1996, 1997              *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*
*  FILE NAME : PTYPES.H
*
*	Date		Name		Description
*	12/11/96	helena		Creation.
*	02/27/97	aliu		Added typedefs for ClassID, int8, int16, int32, uint8, uint16,
*							and uint32.
*	04/01/97	aliu		Added XP_CPLUSPLUS and modified to work under C as well as C++.
*							Modified to use memcpy() for arrayCopy() fns.
*	04/14/97	aliu		Added TPlatformUtilities.
*	05/07/97	aliu		Added import/export specifiers (replacing the old broken
*							EXT_CLASS).  Added version number for our code.  Cleaned up
*							header.
*    6/20/97    helena      Java class name change.
*****************************************************************************************
*/

#ifndef _PTYPES
#define _PTYPES

#include "nlsxp.h"
#include <memory.h>
#include <string.h>


/*=====================================================================================*/
/* Analytics version number */
/*=====================================================================================*/

/**
 * Analytic package code version number.  This version number must be incremented if and
 * only if the code has changed in a binary incompatible way.  For example, if the
 * algorithm for generating sort keys has changed, this code version must be incremented.
 *
 * This is for internal use only.  Clients should use ResourceBundle::getVersionNumber().
 * ResourceBundle::getVersionNumber() returns a full version number for a resource,
 * which consists of this code version number concatenated with the ResourceBundle data
 * file version number.
 */
#define T_ANALYTIC_PACKAGE_VERSION "1"

/*=====================================================================================*/
/* Flags related to testing code coverage  */
/*=====================================================================================*/

#define TESTING_CODE_COVERAGE 1      /* defined when executing tests */
                                     /* with code coverage reporting */

/* #define EXTENDED_FUNCTIONALITY */ /* in general this is undefined */          

/*=====================================================================================*/
/* Flags specifying host character set  */
/*=====================================================================================*/

#define USES_ISO_8859_1 1
#define USES_EBCDIC 0

/*=====================================================================================*/
/* Platform/Language determination */
/*=====================================================================================*/

#if defined(__OS2__) || defined (_AIX) || defined(__AS400__)
#define USE_POSIX
#endif


/*=====================================================================================*/
/* Boolean data type */
/*=====================================================================================*/

/* uncomment these lines on compilers that don't directly support "bool" */
/* it may be necessary to change this for different platforms */
/*
typedef char bool;
enum {
    true = 1,
    false = 0
};
*/
/*Change the bool type to t_bool and true to TRUE and false to FALSE*/
/*till all the files are corrected and tested,we will retain both
definitions*/
#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

typedef char t_bool;
enum {
    TRUE = 1,
    FALSE = 0
};

/*=====================================================================================*/
/* Other generic data types */
/*=====================================================================================*/

/** Unicode character */
/*#if sizeof(wchar_t) &lt; 2 */
/*#ifndef _AIX / * AIX defines UniChar in /usr/include/sys/types.h */
/* typedef unsigned short UniChar; */
/*#endif */
/*#else */
/*typedef wchar_t UniChar; */
/*#endif */

#ifdef NSPR20

/** Unicode string offset */
/*#if sizeof(int) >= 32 */
/*typedef int       TextOffset; */
/*#else */
typedef PRInt32 TextOffset;
/*#endif */

/* Before compiling on a new platform, add a section for that platform, */
/* defining the sized integral data types. */
typedef PRInt8		t_int8;
typedef PRUint8		t_uint8;
typedef PRInt16		t_int16;
typedef PRUint16	t_uint16;
typedef PRInt32		t_int32;
typedef PRUint32	t_uint32;
#define T_INT32_MAX (LONG_MAX)

#else
/** Unicode string offset */
/*#if sizeof(int) >= 32 */
/*typedef int       TextOffset; */
/*#else */
typedef int32 TextOffset;
/*#endif */

/* Before compiling on a new platform, add a section for that platform, */
/* defining the sized integral data types. */
typedef int8		t_int8;
typedef uint8		t_uint8;
typedef int16		t_int16;
typedef uint16	        t_uint16;
typedef int32		t_int32;
typedef uint32	        t_uint32;
#define T_INT32_MAX (LONG_MAX)
#define PR_INT32(x)  x ## L
#endif

/*=====================================================================================*/
/* Calendar/TimeZone data types */
/*=====================================================================================*/

/* Date should be a 64-bit long integer. For systems/compilers */
/* that do not support 64-bit long integer, use IEEE double. */
/*#if sizeof(long) >= 64 */
/*typedef long		Date; */
/*#else */
typedef double	Date;
/*#endif */

/* Common time manipulation constants */
#define		kMillisPerSecond	 (PR_INT32(1000))
#define		kMillisPerMinute	 (PR_INT32(60) * kMillisPerSecond)
#define		kMillisPerHour		 (PR_INT32(60) * kMillisPerMinute)
#define		kMillisPerDay		 (PR_INT32(24) * kMillisPerHour)


/*=====================================================================================*/
/* ClassID-based RTTI */
/*=====================================================================================*/

/**
 * ClassID is used to identify classes without using RTTI, since RTTI
 * is not yet supported by all C++ compilers.  Each class hierarchy which needs
 * to implement polymorphic clone() or operator==() defines two methods,
 * described in detail below.  ClassID values can be compared using operator==().
 * Nothing else should be done with them.
 *
 * getDynamicClassID() is declared in the base class of the hierarchy as
 * a pure virtual.  Each concrete subclass implements it in the same way:
 *
 *		class Base {
 *		public:
 *			virtual ClassID getDynamicClassID() const = 0;
 *		}
 *
 *		class Derived {
 *		public:
 *			virtual ClassID getDynamicClassID() const
 *			{
 *				return Derived::getStaticClassID();
 *			}
 *		}
 *
 * Each concrete class implements getStaticClassID() as well, which allows
 * clients to test for a specific type.
 *
 *		class Derived {
 *		public:
 *			static ClassID getStaticClassID();
 *		private:
 *			static char fgClassID;
 *		}
 *
 *		// In Derived.cpp:
 *		ClassID Derived::getStaticClassID() { return (ClassID)&Derived::fgClassID; }
 *		char Derived::fgClassID = 0; // Value is irrelevant
 */

typedef void* ClassID;

/*=====================================================================================*/
/* DLL import-export API control */
/*=====================================================================================*/

/**
 * Control of DLL import/export.  We break the Analytic package into four DLLs;
 * the Utility, Format, Findword, and Collate DLLs.  Each DLL defines the appropriate
 * compile-time symbol (e.g., T_UTILITY_IMPLEMENTATION) when building.  Declarations
 * of exported API us the appropriate declarator (e.g., T_UTILITY_API).
 *
 * If new DLLs are added, this section must be expanded.
 */

#define T_EXPORT NLSAPI_CLASS_EXPORT
#define T_IMPORT NLSAPI_CLASS_IMPORT

#ifdef T_UTILITY_IMPLEMENTATION
 #define T_UTILITY_API  T_EXPORT
 #define T_COLLATE_API  T_IMPORT
 #define T_FORMAT_API   T_IMPORT
 #define T_FINDWORD_API T_IMPORT
#elif defined(T_FINDWORD_IMPLEMENTATION)
 #define T_UTILITY_API  T_IMPORT
 #define T_COLLATE_API  T_IMPORT
 #define T_FORMAT_API   T_IMPORT
 #define T_FINDWORD_API T_EXPORT
#elif defined(T_FORMAT_IMPLEMENTATION)
 #define T_UTILITY_API  T_IMPORT
 #define T_COLLATE_API  T_IMPORT
 #define T_FORMAT_API   T_EXPORT
 #define T_FINDWORD_API T_IMPORT
#elif defined(T_COLLATE_IMPLEMENTATION)
 #define T_UTILITY_API  T_IMPORT
 #define T_COLLATE_API  T_EXPORT
 #define T_FORMAT_API   T_IMPORT
 #define T_FINDWORD_API T_IMPORT
#else
 #define T_UTILITY_API  T_IMPORT
 #define T_COLLATE_API  T_IMPORT
 #define T_FORMAT_API   T_IMPORT
 #define T_FINDWORD_API T_IMPORT
#endif

/*=====================================================================================*/
/* ErrorCode */
/*=====================================================================================*/

/** Error code to replace exception handling */
enum _ErrorCode {
	ZERO_ERROR = 0,
	ILLEGAL_ARGUMENT_ERROR,			/* Start of codes indicating failure */
	MISSING_RESOURCE_ERROR,
	INVALID_FORMAT_ERROR,
	FILE_ACCESS_ERROR,
	INTERNAL_PROGRAM_ERROR,			/* Indicates a bug in the library code */
	MESSAGE_PARSE_ERROR,
	MEMORY_ALLOCATION_ERROR,        /* Memory allocation error */
    INDEX_OUTOFBOUNDS_ERROR,
    PARSE_ERROR,                    /* Equivalent to Java ParseException */
	USING_FALLBACK_ERROR = -128,	/* Start of information results (semantically successful) */
	USING_DEFAULT_ERROR
};


typedef int32 ErrorCode;


/* Use the following to determine if an ErrorCode represents */
/* operational success or failure. */
#ifdef NLS_CPLUSPLUS
inline t_bool SUCCESS(ErrorCode code) { return (t_bool)(code<=ZERO_ERROR); }
inline t_bool FAILURE(ErrorCode code) { return (t_bool)(code>ZERO_ERROR); }
#else
#define SUCCESS(x) ((x)<=ZERO_ERROR)
#define FAILURE(x) ((x)>ZERO_ERROR)
#endif

/*=====================================================================================*/
/* TPlatformUtilities */
/*=====================================================================================*/

#ifdef NLS_CPLUSPLUS
/**
 * Platform utilities isolates the platform dependencies of the libarary.  For
 * each platform which this code is ported to, these functions may have to
 * be re-implemented.
 */

#ifdef NLS_MAC
#pragma export on
#endif

class T_UTILITY_API TPlatformUtilities
{
public:
	/* Floating point utilities */
	static t_bool		isNaN(double);
	static t_bool		isInfinite(double);
	static double	getNaN();
	static double	getInfinity();

    /**
     * Time zone utilities
     *
     * Wrappers for C runtime library functions relating to timezones.
     * The t_tzset() function (similar to tzset) uses the current setting 
     * of the environment variable TZ to assign values to three global 
     * variables: daylight, timezone, and tzname. These variables have the 
     * following meanings, and are declared in &lt;time.h>.
     *
     *   daylight   Nonzero if daylight-saving-time zone (DST) is specified
     *              in TZ; otherwise, 0. Default value is 1.
     *   timezone   Difference in seconds between coordinated universal
     *              time and local time. E.g., -28,800 for PST (GMT-8hrs)
     *   tzname(0)  Three-letter time-zone name derived from TZ environment
     *              variable. E.g., "PST".
     *   tzname(1)  Three-letter DST zone name derived from TZ environment
     *              variable.  E.g., "PDT". If DST zone is omitted from TZ,
     *              tzname(1) is an empty string.
     *
     * Notes: For example, to set the TZ environment variable to correspond
     * to the current time zone in Germany, you can use one of the
     * following statements:
     *
     *   set TZ=GST1GDT
     *   set TZ=GST+1GDT
     *
     * If the TZ value is not set, t_tzset() attempts to use the time zone
     * information specified by the operating system. Under Windows NT
     * and Windows 95, this information is specified in the Control Panel’s
     * Date/Time application.
     */
	static void		t_tzset();
	static t_int32	t_timezone();
	static char*	t_tzname(int index);

	/* Get system time in seconds since the epoch start (GMT 1/1/70). */
	static t_int32	time();

    /* Return the default data directory for this platform.  See Locale. */
   static const char*  getDefaultDataDirectory();

    /* complete a relative path to a full pathname, and convert to platform-specific syntax. */
    /* The character seperating directories for the relative path is '|'.                    */
    static void pathnameInContext( char* fullname, t_int32 maxsize, char* relpath );

	/* Return the default locale ID string by querying ths system, or
	   zero if one cannot be found. */
	static const char*	getDefaultLocaleID();

	/**
	 * Retrun true if this platform is big-endian, that is, if the number
	 * 0x1234 is stored 0x12, 0x34 in memory.  Return false if this platform
	 * is little-endian, and is, if 0x1234 is stored 0x34, 0x12 in memory.
	 */
	static t_bool			isBigEndian();

    /*
     * Finds the least double greater than d (if positive == true),
     * or the greatest double less than d (if positive == false).
     * If NaN, returns same value.
     *
     * Does not affect floating-point flags,
     */
    static double           nextDouble(double d, t_bool positive);

    inline static t_uint8   mapHostTo8859_1(t_uint8 hostChar);
    inline static t_uint8   map8859_1toHost(t_uint8 latin1char);

private:
	static t_bool			fgIsEndianismDetermined;
	static t_bool			fgIsBigEndian;

	/**
	 * Return a pointer to the top N bytes of the given double.  Used
	 * internally by the NaN- and Infinity-related functions.
	 */
    static char*        topNBytesOfDouble(double* d, int n);

    static t_uint8      EBCDICtoLatin1[];
    static t_uint8      Latin1toEBCDIC[];

};

#ifdef NLS_MAC
#pragma export off
#endif

inline char*
TPlatformUtilities::topNBytesOfDouble(double* d, int n)
{
	return TPlatformUtilities::isBigEndian() ?
		(char*)d :
		(char*)(d + 1) - n;
}

#endif

/*=====================================================================================*/
/* Array copy utility functions */
/*=====================================================================================*/

#ifdef NLS_CPLUSPLUS
inline void arrayCopy(const double* src, double* dst, t_int32 count)
	{ memcpy(dst, src, (size_t)(count * sizeof(*src))); }

inline void arrayCopy(const double* src, t_int32 srcStart, double* dst, t_int32 dstStart, t_int32 count)
	{ memcpy(dst+dstStart, src+srcStart, (size_t)(count * sizeof(*src))); }

inline void arrayCopy(const t_bool* src, t_bool* dst, t_int32 count)
	{ memcpy(dst, src, (size_t)(count * sizeof(*src))); }

inline void arrayCopy(const t_bool* src, t_int32 srcStart, t_bool* dst, t_int32 dstStart, t_int32 count)
	{ memcpy(dst+dstStart, src+srcStart, (size_t)(count * sizeof(*src))); }
#endif

/*------------------------------------------------------------------------------*/
/* Host character set mappings                                                  */
/*                                                                              */
/* We provide a mapping facility for 8-bit host character sets.  We supply an   */
/* implementation for host EBCDIC, which we map to 8859-1 (Latin1).  The source */
/* of our data is RFC 1345; we use the charset IBM297 (aka ebcdic-cp-fr) as our */
/* standard EBCDIC.                                                             */
/*------------------------------------------------------------------------------*/
 
#ifdef NLS_CPLUSPLUS

inline t_uint8
TPlatformUtilities::mapHostTo8859_1(t_uint8 hostChar)
{
#if USES_ISO_8859_1 
    return hostChar;
#elif USES_EBCDIC
    return EBCDICtoLatin1[hostChar];
#endif
}
 
inline t_uint8
TPlatformUtilities::map8859_1toHost(t_uint8 latin1char)
{
#if USES_ISO_8859_1 
    return latin1char;
#elif USES_EBCDIC
    return Latin1toEBCDIC[latin1char];
#endif
}

#endif   /* NLS_CPLUSPLUS */
/*=====================================================================================*/
/* Debugging */
/*=====================================================================================*/

/* This function is useful for debugging; it returns the text name */
/* of an ErrorCode result.  This is not the most efficient way of */
/* doing this but it's just for Debug builds anyway. */
#if defined(_DEBUG) && defined(NLS_CPLUSPLUS)
inline const char* errorName(ErrorCode code)
{
	switch (code)
	{
	case ZERO_ERROR:				return "ZERO_ERROR";
	case ILLEGAL_ARGUMENT_ERROR:	return "ILLEGAL_ARGUMENT_ERROR";
	case MISSING_RESOURCE_ERROR:	return "MISSING_RESOURCE_ERROR";
	case INVALID_FORMAT_ERROR:		return "INVALID_FORMAT_ERROR";
	case FILE_ACCESS_ERROR:			return "FILE_ACCESS_ERROR";
	case INTERNAL_PROGRAM_ERROR:	return "INTERNAL_PROGRAM_ERROR";
	case MESSAGE_PARSE_ERROR:		return "MESSAGE_PARSE_ERROR";
	case MEMORY_ALLOCATION_ERROR:   return "MEMORY_ALLOCATION_ERROR";
	case USING_FALLBACK_ERROR:		return "USING_FALLBACK_ERROR";
	case USING_DEFAULT_ERROR:		return "USING_DEFAULT_ERROR";
    case PARSE_ERROR:               return "PARSE_ERROR";
	default:						return "[BOGUS ErrorCode]";
	}
}
#endif

/*
 Redefine deprecated classes.
 */
#ifdef NLS_CPLUSPLUS
class Collator;
typedef Collator Collation;
class RuleBasedCollator;
typedef RuleBasedCollator TableCollation;
class CollationKey;
typedef CollationKey SortKey;

class BreakIterator;
typedef BreakIterator TextBoundary;

#endif
#endif
