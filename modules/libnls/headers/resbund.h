/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc., 1996, 1997                                            *
*   (C) Copyright International Business Machines Corporation, 1996, 1997               *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*
* File resbund.h
*
*	CREATED BY
*		Richard Gillam
*
* Modification History:
*
*	Date		Name		Description
*	2/5/97		aliu		Added scanForLocaleInFile.  Added
*							constructor which attempts to read resource bundle
*							from a specific file, without searching other files.
*	2/11/97		aliu		Added ErrorCode return values to constructors.  Fixed
*							infinite loops in scanForFile and scanForLocale.
*							Modified getRawResourceData to not delete storage in
*							localeData and resourceData which it doesn't own.
*							Added Mac compatibility #ifdefs for tellp() and
*							ios::nocreate.
*	2/18/97		helena		Updated with 100% documentation coverage.
*	3/13/97		aliu		Rewrote to load in entire resource bundle and store
*							it as a Hashtable of ResourceBundleData objects.
*							Added state table to govern parsing of files.
*							Modified to load locale index out of new file distinct
*							from default.txt.
*	3/25/97		aliu		Modified to support 2-d arrays, needed for timezone data.
*							Added support for custom file suffixes.  Again, needed to
*							support timezone data.
*	4/7/97		aliu		Cleaned up.
*****************************************************************************************
*/

#ifndef _RESBUND
#define _RESBUND
  
#include <iostream.h>
#include "ptypes.h"
#include "unistring.h"
#include "locid.h"
class RBHashtable;
class ResourceBundleData;
class ResourceBundleCache;
class VisitedFileCache;
class Hashtable;

/**
 * A class representing a collection of resource information pertaining to a given
 * locale. A resource bundle provides a way of accessing locale- specfic information in
 * a data file. You create a resource bundle that manages the resources for a given
 * locale and then ask it for individual resources.
 * <P>
 * The resource bundle file is a text (ASCII or Unicode) file with the format:
 * <pre>
 * .   locale {
 * .      tag1 {...}
 * .      tag2 {...}
 * .   }
 * </pre>
 * The tags are used to retrieve the data later. You may not have multiple instances of
 * the same tag.
 * <P>
 * Four data types are supported. These are solitary strings, comma-delimited lists of
 * strings, 2-dimensional arrays of strings, and tagged lists of strings.
 * <P>
 * Note that all data is textual. Adjacent strings are merged by the low-level
 * tokenizer, so that the following effects occur: foo bar, baz // 2 elements, "foo
 * bar", and "baz" "foo" "bar", baz // 2 elements, "foobar", and "baz" Note that a
 * single intervening space is added between merged strings, unless they are both double
 * quoted. This extends to more than two strings in a row.
 * <P>
 * Whitespace is ignored, as in a C source file.
 * <P>
 * Solitary strings have the format:
 * <pre>
 * .   Tag { Data }
 * </pre>
 * This is indistinguishable from a comma-delimited list with only one element, and in
 * fact may be retrieved as such (as an array, or as element 0 or an array).
 * <P>
 * Comma-delimited lists have the format:
 * <pre>
 * .   Tag { Data, Data, Data }
 * </pre>
 * Parsing is lenient; a final string, after the last element, is allowed.
 * <P>
 * Tagged lists have the format:
 * <pre>
 * .   Tag { Subtag { Data } Subtag {Data} }
 * </pre>
 * Data is retrieved by specifying the subtag.
 * <P>
 * Two-dimensional arrays have the format:
 * <pre>
 * .   TwoD {
 * .       { r1c1, r1c2, ..., r1cm },
 * .       { r2c1, r2c2, ..., r2cm },
 * .       ...
 * .       { rnc1, rnc2, ..., rncm }
 * .   }
 * </pre>
 * where n is the number of rows, and m is the number of columns. Parsing is lenient (as
 * in other data types). A final comma is always allowed after the last element; either
 * the last string in a row, or the last row itself. Furthermore, since there is no
 * ambiguity, the commas between the rows are entirely optional. (However, if a comma is
 * present, there can only be one comma, no more.) It is possible to have zero columns,
 * as follows:
 * <pre>
 * .   Odd { {} {} {} } // 3 x 0 array
 * </pre>
 * But it is impossible to have zero rows. The smallest array is thus a 1 x 0 array,
 * which looks like this:
 * <pre>
 * .   Smallest { {} } // 1 x 0 array
 * </pre>
 * The array must be strictly rectangular; that is, each row must have the same number
 * of elements.
 */
 
