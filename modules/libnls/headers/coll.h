/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1996                                                 *
*   (C) Copyright International Business Machines Corporation,  1996                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*/
//===============================================================================
//
// File coll.h
//
// 
//
// Created by: Helena Shih
//
// Modification History:
//
//  Date        Name        Description
// 02/5/97      aliu        Modified createDefault to load collation data from
//                          binary files when possible.  Added related methods
//                          createCollationFromFile, chopLocale, createPathName.
// 02/11/97     aliu        Added members addToCache, findInCache, and fgCache.
// 02/12/97     aliu        Modified to create objects from RuleBasedCollator cache.
//                          Moved cache out of Collation class.
// 02/13/97     aliu        Moved several methods out of this class and into
//                          RuleBasedCollator, with modifications.  Modified
//                          createDefault() to call new RuleBasedCollator(Locale&)
//                          constructor.  General clean up and documentation.
// 02/20/97     helena      Added clone, operator==, operator!=, operator=, copy
//                          constructor and getDynamicClassID.
// 03/25/97     helena      Updated with platform independent data types.
// 05/06/97     helena      Added memory allocation error detection.
//  6/20/97     helena      Java class name change.
// 09/03/97     helena      Added createCollationKeyValues().
// 02/10/98     damiba      Added compare() with length as parameter.
//===============================================================================

#ifndef _COLL
#define _COLL


#ifndef _LOCID
#include "locid.h"
#endif

#ifndef _PTYPES
#include "ptypes.h"
#endif

#ifndef _UNISTRING
#include "unistring.h"
#endif

