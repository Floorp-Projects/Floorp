/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1997                                                 *
*   (C) Copyright International Business Machines Corporation,  1997                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*
* File READERS.H
*
* Contains support classes for the resource-bundle code
* This file contains a group of support classes that are used by the resource-bundle code.
* These classes are as follows:
*   UnicodeStreamReader - A small wrapper class around istream that allows it to read either ASCII
*     or Unicode data.
*   UnicodeStreamWriter - A small wrapper class around ostream that allows it to write Unicode data
*   ResourceFormatReader - A class that parses the low-level special characters in our resource
*     definition file format
*
* @author		Richard Gillam
*
* Modification History:
*
*	Date		Name		Description
*	3/4/97		aliu		Modified to support more efficient DataSink class as
*							an alternative to ostream objects.
*	3/13/97		aliu		Added getNextToken() and supporting methods to enable
*							tokenization and parsing of file from front to back.
*	3/18/97		aliu		Changed getNextToken() to getSingleToken() and wrote
*							a new getNextToken() which merges adjacent strings.
*	3/20/97		aliu		Removed obsolete classes to read tagged and comma-
*							delimited lists (now handled by getNextToken()), and
*							commented out unused classes UnicodeStreamWriter and
*							UnicodeDataSinkWriter.
*	3/25/97		aliu		Cleaned up code.
*
*****************************************************************************************
*/

#ifndef _READERS
#define _READERS

#ifndef _PTYPES
#include "ptypes.h"
#endif

//#include "datasink.h"
#include <stdio.h>
class UnicodeString;

enum {
	kNoErr		= 0,
	kEofOnRead,
	kEofOnWrite,
	kItemNotFound
};

//========================================================================================
// UnicodeStreamReader
//========================================================================================
/** Wrapper around istream for reading Unicode data
 *  This class wraps an istream and allows us to read Unicode character data.  The stream
 *  may actually be in either ASCII or Unicode format, but this class always returns
 *  Unicode characters.  The caller can pass the following values for "format":
 *    kASCII - Incoming data is ASCII; zero-pad everything out to 16 bits to get Unicode
 *    kBigEndianUnicode - Incoming data is Unicode, and the most significant byte
 *      of each character comes first
 *    kLittleEndianUnicode - Incoming data is Unicode, and the least significant byte
 *      of each character comes first
 *    kAuto - Infer the character format from the incoming data.  This relies on the
 *      "official" Unicode text file format:  A file containing Unicode starts with the
 *      Unicode byte order mark ($FEFF).  If we read something else, the file is assumed to
 *      be ASCII.  If it's $FEFF or $FFFE, we know it's Unicode and can infer the byte
 *      ordering we need to use.
 *    kDefault - Incoming data is Unicode, and whatever byte ordering the system we're
 *      running on uses internally is the byte ordering we're using (used for memory streams).
 */

#ifdef NLS_MAC
#pragma export on
#endif

class UnicodeStreamReader {
	public:
		enum CharFormat {
			kASCII,
			kBigEndianUnicode,
			kLittleEndianUnicode,
			kAuto,
			kDefault
		};

                                    UnicodeStreamReader(    FILE*       stream,
                                                            CharFormat  format);
									~UnicodeStreamReader();

		void						reset();
									
		UniChar						get(short&	err);
		void						putback(UniChar		theChar,
											short		err = kNoErr);


		enum Endian {
			kBig,
			kLittle,
			kUnknown
		};

	protected:
		static Endian				fgEndian;

	private:
		static void					determineEndianism();

		FILE*						fStream;
		CharFormat					fFormat;
		UniChar						fPutback;
};