#ifdef NLS_MAC
#pragma export on
#endif

class T_UTILITY_API ResourceBundle {
public:
    /**
     * Constructor
     *
     * @param path    This is a full pathname in the platform-specific format for the
     *                directory containing the resource data files we want to load
     *                resources from. We use locale IDs to generate filenames, and the
     *                filenames have this string prepended to them before being passed
     *                to the C++ I/O functions. Therefore, this string must always end
     *                with a directory delimiter (whatever that is for the target OS)
     *                for this class to work correctly.
     * @param locale  This is the locale this resource bundle is for. To get resources
     *                for the French locale, for example, you would create a
     *                ResourceBundle passing Locale::FRENCH for the "locale" parameter,
     *                and all subsequent calls to that resource bundle will return
     *                resources that pertain to the French locale. If the caller doesn't
     *                pass a locale parameter, the default locale for the system (as
     *                returned by Locale::getDefault()) will be used.
     * @param suffix  If not specified, the suffix is kDefaultSuffix, that is, ".txt".
     *                The caller may also specify a suffix explicitly, in which case
     *                files with that suffix will be read instead.
     *
     * The ErrorCode& err parameter is used to return status information to the user. To
     * check whether the construction succeeded or not, you should check the value of
     * SUCCESS(err). If you wish more detailed information, you can check for
     * informational error results which still indicate success. USING_FALLBACK_ERROR
     * indicates that a fall back locale was used. For example, 'de_CH' was requested,
     * but nothing was found there, so 'de' was used. USING_DEFAULT_ERROR indicates that
     * the default locale data was used; neither the requested locale nor any of its
     * fall back locales could be found.
     */
						ResourceBundle(	const UnicodeString&	path,
										const Locale&			locale,
										ErrorCode&				err);
						ResourceBundle(	const UnicodeString&	path,
										ErrorCode&				err);
						ResourceBundle(	const UnicodeString&	path,
										const Locale&			locale,
										const char*				suffix,
										ErrorCode&				err);
						ResourceBundle(	const UnicodeString&	path,
										const char*				suffix,
										ErrorCode&				err);
						~ResourceBundle();

    /**
     * Returns the contents of a string resource. Resource data is undifferentiated
     * Unicode text. The resource file may contain quoted strings or escape sequences;
     * these will be parsed prior to the data's return.
     *
     * @param resourceTag  The resource tag of the string resource the caller wants
     * @param theString    Receives the actual data in the resource
     * @param err          Set to MISSING_RESOURCE_ERROR if a resource with the
     *                     specified tag couldn't be found.
     */
	void				getString(	const UnicodeString&	resourceTag,
									UnicodeString&			theString,
									ErrorCode&				err) const;

    /**
     * Returns the contents of a string-array resource. This will return the contents of
     * a string-array (comma-delimited-list) resource as a C++ array of UnicodeString
     * objects. The number of elements in the array is returned in numArrayItems.
     * Calling getStringArray on a resource of type string will return an array with one
     * element; calling it on a resource of type tagged-array results in a
     * MISSING_RESOURCE_ERROR error.
     *
     * @param resourceTag    The resource tag of the string-array resource the caller
     *                       wants
     * @param numArrayItems  Receives the number of items in the array the function
     *                       returns.
     * @param err            Set to MISSING_RESOURCE_ERROR if a resource with the
     *                       specified tag couldn't be found.
     * @return               The resource requested, as a pointer to an array of
     *                       UnicodeStrings. The caller does not own the storage and
     *                       must not delete it.
     */
	const UnicodeString*	getStringArray(	const UnicodeString&	resourceTag,
											t_int32&				numArrayItems,
											ErrorCode&				err) const;

