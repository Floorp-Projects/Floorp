/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1997                                                 *
*   (C) Copyright International Business Machines Corporation,  1996                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*
*  FILE NAME : unistring.h
*
*	Modification History:
*
*	Date		Name		Description
*	02/05/97	aliu		Added UnicodeString streamIn and streamOut methods.
*	03/26/97	aliu		Added indexOf(UniChar,).
*	04/24/97	aliu		Numerous changes per code review.
*   05/06/97    helena      Added isBogus().
*****************************************************************************************
*/

#ifndef _UNISTRING
#define _UNISTRING

#include <limits.h>
#include <stdlib.h>
#include <iostream.h>
#include <stdio.h>
#include "ptypes.h"
class Locale;

/**
 * Simple Unicode string class. This is a simple class that encapsulates a
 * Unicode string, allowing the user to manipulate it and allowing it to grow
 * and shrink without the user having to worry about this.
 * <P>
 * The char* interfaces on this class work with either the Latin1 (ISO 8859-1)
 * character set or a host character set.  The host character set may be any
 * 8-bit character set for which TPlatformUtilities::mapHostTo8859_1() and
 * TPlatformUtilities::map8859_1ToHost() have been defined; the default
 * implementation maps to and from EBCDIC as defined in RFC 1345.  If the
 * host character set is used, then incoming characters are mapped to Unicode,
 * and outgoing characters are mapped back to the host character set.
 * <P>
 * All inbound transcoding of char* data is done by zero-extending the incoming
 * characters, and all outbound transcoding is done by truncating the top byte
 * from the characters.
 */
#ifdef NLS_MAC
#pragma export on
#endif

class T_UTILITY_API UnicodeString {
	public:
       /**
         * Standard operator new.  This function is only provided because the
         * special operator new would otherwise hide it.  This function just
         * turns around and calls the global operator new function.
         */
        void*                       operator new(size_t size);

        /**
         * Placement new.  This version of operator new just returns the "location"
         * parameter unchanged as its result.  It ignores the "size" parameter.
         * This function is here only to allow stack allocation of UnicodeStrings
         * through the C wrapper interface.  DO NOT CALL THIS FUNCTION FROM C++
         * UNLESS YOU'RE SURE YOU KNOW WHAT YOU'RE DOING!
         * @param size Ignored.  There's no way this function can check the size
         *             of the block you pass to it.  This function trusts you've
         *             allocated enough space at that location to hold a Unicode-
         *             String object.
         * @param location The location where you want the new UnicodeString to
         *                 be stored.  Typically this will be a local variable on
         *                 the stack.  This function trusts that there's enough
         *                 location to hold a UnicodeString object.
         * @return Whatever was passed in for "location".
         */
        void*                       operator new(size_t size, void* location);

									UnicodeString();
									UnicodeString(const UnicodeString&	that);
									UnicodeString(const UniChar*	that);
									UnicodeString(const UniChar*	that,
														t_int32		thatLength);
									UnicodeString(const char*	that); // Must be null-terminated

		/**
         * External-buffer constructor.  This constructor allows UnicodeString to
         * use storage provided by the client as its character buffer, rather than
         * allocating its own storage.  The client passes a pointer to the storage,
         * along with the number of characters currently stored in it (we don't
         * use null termination to determine the string length, and the string is
         * not ever guaranteed to be null-terminated) and the number of characters
         * the storage is capable of holding.
         * <P>
         * WARNING:  Do not change the characters in the buffer during the period
         *           that the UnicodeString it active.  Doing so may lead to
         *           undefined results.
         * <P>
         * WARNING:  If the string grows beyond the capacity of the buffer passed
         *           to this constructor, UnicodeString will allocate its own storage,
         *           and no subsequent changes to the UnicodeString will be reflected
         *           in the buffer passed to this constructor (UnicodeString itself
         *           will continue to work right, however.
         * <P>
         * WARNING:  The string stored in the client-owned buffer is never guaranteed
         *           to be null-terminated.
         * @param charBuffer    A pointer to a range of storage that the new UnicodeString
         *                      should use as its character-storage buffer.  The client
         *                      retains responsibility for deleting this storage after
         *                      the UnicodeString goes away.
         * @param numCharsInBuffer  The number of characters currently stored in charBuffer.
         * @param bufferCapabity    The number of characters the buffer if capable of
         *                          holding.  This must be greater than or equal to
         *                          numCharsInBuffer, but this isn't checked.
         */
                                    UnicodeString(UniChar*  charBuffer,
                                                  t_int32   numCharsInBuffer,
                                                  t_int32   bufferCapacity);