/**
 * The Collation class is an abstract class which provides Unicode text
 * comparison services.  Text collation supports language-sensitive
 * comparison of strings, allowing for text searching and alphabetical
 * sorting.  The collation classes provide a choice of ordering
 * strength (for example, to ignore or not ignore case differences) and
 * handle ignored, expanding, and contracting characters.
 * <p>
 * Developers don't need to know anything about the collation rules for various
 * languages. Any features requiring collation can use the collation object
 * associated with the current default locale, or with a specific locale
 * (like France or Japan) if appropriate.
 * <UL TYPE=round>
 *     <LI><strong>Basic Collation</strong>: Correctly sorting strings is
 *         tricky, even in English. The results of a sort must be consistent
 *         ; any differences in strings must always be sorted the same
 *         way. The sort assigns relative priorities to different features of
 *         the text, based on the characters themselves and on the current
 *         ordering strength of the collation object. Correct comparison and
 *         sorting of natural languages requires the following:
 *         <UL TYPE=square>
 *             <LI>Ordering priorities: The first primary difference will
 *             determine the resultant order.  No matter what the other
 *             characters are. For example, "cat" &lt; "dog". Some languages
 *             require primary, secondary, and tertiary ordering. For example,
 *             in Czech, case differences are a tertiary difference (A vs. a),
 *             accent differences are a secondary differece (e vs. ê) and
 *             different base letters are a primary difference (d vs. e).
 *             <LI>Group characters: In collating some languages, a sequence of
 *             characters is treated as though it was a single letter of the
 *             alphabet.  For example, "cx" &lt; "chx" &lt; "dx".
 *             <LI>Expanding characters: In some languages, a single character
 *             is treated as though it was a sequence of letters of the
 *             alphabet. For example, "aex" &lt; "æx" &lt; "aexx".
 *             <LI>Ignorable characters: Certain characters are ignored when
 *             collating. That is, they are not significant unless there are
 *             other differences in the remainder of the string.  For example,
 *             "blackbird" &lt; "black-bird" &lt; "blackbirds"
 *         </UL>
 *     <LI><strong>Localizable Collation</strong>: Different collation objects
 *         associated with various locales handle the differences required
 *         when sorting text strings for different languages.
 *     <LI><strong>Customization</strong>: You can produce a new collation by
 *         adding to or changing an existing one.
 * </UL>
 * <p>Because compare()'s algorithm is complex, it is faster to sort long lists
 * of words by retrieving sort keys or collation keys with getCollationKey().
 * You can then cache the sort keys and compare them using CollationKey::compareTo().
 * The following is a list of features of sort key:
 * <UL Type=round>
 *     <LI>bit-ordered (so you can do bit-wise comparison on sort keys)
 *     <LI>use CollationKey.compareTo to compare sort keys
 *     <LI>faster than direct compare algorithm once the keys are generated
 * </UL>
 * <p>Collation subclasses implement different collation rules for different
 * languages and different applications. (phone book, dictionary, etc.)
 * <p>Use collation strength parameters, PRIMARY, SECONDARY, TERTIARY, and
 * IDENTICAL to specify the comparison level.
 * Each unicode character is assigned ordering priority: primary, secondary,
 * tertiary and no difference.
 * <p>Decomposition mode determines how composed characters are handled for
 * Unicode.
 * <UL Type=round>
 *     <LI>No Decomposition: With no decomposition, accented characters will
 *         not be sorted correctly; this should only be used if the source
 *         text is guaranteed to have no accented characters.
 *     <LI>Canonical Decomposition : Characters that are canonical variants
 *         according to Unicode 2.0 are decomposed in collation if canonical
 *         decomposition mode is set.  This is the default, and is required
 *         for proper collation of accented characters.
 *     <LI>Full Decomposition : With full decomposition, both canonical
 *         variants and compatibility variants are decomposed.  This causes
 *         not only accented characters to be sorted, but also characters that
 *         have special formats to be sorted with their norminal form.  For
 *         example, the half-width and full-width ASCII and Katakana characters
 *         are then sorted properly.
 * </UL>
 * <P>NO_DECOMPOSITION is the fastest, but does not produce correct results,
 * except for languages that do not use accents.  You should generally use at
 * least CANONICAL_DECOMPOSITION.  FULL_DECOMPOSITION decomposes even more
 * characters.  For example, it maps the Japanese half-width kana characters
 * to their normalized characters.  For more information on the precise
 * mappings used, see http://unicode.org.
 *
 * <p>LESS, EQUAL, GREATER identifies the result
 * of unicode text strings comparison.
 * <p>Use the static method Collator::createInstance() to instantiate the class
 * by passing the desired locale as the argument.
 * <p>Example of use:
 * <pre>
 * .       ErrorCode status = ZERO_ERROR;
 * .       // Compare two strings in the default locale
 * .       Collator *myCollation = Collator::createInstance(status);
 * .       if (FAILURE(status)) return;
 * .       Collator::EComparisonResult result = myCollation->compare("abc", "ABC");
 * .       delete myCollation;
 * </pre>
 * <p>Another example:
 * <pre>
 * .       // compare two strings in French
 * .       Collator *myCollation = Collator.createInstance(Locale::FRANCE, status);
 * .       if (FAILURE(status)) return;
 * .       Collator::EComparisonResult result = myCollation->compare("abc", "ABC");
 * </pre>
 * <p>The following example demonstrates different ways of comparing two
 * strings:
 * <pre>
 * .       UnicodeString a("abcdefgh"), b("ijklmnop");
 *
 * .       // This comparision is not as fast as collation keys.
 * .       // For multiple comparison, use CollationKey.  Please see CollationKey
 * .       // class description for more description
 * .       if (myCollation->compare(a, b) == Collator::LESS) { // ... }
 *
 * .       // Faster than compare when collation keys are cached
 * .       ErrorCode aKeyStatus, bKeyStatus;
 * .       CollationKey aKey, bKey;
 * .       aKey = myCollation->getCollationKey(a, aKey, aKeyStatus);
 * .       bKey = myCollation->getCollationKey(b, bKey, bKeyStatus);
 * .       if (SUCCESS(aKeyStatus)) && SUCCESS(bKeyStatus))
 * .            if (aKey.compareTo(bKey) == Collator::LESS)
 * .                 { // ... }
 *
 * </pre>
 * <P><strong>NOTE</strong>: Two collation keys from different collations cannot be
 * compared.  Incorrect comparison result will be returned if you compare
 * two collation keys from different collators.
 *
 * @see                RuleBasedCollator
 * @see                CollationKey
 * @see                Locale
 * @version            1.7 1/14/97
 * @author             Helena Shih
 */


#ifdef NLS_MAC
#pragma export on
#endif

class T_COLLATE_API Collator {
	public : 
   /**
	* NO_DECOMPOSITION : Accented characters will not be decomposed for sorting.  
	* Please see class description for more details.
    * CANONICAL_DECOMPOSITION : Characters that are canonical variants according 
	* to Unicode 2.0 will be decomposed for sorting.  This is the default setting. 
	* FULL_DECOMPOSITION : Both canonical variants and compatibility variants be
	* decomposed for sorting.
    */