    /**
     * Returns a single item from a string-array resource. This will return the contents
     * of a single item in a resource of string-array (comma-delimited-list) type. If
     * the resource is not an array, a MISSING_RESOURCE_ERROR will be returned in err.
     *
     * @param resourceTag   The resource tag of the resource the caller wants to extract
     *                      an item from.
     * @param index         The index (zero-based) of the particular array item the user
     *                      wants to extract from the resource.
     * @param theArrayItem  Receives the actual text of the desired array item.
     * @param err           Set to MISSING_RESOURCE_ERROR if a resource with the
     *                      specified tag couldn't be found, or if the index was out of range.
     */
	void				getArrayItem(	const UnicodeString&	resourceTag,
										t_int32					index,
										UnicodeString&			theArrayItem,
										ErrorCode&				err) const;

    /**
     * Return the contents of a 2-dimensional array resource. The return value will be a
     * UnicodeString** array. (This is really an array of pointers; each pointer is a
     * ROW of the data.) The number of rows and columns is returned. If the resource is
     * of the wrong type, or not present, MISSING_RESOURCE_ERROR is placed in err.
     *
     * @param resourceTag  The resource tag of the string-array resource the caller
     *                     wants
     * @param rowCount     Receives the number of rows in the array the function
     *                     returns.
     * @param columnCount  Receives the number of columns in the array the function
     *                     returns.
     * @param err          Set to MISSING_RESOURCE_ERROR if a resource with the
     *                     specified tag couldn't be found.
     * @return             The resource requested, as a UnicodeStrings**. The caller
     *                     does not own the storage and must not delete it.
     */
	const UnicodeString**	get2dArray(const UnicodeString&	resourceTag,
									   t_int32&				rowCount,
									   t_int32&				columnCount,
									   ErrorCode&			err) const;

    /**
     * Return a single string from a 2-dimensional array resource. If the resource does
     * not exists, or if it is not a 2-d array, or if the row or column indices are out
     * of bounds, err is set to MISSING_RESOURCE_ERROR.
     *
     * @param resourceTag   The resource tag of the resource the caller wants to extract
     *                      an item from.
     * @param rowIndex      The row index (zero-based) of the array item the user wants
     *                      to extract from the resource.
     * @param columnIndex   The column index (zero-based) of the array item the user
     *                      wants to extract from the resource.
     * @param theArrayItem  Receives the actual text of the desired array item.
     * @param err           Set to MISSING_RESOURCE_ERROR if a resource with the
     *                      specified tag couldn't be found, if the resource data was in
	 *                      the wrong format, or if either index is out of bounds.
     */
	void				get2dArrayItem(const UnicodeString&	resourceTag,
									   t_int32				rowIndex,
									   t_int32				columnIndex,
									   UnicodeString&		theArrayItem,
									   ErrorCode&			err) const;

    /**
     * Returns a single item from a tagged-array resource This will return the contents
     * of a single item in a resource of type tagged-array. If this function is called
     * for a resource that is not of type tagged-array, it will set err to
     * MISSING_RESOUCE_ERROR.
     *
     * @param resourceTag   The resource tag of the resource the caller wants to extract
     *                      an item from.
     * @param itemTag       The item tag for the item the caller wants to extract.
     * @param theArrayItem  Receives the text of the desired array item.
     * @param err           Set to MISSING_RESOURCE_ERROR if a resource with the
     *                      specified resource tag couldn't be found, or if an item
	 *                      with the specified item tag coldn't be found in the resource.
     */
	void				getTaggedArrayItem(	const UnicodeString&	resourceTag,
											const UnicodeString&	itemTag,
											UnicodeString&			theArrayItem,
											ErrorCode&				err) const;