		/* Creates a UnicodeString from a given const char* buffer and an
		 * encoding name.
		 * Netscape added method.
		 * <P>
		 * @param that		A null-terminated char buffer in a given encoding
		 * @param encoding	name for the encoding used for buffer
		 *
		 */
								
									UnicodeString(const char*	that, 
														const char* encoding);
                                    ~UnicodeString() { if (!fClientOwnsStorage)
                                                         delete [] fChars; }
									
        UnicodeString&              operator=(const UnicodeString&  that);
        
        /**
         * Compares a UnicodeString to something else.  All versions of compare()
         * do bitwise comparison; internationally-sensitive comparison requires
         * the Collation library. The offset and length parameters are pinned to
         * permissible values if they are out of range.
         */
        t_int8                      compare(const UnicodeString&    that) const;
        t_int8                      compare(TextOffset              thisOffset,
                                            t_int32                 thisLength,
                                            const UnicodeString&    that,
                                            TextOffset              thatOffset,
                                            t_int32                 thatLength) const;
        t_int8                      compare(const UniChar*          that) const; // Must be null-terminated
        t_int8                      compare(const UniChar*          that,
                                            t_int32                 thatLength) const;
        t_int8                      compare(const char*             that) const;

        /**
         * Compares substrings of two UnicodeStrings. Same as compare(), but
         * takes starting and ending offsets instead of starting offsets and
         * character counts. The characters from the starting offset up to, but
         * not including the ending offset are compared. The start and limit
         * parameters are pinned to permissible values if they are out of range.
         */
		t_int8						compareBetween(	TextOffset				thisStart,
													TextOffset				thisLimit,
													const UnicodeString&	that,
													TextOffset				thatStart,
													TextOffset				thatLimit) const;

		/**
		 * Comparison operators.  All of these operators map through to compare().
		 */
		t_bool						operator==(const UnicodeString&	that) const;
		t_bool						operator!=(const UnicodeString&	that) const;
		t_bool						operator>(const UnicodeString&	that) const;
		t_bool						operator<(const UnicodeString&	that) const;
		t_bool						operator>=(const UnicodeString&	that) const;
		t_bool						operator<=(const UnicodeString&	that) const;

        /**
         * Returns the offset within this String of the first occurrence of the
         * specified substring "that". The search begins with the character at fromIndex
	 * and examines at most forLength characters.  Returns -1 if "that" is not found.
         */
		TextOffset					indexOf(const UnicodeString&	that,
											TextOffset				fromOffset = 0,
											t_uint32				forLength = -1) const;
		TextOffset					indexOf(UniChar					character,
											TextOffset				fromOffset = 0,
											t_uint32				forLength = -1) const;
        /**
         * Returns the offset within this String of the last occurrence of the
         * specified substring "that". The search begins with the character before fromOffset
	 * and examines at most forLength characters (moving backward from fromOffset).
	 * Returns -1 if "that" is not found.
         */
		TextOffset					lastIndexOf(const UnicodeString&	that,
												TextOffset				fromOffset = T_INT32_MAX,
												t_uint32				forLength = -1) const;
		TextOffset					lastIndexOf(UniChar					character,
												TextOffset				fromOffset = T_INT32_MAX,
												t_uint32				forLength = -1) const;

		
		/**
		 * Returns true if "that" appears in its entirety at the beginning of "this"
		 */
		t_bool						startsWith(const UnicodeString&	that) const;
		