		enum EDecompositionMode {
			NO_DECOMPOSITION,
			CANONICAL_DECOMPOSITION,
			FULL_DECOMPOSITION
		};
    /**
     * Base letter represents a primary difference.  Set comparison
     * level to PRIMARY to ignore secondary and tertiary differences.
     * Use this to set the strength of a Collator object.
     * Example of primary difference, "abc" &lt; "abd"
     * 
     * Diacritical differences on the same base letter represent a secondary
     * difference.  Set comparison level to SECONDARY to ignore tertiary
     * differences. Use this to set the strength of a Collator object.
     * Example of secondary difference, "ä" >> "a".
     *
     * Uppercase and lowercase versions of the same character represents a
     * tertiary difference.  Set comparison level to TERTIARY to include
     * all comparison differences. Use this to set the strength of a Collator
     * object.
     * Example of tertiary difference, "abc" &lt;&lt;&lt; "ABC".
     *
     * Two characters are considered "identical" when they have the same
     * unicode spellings.
     * For example, "ä" == "ä".
     *
     * ECollationStrength is also used to determine the strength of sort keys 
     * generated from Collator objects.
     */

     
		enum ECollationStrength {
			PRIMARY,
			SECONDARY,
			TERTIARY,
			IDENTICAL
		};
	/**
	 * LESS is returned if source string is compared to be less than target
	 * string in the compare() method.
     * EQUAL is returned if source string is compared to be equal to target
     * string in the compare() method.
     * GREATER is returned if source string is compared to be greater than
     * target string in the compare() method.
     * @see Collator#compare
     */
		enum EComparisonResult {
			LESS = -1,
			EQUAL = 0,
			GREATER = 1
		};
	/**
	 * Destructor
	 */
	virtual							~Collator();

	/**
	 * Returns true if "other" is the same as "this"
	 */
	virtual		t_bool				operator==(const Collator& other) const;

	/**
	 * Returns true if "other" is not the same as "this".
	 */
	virtual		t_bool				operator!=(const Collator& other) const;

	/**
	 * Makes a shallow copy of the current object.
	 */
	virtual		Collator*			clone() const = 0;
    /**
     * Creates the collator object for the current default locale.
     * The default locale is determined by Locale::getDefault.
     * @return the collation object of the default locale.(for example, en_US)
     * @see Locale#getDefault
	 * The ErrorCode& err parameter is used to return status information to the user.
	 * To check whether the construction succeeded or not, you should check
	 * the value of SUCCESS(err).  If you wish more detailed information, you
	 * can check for informational error results which still indicate success.
	 * USING_FALLBACK_ERROR indicates that a fall back locale was used.  For
	 * example, 'de_CH' was requested, but nothing was found there, so 'de' was
	 * used.  USING_DEFAULT_ERROR indicates that the default locale data was
	 * used; neither the requested locale nor any of its fall back locales
	 * could be found.
	 * The caller owns the returned object and is responsible for deleting it.
     */
		static	Collator*			createInstance(	ErrorCode&	err);

    /**
     * Gets the table-based collation object for the desired locale.  The
     * resource of the desired locale will be loaded by ResourceLoader. 
	 * Locale::ENGLISH is the base collation table and all other languages are 
	 * built on top of it with additional language-specific modifications.
     * @param desiredLocale the desired locale to create the collation table
     * with.
     * @return the created table-based collation object based on the desired
     * locale.
     * @see Locale
     * @see ResourceLoader
 	 * The ErrorCode& err parameter is used to return status information to the user.
	 * To check whether the construction succeeded or not, you should check
	 * the value of SUCCESS(err).  If you wish more detailed information, you
	 * can check for informational error results which still indicate success.
	 * USING_FALLBACK_ERROR indicates that a fall back locale was used.  For
	 * example, 'de_CH' was requested, but nothing was found there, so 'de' was
	 * used.  USING_DEFAULT_ERROR indicates that the default locale data was
	 * used; neither the requested locale nor any of its fall back locales
	 * could be found.
	 * The caller owns the returned object and is responsible for deleting it.
    */
		static	Collator*			createInstance(	const Locale&	loc,
													ErrorCode&		err);

	/**
	 * The following methods are obsolete in our current APIs.  Some methods
	 * were renamed in JDK 1.1.  The older versions of the methods will be kept 
	 * around for compatibility but will be made obsolete in the future.
	 */