    /**
     * Returns a tagged-array resource.  The contents of the resource is returned as two
	 * separate arrays of UnicodeStrings, the addresses of which are placed in "itemTags"
	 * and "items".  After calling this function, the items in the resource will be in the
	 * list pointed to by "items", and for each items[i], itemTags[i] will be the tag that
	 * corresponds to it.  The total number of entries in both arrays is returned in
	 * numItems.
     *
     * @param resourceTag   The resource tag of the resource the caller wants to extract
     *                      an item from.
	 * @param itemTags		Set to point to an array of UnicodeStrings representing the
	 *						tags in the specified resource.  The caller DOES own this array,
	 *                      and must delete it.
	 * @param items			Set to point to an array of UnicodeStrings containing the
	 *						individual resource items themselves.  itemTags[i] will
	 *						contain the tag corresponding to items[i].  The caller DOES
	 *                      own this array, and must delete it.
	 * @param numItems		Receives the number of items in the arrays pointed to by
	 *						items and itemTags.
     * @param err           Set to MISSING_RESOURCE_ERROR if a resource with the
     *                      specified tag couldn't be found.
     */
	void				getTaggedArray(	const UnicodeString&	resourceTag,
										UnicodeString*&			itemTags,
										UnicodeString*&			items,
										t_int32&				numItems,
										ErrorCode&				err) const;
    /**
     * Return the version number associated with this ResourceBundle. This version
     * number is a string of the form MAJOR.MINOR, where MAJOR is the version number of
     * the current analytic code package, and MINOR is the version number contained in
     * the resource file as the value of the tag "Version". A change in the MINOR
     * version indicated an updated data file. A change in the MAJOR version indicates a
     * new version of the code which is not binary-compatible with the previous version.
	 * If no "Version" tag is present in a resource file, the MINOR version "0" is assigned.
	 *
     * For example, if the Collation sort key algorithm changes, the MAJOR version
     * increments. If the collation data in a resource file changes, the MINOR version
     * for that file increments.
	 *
     * @return  A string of the form N.n, where N is the major version number,
     *          representing the code version, and n is the minor version number,
     *          representing the resource data file. The caller does not own this
     *          string.
     */
	const char*			getVersionNumber();

private:
	friend class Locale;
	friend class RuleBasedCollator;
	
    /**
     * This constructor is used by Collation to load a resource bundle from a specific
     * file, without trying other files. This is used by the Collation caching
     * mechanism.
     */
							ResourceBundle(	const UnicodeString&	path,
											const UnicodeString&	localeName,
											const char*				suffix,
											ErrorCode&				status);

    /**
     * Return a list of all installed locales. This function returns a list of the IDs
     * of all locales represented in the directory specified by this ResourceBundle. It
     * depends on that directory having an "Index" tagged-list item in its "index.txt"
     * file; it parses that list to determine its return value (therefore, that list
     * also has to be up to date). This function is static.
     *
     * This function is the implementation of the Locale::listInstalledLocales()
     * function. It's private because the API for it real;ly belongs in Locale.
     *
     * @param path                 The path to the locale data files. The function will
     *                             look here for "index.txt".
     * @param numInstalledLocales  Receives the number of installed locales, according
     *                             to the Index resource in index.txt.
     * @return                     A list of the installed locales, as a pointer to an
     *                             array of UnicodeStrings. This storage is not owned by
     *                             the caller, who must not delete it. The information
     *                             in this list is derived from the Index resource in
     *                             default.txt, which must be kept up to date.
     */
	static const UnicodeString*	listInstalledLocales(const UnicodeString& path,
													 t_int32&	numInstalledLocales);

    /**
     * Construct a file name by concatenating together the three given elements and
     * returning the result as a CharString. No separators are interposed.
     */
	static char* 		createFilename(	const UnicodeString&	prefix,
										const UnicodeString&	name,
										const UnicodeString&	suffix);
	
    /**
     * Retrieve a ResourceBundle from the cache. Return NULL if not found.
     */
	static const Hashtable* getFromCache(const UnicodeString& path,
										 const UnicodeString& localeName,
										 const UnicodeString& suffix);

	static const Hashtable* getFromCacheWithFallback(const UnicodeString& path,
													 const UnicodeString& desiredLocale,
													 UnicodeString& returnedLocale,
													 const UnicodeString& suffix,
													 ErrorCode& error);

    /**
     * Construct a string key for this object, based on the path name and the locale.
     */
	static void makeHashkey(UnicodeString& keyName,
							const UnicodeString& path,
							const UnicodeString& localeName,
							const UnicodeString& suffix);