		/**
		 * Returns true if "that" appears in its entirety at the end of "this"
		 */
		t_bool						endsWith(const UnicodeString&	that) const;
												
		/**
		 * Stores in "that" a copy of "this" that has had leading and trailing whitespace
		 * removed from it.  "this" itself is unaffected.
		 */
		UnicodeString&				trim(UnicodeString& that) const;

        /**
         * Trims leading and trailing whitespace from this UnicodeString.
         */
		void						trim();

		/**
		 * If the string is shorter than targetLength, adds enough copies of padChar to the
		 * beginning to make the length targetLength and returns true; otherwise returns false.
		 */
		t_bool						padLeading(	t_int32	targetLength,
												UniChar	padChar = ' ');

		/**
		 * If the string is shorter than targetLength, adds enough copies of padChar to the
		 * end to make the length targetLength and returns true; otherwise returns false.
		 */
		t_bool						padTrailing(t_int32	targetLength,
												UniChar	padChar = ' ');

		/**
		 * If the string is longer than targetLength, deletes enough characters from the
		 * end to make the length targetLength and returns true; otherwise returns false.
		 */
		t_bool						truncate(t_int32	targetLength);


        /**
         * Allows UnicodeString to be used with interfaces that use UniChar*.
         * Returns a pointer to the UnicodeString's internal storage. This
         * storage is still owned by the UnicodeString, and the caller is not
         * allowed to change it. The string returned by this function is
         * correctly null-terminated.
         */
									operator const UniChar*() const;

        /**
         * Extracts the characters from a UnicodeString without copying. Returns
         * a pointer to the UnicodeString's internal storage. The caller
         * acquires ownership of this storage and is responsible for deleting
         * it. The UnicodeString is set to empty by this operation. WARNING: The
         * string returned is not null-terminated unless the caller explicitly
         * adds a null character to the end with operator+=().
         */
		UniChar*					orphanStorage() ;

        /**
         * Extracts a substring.  Extracts the specified substring of the
         * UnicodeString into the storage referred to by extractInto. The offset
         * and length parameters are pinned to permissible values if they are
         * out of range.
         * <P>
         * NOTE: No null byte is written to UniChar* extractInto. If you want
         * extractInto to have a null-terminated string you should do
         * extractInto[len]=0, where len is the actual number of characters
         * extracted.
         */
		UnicodeString&				extract(	TextOffset		thisOffset,
												t_int32			thisLength,
												UnicodeString&	extractInto) const;
		void						extract(	TextOffset		thisOffset,
												t_int32			thisLength,
												UniChar*		extractInto) const;

        /**
         * This version of extract() extracts into an array of char. The
         * characters are converted from UniChar to char by truncating the
         * high-order byte (in other words, this function assumes the Unicode
         * data being converted is all from the Latin1 character set). The
         * offset and length parameters are pinned to permissible values if they
         * are out of range.
         * <P>
         * NOTE: No null byte is written. If you want extractInto to have a
         * null-terminated string you should do extractInto[len]=0, where len is
         * the actual number of characters extracted.
         */
		void						extract(	TextOffset		thisOffset,
												t_int32			thisLength,
												char*			extractInto) const;

        /**
         * Extract a substring. Same as extract(), but the substring is
         * specified as starting and ending offsets [start, limit). That is,
         * from the starting offset up to, but not including, the ending offset.
         * The start and limit parameters are pinned to permissible values if
         * they are out of range.
         */
		UnicodeString&				extractBetween(	TextOffset		start,
													TextOffset		limit,
													UnicodeString&	extractInto) const;
		