	// From createInstance
		static	Collator*			createDefault(ErrorCode&	err);
		static	Collator*			createDefault(const	Locale&	loc,
												ErrorCode&	err);
		
		// comparison
    /**
     * The comparison function compares the character data stored in two
     * different strings.  Returns information about whether a string
     * is less than, greater than or equal to another string.
     * <p>Example of use:
     * <pre>
     * .       ErrorCode status = ZERO_ERROR;
     * .       Collator *myCollation = Collator::createInstance(Locale::US, status);
     * .       if (FAILURE(status)) return;
     * .       myCollation->setStrength(Collator::PRIMARY);
     * .       // result would be Collator::EQUAL ("abc" == "ABC")
     * .       // (no primary difference between "abc" and "ABC")
     * .       Collator::EComparisonResult result = myCollation->compare("abc", "ABC");
     * .       myCollation->setStrength(Collator::TERTIARY);
     * .       // result would be Collator::LESS (abc" &lt;&lt;&lt; "ABC")
     * .       // (with tertiary difference between "abc" and "ABC")
     * .       Collator::EComparisonResult result = myCollation->compare("abc", "ABC");
     * </pre>
     * @param source the source string to be compared with.
     * @param target the string that is to be compared with the source string.
     * @return Returns a byte value. GREATER if source is greater
     * than target; EQUAL if source is equal to target; LESS if source is less
     * than target
     **/
		virtual EComparisonResult	compare(	const	UnicodeString&	source, 
												const	UnicodeString&	target) const = 0;
    /**
     * Does the same thing as compare but limits the comparison to a specified length
     * <p>Example of use:
     * <pre>
     * .       ErrorCode status = ZERO_ERROR;
     * .       Collator *myCollation = Collator::createInstance(Locale::US, status);
     * .       if (FAILURE(status)) return;
     * .       myCollation->setStrength(Collator::PRIMARY);
     * .       // result would be Collator::EQUAL ("abc" == "ABC")
     * .       // (no primary difference between "abc" and "ABC")
     * .       Collator::EComparisonResult result = myCollation->compare("abc", "ABC",3);
     * .       myCollation->setStrength(Collator::TERTIARY);
     * .       // result would be Collator::LESS (abc" &lt;&lt;&lt; "ABC")
     * .       // (with tertiary difference between "abc" and "ABC")
     * .       Collator::EComparisonResult result = myCollation->compare("abc", "ABC",3);
     * </pre>
     * @param source the source string to be compared with.
     * @param target the string that is to be compared with the source string.
     * @param length the length the comparison is limitted to
     * @return Returns a byte value. GREATER if source (up to the specified length) is greater
     * than target; EQUAL if source (up to specified length) is equal to target; LESS if source
     * (up to the specified length) is less  than target.   
     **/

        virtual EComparisonResult   compare(    const   UnicodeString&  source,
                                                const   UnicodeString&  target,
                                                t_int32 length) const = 0;
    
    