//========================================================================================
// ResourceFormatReader
//========================================================================================
/**
 * Class for reading information from a file in our resource-definition format.
 * This takes care of interpreting (and when necessary disregarding) the extra stuff
 * we allow people to put into the file to make it human-readable.  The special characters
 * we allow in resource files are as follows:
 *   / * Begins a comment, which is terminated by * / (The spaces in these tokens aren't
 *		really there; I inserted them to keep the C++ compiler from seeing them as
 *		comment delimiters itself; this is standard C++/Java comment syntax).  These
 *		comments do not nest.
 *   // Begins a comment that terminates at the end of the line.
 *   "  begins and ends a quoted string.  Within a quoted string characters that would
 *      otherwise have special meaning (except for backslash escape sequences) don't.
 *   \  Begins an escape sequence.  The following escape sequences are possible:
 *      \n     Line feed
 *      \t     Tab
 *      \x##   ASCII (Latin1) character.  May be followed by one or two hex digits that
 *             specify the actual character value (if there are no hex digits, or if
 *             the value would be 0, the \x sequence is ignored)
 *      \u#### Unicode character.  May be followed by up to four hex digits that specify
 *             the actual character value (if there are no hex digits, or if the value
 *             would be 0, the \u sequence is ignored)
 *      \      Backslash before any other character deprives that character of a special
 *             meaning, if it had a special meaning.  Thus, \\ represents a backslash,
 *             and \" can be used to put a quote into a quoted string.
 * In addition, whitespace characters (spaces, tabs, line feeds, carriage returns, and
 * Unicode paragraph separators) are ignored, unless they occur within quoted strings.
 * Adjacent string literals are merged together, with a single interveing space, unless
 * both are quoted strings, in which case no space is added between them.
 */
class ResourceFormatReader {
public:
                                ResourceFormatReader(   FILE*                           stream,
                                                        UnicodeStreamReader::CharFormat format);
								~ResourceFormatReader();

	/**
	 * The types of tokens which may be returned by getNextToken.
	 */
	enum ETokenType
	{
		kString,				// A string token, such as "MonthNames"
		kOpenBrace,				// An opening brace character
		kCloseBrace,			// A closing brace character
		kComma,					// A comma

		kEOF,					// End of the file has been reached successfully
		kError,					// An error, such an unterminated quoted string
		kTokenTypeCount = 4		// Number of "real" token types
	};

	/**
	 * Read and return the next token from the stream.  If the token is
	 * of type kString, fill in the stringToken parameter with the
	 * token.  If the token is kError, then the err parameter will contain
	 * the specific error.  This will be kItemNotFound at the end of file,
	 * indicating that all tokens have been returned.  This method will
	 * never return kString twice in a row; instead, multiple adjacent string
	 * tokens will be merged into one, with a single intervening space, unless
	 * both token are quoted strings, in which case no intervening space is
	 * added.
	 *
	 * @param stringToken	Fill in parameter to receive value of string
	 *						token, if the return value is kString.
	 * @param err			Fill in parameter to receive error code,
	 *						if the return value is kError.  After the
	 *						last token is returned, this will be set to
	 *						kItemNotFound, and kError will be returned.
	 *						Any other value indicates an abnormal error.
	 * @return				The type of the next token.  This will be either
	 *						kString, kOpenBrace, kCloseBrace, kComma, or
	 *						kError.  It will never be kNull.
	 */
	ETokenType					getNextToken(	UnicodeString&	stringToken,
												short&			err);

	/**
	 * Reset to the start of the input stream.  After calling this method,
	 * the next call to getNextToken() will return the first token in the
	 * stream (if there is one).
	 */
	void						reset();

protected:
	/**
	 * Retrieve the next character, ignoring comments.  If skipwhite is true,
	 * whitespace is skipped as well.
	 */
	UniChar						getNextChar(t_bool skipwhite, short& err);

	ETokenType					getStringToken(UniChar			initialChar,
											   UnicodeString&	stringToken,
											   short&			err);

	void						seekUntilNewline(short&	err);
	
	void						seekUntilEndOfComment(short& err);

	UniChar						convertEscapeSequence(short& err);

	static t_bool					isWhitespace(UniChar c);

	static t_bool					isNewline(UniChar c);

	static t_bool					isHexDigit(UniChar c);

	// Special characters we recognize during processing
	static const UniChar		kOPENBRACE;
	static const UniChar		kCLOSEBRACE;
	static const UniChar		kCOMMA;
	static const UniChar		kQUOTE;
	static const UniChar		kESCAPE;
	static const UniChar		kSLASH;
	static const UniChar		kASTERISK;
	static const UniChar		kSPACE;

	UnicodeStreamReader			fReader;
};

#ifdef NLS_MAC
#pragma export off
#endif

#endif