    /**
     * Handlers which are passed to parse() have this signature.
     */
	typedef void (*Handler)(const UnicodeString& localeName,
							Hashtable* hashtable,
							void* context);

    /**
     * Parse a file, storing the resource data in the cache.
     */
	static void parse(const char* filename,
					  Handler handler,
					  void* context,
					  ErrorCode &error);

    /**
     * If the given file exists and has not been parsed, then parse it (caching the
     * resultant data) and return true.
     */
	static t_bool parseIfUnparsed(const UnicodeString& path,
								const UnicodeString& locale,
								const UnicodeString& suffix,
								ErrorCode& error);

	const Hashtable* getHashtableForLocale(const UnicodeString& localeName,
										   UnicodeString& returnedLocale,
										   ErrorCode& err);

	const Hashtable* getHashtableForLocale(const UnicodeString& desiredLocale,
										   ErrorCode& error);

	const ResourceBundleData* getDataForTag(const UnicodeString& tag,
											ErrorCode& err) const;

	void constructForLocale(const UnicodeString& path,
							const Locale& locale,
							const char* suffix,
							ErrorCode& error);

	struct AddToCacheContext
	{
		const UnicodeString* fPath;
		const UnicodeString* fFilenameSuffix;
	};

	static void addToCache(const UnicodeString& localeName,
						   Hashtable* hashtable,
						   void* context);

	static void saveCollationHashtable(const UnicodeString& localeName,
									   Hashtable* hashtable,
									   void* context);
private:
    /**
     * This internal class iterates over the fallback and/or default locales. It
     * progresses as follows: Specific: language+country+variant language+country
     * language Default: language+country+variant language+country language Root:
     */
	class LocaleFallbackIterator
	{
	public:
		LocaleFallbackIterator(const UnicodeString& startingLocale,
							   const UnicodeString& root,
							   t_bool useDefaultLocale);

		const UnicodeString& getLocale() const { return fLocale; }

		t_bool nextLocale(ErrorCode& status);

	private:
		void chopLocale();

		UnicodeString fLocale;
		UnicodeString fDefaultLocale;
		UnicodeString fRoot;
		t_bool fUseDefaultLocale;
		t_bool fTriedDefaultLocale;
		t_bool fTriedRoot;
	};

private:
    /**
     * Node IDs for the state transition table. These must fit into 8 bits, as specified
     * by kNodeMask. They must not overlap the bit range used by EAction.
     */
	public: // Not really public; just to allow inner class below to compile
	enum ENode
	{
		/*0*/	kError,
		/*1*/	kInitial,	// Next: Locale name
		/*2*/	kGotLoc,	// Next: {
		/*3*/	kIdle,		// Next: Tag name | }
		/*4*/	kGotTag,	// Next: {
		/*5*/	kNode5,		// Next: Data | Subtag
		/*6*/	kNode6,		// Next: } | { | ,
		/*7*/	kList,		// Next: List data
		/*8*/	kNode8,		// Next: ,
		/*9*/	kTagList,	// Next: Subtag data
		/*10*/	kNode10,	// Next: }
		/*11*/	kNode11,	// Next: Subtag
		/*12*/	kNode12,	// Next: {
		/*13*/	k2dArray,	// Next: Data | }
		/*14*/	kNode14,	// Next: , | }
		/*15*/	kNode15,	// Next: , | }
		/*16*/	kNode16,	// Next: { | }
		kNodeCount,
		kNodeMask = 0x00FF
	};
	private:

    /**
     * Action codes for the state transtiion table. These must fit into 8 bits, as
     * specified by kActionMask, and must not overlap the bit range used by ENode. We
     * may move these to being bitmasks (i.e., 0x0100, 0x0200, 0x0400, etc.) in the
     * future if we want to assign multiple actions to a single transition.
     */
	public: // Not really public; just to allow inner class below to compile
	enum EAction
	{
		// Generic actions
		kNOP		=0x0100,	// Do nothing
		kOpen		=0x0200,	// Open a new locale data block with the data string as the locale name
		kClose		=0x0300,	// Close a locale data block
		kSetTag		=0x0400,	// Record the last string as the tag name