    /** Transforms the string into a series of characters that can be compared
     * with CollationKey::compareTo. It is not possible to restore the original
     * string from the chars in the sort key.  The generated sort key handles 
     * only a limited number of ignorable characters.
     * <p>Use CollationKey::equals or CollationKey::compare to compare the
     * generated sort keys.
     * <p>Example of use:
     * <pre>
     * .       ErrorCode status = ZERO_ERROR;
     * .       Collator *myCollation = Collator::createInstance(Locale::US, status);
     * .       if (FAILURE(status)) return;
     * .       myCollation->setStrength(Collator::PRIMARY);
     * .       ErrorCode key1Status, key2Status;
     * .       CollationKey CollationKey1
     * .       CollationKey1 = myCollation->getCollationKey("abc", CollationKey1, key1Status);
     * .       CollationKey CollationKey2
     * .       CollationKey2 = myCollation->getCollationKey("ABC", CollationKey2, key2Status);
     * .       if (FAILURE(key1Status) || FAILURE(key2Status)) { delete myCollation; return; }
     * .       // Use CollationKey::compare() to compare the sort keys
     * .       // result would be 0 (CollationKey1 == CollationKey2)
     * .       int result = CollationKey1.compare(CollationKey2);
     * .       myCollation->setStrength(Collator::TERTIARY);
     * .       CollationKey1 = myCollation->getCollationKey("abc", CollationKey1, key1Status);
     * .       CollationKey2 = myCollation->getCollationKey("ABC", CollationKey2, key2Status);
     * .       if (FAILURE(key1Status) || FAILURE(key2Status)) { delete myCollation; return; }
     * .       // Use CollationKey::compareTo to compare the collation keys
     * .       // result would be -1 (CollationKey1 &lt; CollationKey2)
     * .       result = CollationKey1.compareTo(CollationKey2);
     * .       delete myCollation;
     * </pre>
     * <p>If the source string is null, a null collation key will be returned.
     * @param source the source string to be transformed into a sort key.
     * @param key the collation key to be filled in
     * @return the collation key of the string based on the collation rules.
     * @see CollationKey#compare
     */
		virtual CollationKey&		getCollationKey(	const	UnicodeString&	source,
														CollationKey&		key,
														ErrorCode&      status) const = 0;
    /** 
     * Transforms the string into a unsigned short array that can be compared,
     * the caller owns the returned array.
     * <p>Example of use:
     * <pre>
     * .       ErrorCode status = ZERO_ERROR;
     * .       Collator *myCollation = Collator::createInstance(Locale::US, status);
     * .       if (FAILURE(status)) return;
     * .       myCollation->setStrength(Collator::PRIMARY);
     * .       ErrorCode key1Status, key2Status;
     * .       t_uint16 *array1 = 0;
     * .       t_uint16 *array2 = 0;
     * .       t_int32 array1Count, array2Count;
     * .       array1 = myCollation->getCollationKey("abc", array1Count, key1Status);
     * .       array2 = myCollation->getCollationKey("ABC", array2Count, key2Status);
     * .       if (FAILURE(key1Status) || FAILURE(key2Status)) { delete myCollation; return; }
     * .       // Use a loop to compare the two arrays
     * .       delete array1;
     * .       delete array2;
     * .       delete myCollation;
     * </pre>
     * <p>If the source string is null, a null collation key will be returned.
     * @param source the source string to be transformed into a sort key.
     * @param count returns the number of elements in the returned array.
     * @return the collation key value array of the string based on the collation rules.
     * @see CollationKey#compare
     */
        virtual UniChar*       createCollationKeyValues(   const   UnicodeString&  source,
                                                            t_int32& count,
                                                            ErrorCode&      status) const = 0;

	/**
	 * The following method is obsolete in our current APIs.  Some methods
	 * were renamed in JDK 1.1.  The older versions of the methods will be kept 
	 * around for compatibility but will be made obsolete in the future.
	 */

		virtual SortKey&			getSortKey(	const	UnicodeString&	source,
														SortKey&		key,
														ErrorCode&      status) const;
    /**
     * Generates the hash code for the collation object
     */
		virtual t_int32				hashCode() const = 0;

    /**
     * Convenience method for comparing two strings based on
     * the collation rules.
     * @param source the source string to be compared with.
     * @param target the target string to be compared with.
     * @return true if the first string is greater than the second one,
     * according to the collation rules. false, otherwise.
     * @see Collator#compare
     */
				t_bool				greater(	const	UnicodeString& source, 
												const	UnicodeString& target) const;
    /**
     * Convenience method for comparing two strings based on the collation
     * rules.
     * @param source the source string to be compared with.
     * @param target the target string to be compared with.
     * @return true if the first string is greater than or equal to the
     * second one, according to the collation rules. false, otherwise.
     * @see Collator#compare
     */
                t_bool              greaterOrEqual( const   UnicodeString& source, 
                                                    const   UnicodeString& target) const;
    /**
     * Convenience method for comparing two strings based on the collation
     * rules.
     * @param source the source string to be compared with.
     * @param target the target string to be compared with.
     * @return true if the strings are equal according to the collation
     * rules.  false, otherwise.
     * @see Collator#compare
     */
				t_bool				equals(	const	UnicodeString& source, 
											const	UnicodeString& target) const;
		