        /**
         * Return the character at the given offset of this string. If the
         * offset is out of range, return 0 (for the const method) or a
         * reference to a UniChar having the value 0 (for the non-const method).
         */
		UniChar						operator[](TextOffset	offset) const;
		UniChar&					operator[](TextOffset	offset);
		
		/**
		 * Append a string or character.  The specfied string or character is added
		 * to the end of the string.
		 */
		UnicodeString&				operator+=(const UnicodeString&	that);
		UnicodeString&				operator+=(UniChar	that);
		
		/**
		 * Insert a string.  The contents of "that" are inserted into *this so that
		 * the first character from "that" occurs at thisOffset.  If thisOffset is out
		 * of range, the new characters are added at the end.
		 */
		UnicodeString&				insert(	TextOffset				thisOffset,
											const UnicodeString&	that);
		
        /**
         * Remove part of this string. remove() with no arguments removes all
         * characters of this string. Note: The storage is not removed, but the
         * logical length, and possibly the contents, are altered.
         */
		UnicodeString&				remove();
		UnicodeString&				remove(	TextOffset	offset,
											t_int32		length = T_INT32_MAX);

        /**
         * Delete characters. Same as remove(), but the range of characters to
         * delete is specified as a pair of starting and ending offsets [start,
         * limit), rather than a starting offset and a character count. That is,
         * from the starting offset up to, but not including, the ending offset.
         * The start and limit parameters are pinned to permissible values if
         * they are out of range.
         */
		UnicodeString&				removeBetween(	TextOffset	start = 0,
													TextOffset	limit = T_INT32_MAX);

		/**
		 * Replace characters.  Replaces the characters in the range specified by
		 * thisOffset and thisLength with the characters in "that" (or the specfied
		 * subrange of "that").  All parameters are pinned to permissible values
		 * if necessary.  If the source and replacement text are different lengths,
		 * the string will be lengthened or shortened as necessary.
		 */
		UnicodeString&				replace(	TextOffset				thisOffset,
												t_int32					thisLength,
												const UnicodeString&	that,
												TextOffset				thatOffset = 0,
												t_int32					thatLength = T_INT32_MAX);
		UnicodeString&				replace(	TextOffset		thisOfset,
												t_int32			thisLength,
												const UniChar*	that);
		UnicodeString&				replace(	TextOffset		thisOffset,
												t_int32			thisLength,
												const UniChar*	that,
												t_int32			thatLength);

		UnicodeString&				replace(	TextOffset	thisOffset,
												t_int32		thisLength,
												const char*	that);

        /**
         * Replace characters. Same as replace(), but the affected subranges are
         * specified as pairs of starting and ending offsets [start, limit)
         * rather than starting offsets and lengths. That is, from the starting
         * offset up to, but not including, the ending offset. The start and
         * limit parameters are pinned to permissible values if they are out of
         * range.
         */
		UnicodeString&				replaceBetween(	TextOffset				thisStart,
													TextOffset				thisLimit,
													const UnicodeString&	that,
													TextOffset				thatStart = 0,
													TextOffset				thatLimit = T_INT32_MAX);
		
 		/**
		 * Replaces all occurrences of "oldText" in the string in the range defined by
		 * fromOffset and forLength with "newText".
		 */
		void						findAndReplace( const UnicodeString&	oldText,
													const UnicodeString&	newText,
													TextOffset				fromOffset = 0,
													t_uint32				forLength = -1);
        /**
         * Reverse the characters in this string in place. That is, "abcd"
         * becomes "dcba". Return a reference to this string.
         */
		UnicodeString&				reverse();
		UnicodeString&				reverse(TextOffset from,
                                            TextOffset to);

        /**
         * Convert this string to uppercase or lowercase. The methods which take
         * no arguments use the default Locale. (These methods cannot take a
         * default argument of Locale::getDefault() because that would create a
         * circular class dependency between UnicodeString and Locale.)
         */
		UnicodeString&				toUpper();
		UnicodeString&				toUpper(const Locale& locale);
		UnicodeString&				toLower();
		UnicodeString&				toLower(const Locale& locale);
		