		// Comma-delimited lists
		kBegList	=0x1100,	// Start a new string list with the last string as the first element
		kEndList	=0x1200,	// Close a string list being built
		kListStr	=0x1300,	// Record the last string as a data string and increment the index
		kStr		=0x1400,	// Record the last string as a singleton string

		// 2-d lists
		kBeg2dList	=0x2100,	// Start a new 2d string list with no elements as yet
		kEnd2dList	=0x2200,	// Close a 2d string list being built
		k2dStr		=0x2300,	// Record the last string as a 2d string
		kNewRow		=0x2400,	// Start a new row

		// Tagged lists
		kBegTagged	=0x3100,	// Start a new tagged list with the last string as the first subtag
		kEndTagged	=0x3200,	// Close a tagged list being build
		kSubtag		=0x3300,	// Record the last string as the subtag
		kTaggedStr	=0x3400,	// Record the last string as a tagged string
				
		kActionMask	=0xFF00
	};

    /**
     * A convenience class which encapsulates a node ID and an action. Really just a
     * struct with constructors.
     */
	class T_UTILITY_API Transition
	{
	public:
	        Transition();  // must have a default constructor for HPUX
	        Transition(ENode n);
		Transition(const int i); // int will be split into ENode and EAction
		ENode fNext;
		EAction fAction;
	};
	friend class Transition;

private:
    /**
     * This table describes an ATM (state machine) which parses resource bundle text
     * files rather strictly. Each row represents a node. The columns of that row
     * represent transitions into other nodes. Most transitions are "kError" because
     * most transitions are disallowed. For example, if the parser has just seen a tag
     * name, it enters node 4 ("kGotTag"). The state table then marks only one valid
     * transition, which is into node 5, upon seeing a kOpenBrace token.
     *
     * Some of the transitions are ORed with another value. This second value encodes
     * actions to be taken with the transition. The action values and the node IDs
     * occupy mutually exclusive bit ranges in a 16-bit word. Actions occupy the upper 8
     * bits; node IDs occupy the lower 8 bits. This allows us to OR them together to get
     * a composite transition/action value.
     *
     * We allow an extra comma after the last element in a comma-delimited list
     * (transition from kList to kIdle on kCloseBrace).
     */
	static Transition kTransitionTable[];

    /**
     * Retrieve an element from the transition table.
	 * [The type of "col" should actually be ResourceFormatReader::ETokenType, but we
	 * declare it short here to avoid exposing ResourceFormatReader to the world.]
     */
	static Transition& getTransition(ENode row, short col);

public:
#if 0
	// This is used for debugging
	friend ostream& operator<<(ostream&, const ResourceBundle&);
#endif

private:
	static const char*			kDefaultSuffix;
	static const char*			kDefaultFilename;
	static const char*			kDefaultLocaleName;
	static const char*			kIndexLocaleName;
	static const char*			kIndexFilename;
	static const char*			kIndexTag;
	static const UniChar		kSeparator;

	static const char*			kDefaultMinorVersion;
	static const char*			kVersionSeparator;
	static const UnicodeString	kVersionTag;

	static ResourceBundleCache*	fgCache;
	static VisitedFileCache*	fgVisitedFiles;

    /**
     * Data members. The ResourceBundle object is kept lightweight by having the fData[]
     * array entries be non-owned pointers. The cache (fgCache) owns the entries and
     * will delete them at static destruction time.
     */
	UnicodeString			fPath;
	UnicodeString			fFilenameSuffix;

	enum {					kDataCount = 4 };
	const Hashtable*		fData[kDataCount]; // These aren't const if fIsDataOwned is true
	t_bool					fLoaded[kDataCount];
	t_bool					fIsDataOwned;
	UnicodeString			fRealLocaleID;
	LocaleFallbackIterator*	fLocaleIterator;
	char*					fVersionID;
};

#ifdef NLS_MAC
#pragma export off
#endif

inline ResourceBundle::Transition&
ResourceBundle::getTransition(ENode row,
							  short col)
{
	// Row length is 4
	return kTransitionTable[col + (row<<2)];
}

#endif