		// getter/setter
    /**
     * Get the decomposition mode of the collator object.
     * @return the decomposition mode
     * @see Collator#setDecomposition
     */
				EDecompositionMode	getDecomposition() const;
    /**
     * Set the decomposition mode of the collator object. success is equal
	 * to ILLEGAL_ARGUMENT_ERROR if error occurs.
     * @param the new decomposition mode
     * @see Collator#getDecomposition
     */
				void				setDecomposition(	EDecompositionMode	mode);
    /**
     * Determines the minimum strength that will be use in comparison or
     * transformation.
     * <p>E.g. with strength == SECONDARY, the tertiary difference is ignored
     * <p>E.g. with strength == PRIMARY, the secondary and tertiary difference
     * are ignored.
     * @return the current comparison level.
     * @see Collator#setStrength
     */
				ECollationStrength	getStrength() const;
    /**
     * Sets the minimum strength to be used in comparison or transformation.
     * <p>Example of use:
     * <pre>
     * .       ErrorCode status = ZERO_ERROR;
     * .       Collator *myCollation = Collator::createInstance(Locale::US, status);
     * .       if (FAILURE(status)) return;
     * .       myCollation->setStrength(Collator::PRIMARY);
     * .       // result will be "abc" == "ABC"
     * .       // tertiary differences will be ignored
     * .       Collator::ComparisonResult result = myCollation->compare("abc", "ABC");
     * </pre>
     * @see Collator#getStrength
     * @param newStrength the new comparison level.
     */
				void				setStrength(	ECollationStrength	newStrength);
   /**
     * Get name of the object for the desired Locale, in the desired langauge
     * @param objectLocale must be from getAvailableLocales
     * @param displayLocale specifies the desired locale for output
	 * @param name the fill-in parameter of the return value
     * @return display-able name of the object for the object locale in the
     * desired language
     */
    static	UnicodeString&		getDisplayName(	const	Locale&		objectLocale,
												const	Locale&		displayLocale,
														UnicodeString& name) ;
    /**
     * Get name of the object for the desired Locale, in the langauge of the
     * default locale.
     * @param objectLocale must be from getAvailableLocales
	 * @param name the fill-in parameter of the return value
     * @return name of the object for the desired locale in the default
     * language
     */
	static	UnicodeString&		getDisplayName(	const	Locale&			objectLocale,
														UnicodeString&	name) ;

    /**
     * Get the set of Locales for which Collations are installed
	 * @param count the output parameter of number of elements in the locale list
     * @return the list of available locales which collations are installed
     */
	static	const	Locale*		getAvailableLocales(t_int32& count);

	/**
	 * Returns a unique class ID POLYMORPHICALLY.  Pure virtual method.
	 * This method is to implement a simple version of RTTI, since not all
	 * C++ compilers support genuine RTTI.  Polymorphic operator==() and
	 * clone() methods call this method.
	 *
	 * Concrete subclasses of Format must implement getDynamicClassID()
	 * and also a static method and data member:
	 *
	 *		static ClassID getStaticClassID() { return (ClassID)&fgClassID; }
	 *		static char fgClassID;
	 *
	 * @return			The class ID for this object. All objects of a
	 *					given class have the same class ID.  Objects of
	 *					other classes have different class IDs.
	 */
	virtual ClassID getDynamicClassID() const = 0;

  	/** Netscape
  	 * Returns the version number for the collator
  	 * format is major.minor ie: 01.00
  	 */
  	virtual const char* getVersionNumber();

	protected:
	/**
	 * Constructors
	 */
								Collator();
								Collator(const	Collator&	other);

	/**
	 * Assignment operator
	 */
	const		Collator&		operator=(const	Collator&	other);

	//--------------------------------------------------------------------------
	private:
			
			ECollationStrength	strength;
			EDecompositionMode	decmp;
};

#ifdef NLS_MAC
#pragma export off
#endif

inline t_bool
Collator::operator==(const Collator& other) const
{
	t_bool result;
	if (this == &other) result = TRUE;
	else result = ((strength == other.strength) && (decmp == other.decmp));
	return result;
}

inline t_bool
Collator::operator!=(const Collator& other) const
{
	t_bool result;
	result = !(*this == other);
	return result;
}

inline Collator*
Collator::createDefault(ErrorCode& status)
{
	return Collator::createInstance(status);
}

inline Collator*
Collator::createDefault(const Locale& loc,
					   ErrorCode& status)
{
	return Collator::createInstance(loc, status);
}

inline SortKey&
Collator::getSortKey(	const	UnicodeString&	source,
								SortKey&		key,
								ErrorCode&      status) const
{
    return (SortKey&)getCollationKey(source, (CollationKey&)key, status);
}

#endif