        /**
         * Return the length of this string. This will always be a non-negative
         * number.
         */
		t_int32						size() const;

        /**
         * Return the hash code for this string. This is used by hash tables
         * which use this object as a key. The hash code is cached, and
         * recomputed when necessary. For this reason, this method may alter the
         * physical object, even though it is semantically const.
         */
		t_int32						hashCode() const;
		
	    /**
         * Returns the number of display cells the specified substring takes up.
         * This function is designed for Asian text and properly takes into account
         * halfwidth and fullwidth variants of various CJK characters and the combining
         * behavior of the Hangul Jamo characters (with some limitations; see
         * documentation for Unicode::getCellWidth()).
         * <P>
         * In order to avoid dealing
         * with fractions, this function can either be construed to return twice the
         * actual number of display cells or to treat a "cell" as the width of a halfwidth
         * character rather than the width of a fullwidth character.
         * <P>
         * The "asian" parameter controls whether characters considered NEUTRAL by
         * the Unicode class are treated as halfwidth or fullwidth here.  If you set
         * "asian" to FALSE, neutrals are treated as halfwidth, and this function returns
         * a close approximation of how many Latin display cells the text will take up
         * in a monospaced font.
		 */
		t_int32						numDisplayCells(TextOffset	fromOffset = 0,
													t_int32		forLength = T_INT32_MAX,
													t_bool		asian = TRUE) const;
       /**
         * The streamIn and streamOut methods read and write objects of this
         * class as binary, platform-dependent data in the iostream. The stream
         * must be in ios::binary mode for this to work. These methods are not
         * intended for general public use; they are used by the framework to
         * improve performance by storing certain objects in binary files.
         */
		void                        streamOut(FILE* os) const;
        void                        streamIn(FILE* is);

		/**
		 * Returns TRUE if the string resize failed.  It is very important
		 * to check if a unicode string is valid after modification.  
		 */
		t_bool						isBogus() const;

	/*
	 * Additional Netscape routines
	 */
		/** Converts the String to a char* using a target encoding */
		char*						toCString(const char* encoding) const;

		/** Compare case insensitive. Still diacrit sensitive. Is not locale sensitive.
		 *	All versions of compare() do bitwise comparison; internationally-
		 *	sensitive comparison requires the Collation library. */
		int							compareIgnoreCase(const UnicodeString&	that) const;
		int							compareIgnoreCase(const UniChar*	that, 
												t_int32 thatLength) const;
		int							compareIgnoreCase(const UniChar*	that) const;
		int							compareIgnoreCase(const char*	that, 
												const char* encoding) const;
		/* Assumes a LATIN-1 string */
		int	
									compareIgnoreCase(const char*	that) const;


	private:
		/* Netscape Private */
		char*						toCStringTruncate() const;

		static t_int32				lengthOf(const UniChar*	chars);
		static t_int32				lengthOf(const char*	chars);
		void						resize(t_int32 	newLength);
		void						setToBogus(void);
		static void					copy(	const UniChar*	from,
											UniChar*		to,
											t_int32			numChars);
		static void					copy(	const char*	from,
											UniChar*	to,
											t_int32		numChars);
		static void					copy(	const UniChar*	from,
											char*			to,
											t_int32			numChars);

		t_int8						doCompare(	const UniChar*	thiss,
												t_int32			thisLength,
												const UniChar*	that,
												t_int32			thatLength) const;
		static const t_int32		kInvalidHashCode;
		static const t_int32		kEmptyHashCode;
		static UniChar				fgErrorChar;
	
		UniChar*					fChars;
		t_int32						fSize;
		t_int32						fCapacity;
		t_int32						fHashCode;
        t_bool                      fClientOwnsStorage;
		t_bool						fBogus;
};

#ifdef NLS_MAC
#pragma export off
#endif

/**
 * Write the contents of a UnicodeString to an ostream. This functions writes
 * the characters in a UnicodeString to an ostream. The UniChars in the
 * UnicodeString are truncated to char, leading to undefined results with
 * anything not in the Latin1 character set.
 */
NLSUNIAPI_PUBLIC(ostream&) operator<<(ostream&				stream,
							  const UnicodeString&	string);

//----------------------------------------------------
// operator new
//----------------------------------------------------

inline void*
UnicodeString::operator new(size_t size)
{
    return ::operator new(size);
}

inline void*
UnicodeString::operator new(size_t size, void* location)
{
    // WARNING: Do not use this operator unless you're sure you know what you're
    // doing!  It just passes "location" through blindly.  If there isn't enough
    // free space at "location" to hold a UnicodeString (or if "location" is
    // somehow invalid), you're in trouble!
    return location;
}
//----------------------------------------------------
// Fast append
//----------------------------------------------------

inline UnicodeString&
UnicodeString::operator+=(UniChar	that)
{
	if (fSize < fCapacity) {
		fChars[fSize++] = that;
		fHashCode = kInvalidHashCode;
	} else {
		resize(fSize + 1);
		if (!fBogus)                   // change required for HP-UX         
		 fChars[fSize - 1] = that;
	} 
	return *this;
}

//----------------------------------------------------
// Character access
//----------------------------------------------------

inline UniChar
UnicodeString::operator[](TextOffset	offset) const
{
	// Cast to unsigned in order to detect negative values.
	// Assume fSize >= 0.
	return ((t_uint32)offset < (t_uint32)fSize) ? fChars[offset] : 0;
}

inline UniChar&
UnicodeString::operator[](TextOffset	offset)
{
	// Cast to unsigned in order to detect negative values
	// Assume fSize >= 0.
	
	UniChar& result = fgErrorChar;
	if ((t_uint32)offset < (t_uint32)fSize)
	{
		fHashCode = kInvalidHashCode;
		result =  fChars[offset];
	} else
	{
	        fgErrorChar = 0; // Always reset this to zero in case the caller has modified it
		result = fgErrorChar;
	}
	
	return result;
}

//----------------------------------------------------
// Other inline methods
//----------------------------------------------------

inline UnicodeString&
UnicodeString::remove()
{
	fSize = 0;
	fBogus = FALSE;
	return *this;
}

inline t_int32
UnicodeString::size() const
{
	return fSize;
}

inline t_int8
UnicodeString::compare(const UnicodeString& that) const
{
    return doCompare(fChars, fSize, that.fChars, that.fSize);
}

inline t_bool
UnicodeString::operator==(const UnicodeString&  that) const
{
    // Check fSize first to avoid the call to compare in many cases
    return fSize == that.fSize && compare(that) == 0;
}


inline t_bool
UnicodeString::operator!=(const UnicodeString&	that) const
{
	return compare(that) != 0;
}

inline t_bool
UnicodeString::operator>(const UnicodeString&	that) const
{
	return compare(that) == 1;
}

inline t_bool
UnicodeString::operator<(const UnicodeString&	that) const
{
	return compare(that) == -1;
}

inline t_bool
UnicodeString::operator<=(const UnicodeString&	that) const
{
	return compare(that) != 1;
}

inline t_bool
UnicodeString::operator>=(const UnicodeString&	that) const
{
	return compare(that) != -1;
}

inline t_bool
UnicodeString::isBogus() const { return fBogus; }

/**
 * The arrayCopy() methods copy an array of UnicodeString OBJECTS (not
 * pointers).
 */
inline void arrayCopy(const UnicodeString* src, UnicodeString* dst, t_int32 count)
	{ while (count-- > 0) *dst++ = *src++; }

inline void arrayCopy(const UnicodeString* src, t_int32 srcStart, UnicodeString* dst, t_int32 dstStart, t_int32 count)
	{ arrayCopy(src+srcStart, dst+dstStart, count); }

#endif
