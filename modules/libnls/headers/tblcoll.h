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
// File tblcoll.h
//
// 
//
// Created by: Helena Shih
//
// Modification History:
//
//  Date        Name        Description
//  2/5/97      aliu        Added streamIn and streamOut methods.  Added
//                          constructor which reads RuleBasedCollator object from
//                          a binary file.  Added writeToFile method which streams
//                          RuleBasedCollator out to a binary file.  The streamIn
//                          and streamOut methods use istream and ostream objects
//                          in binary mode.
//  2/12/97     aliu        Modified to use TableCollationData sub-object to
//                          hold invariant data.
//  2/13/97     aliu        Moved several methods into this class from Collation.
//                          Added a private RuleBasedCollator(Locale&) constructor,
//                          to be used by Collator::createDefault().  General
//                          clean up.
//  2/20/97     helena      Added clone, operator==, operator!=, operator=, and copy
//                          constructor and getDynamicClassID.
//  3/5/97      aliu        Modified constructFromFile() to add parameter
//                          specifying whether or not binary loading is to be
//                          attempted.  This is required for dynamic rule loading.
// 05/07/97     helena      Added memory allocation error detection.
//  6/17/97     helena      Added IDENTICAL strength for compare, changed getRules to 
//                          use MergeCollation::getPattern.
//  6/20/97     helena      Java class name change.
//  8/18/97     helena      Added internal API documentation.
// 09/03/97     helena      Added createCollationKeyValues().
// 02/10/98		damiba		Added compare with "length" parameter
//===============================================================================

#ifndef _TBLCOLL
#define _TBLCOLL

#ifndef _PTYPES
#include "ptypes.h"
#endif

#ifndef _COLL
#include "coll.h"
#endif

#ifndef _UNISTRING
#include "unistring.h"
#endif

#ifndef _SORTKEY
#include "sortkey.h"
#endif

#include <stdio.h>

class CompactIntArray;
class VectorOfPToContractElement;
class VectorOfInt;
class VectorOfPToContractTable;
class VectorOfPToExpandTable;
class MergeCollation;
class CollationElementIterator;

/**
 * The RuleBasedCollator class provides the simple implementation of Collator,
 * using data-driven tables.  The user can create a customized table-based
 * collation.
 * <P>
 * RuleBasedCollator maps characters to collation keys.
 * <p>
 * Table Collation has the following restrictions for efficiency (other
 * subclasses may be used for more complex languages) :
 *       <p>1. If the French secondary ordering is specified in a collation object, 
 *             it is applied to the whole object.
 *       <p>2. All non-mentioned Unicode characters are at the end of the
 *             collation order.
 *       <p>3. Private use characters are treated as identical.  The private
 *             use area in Unicode is 0xE800-0xF8FF.
 * <p>The collation table is composed of a list of collation rules, where each
 * rule is of three forms:
 * <pre>
 * .    &lt; modifier >
 * .    &lt; relation > &lt; text-argument >
 * .    &lt; reset > &lt; text-argument >
 * </pre>
 * The following demonstrates how to create your own collation rules:
 * <UL Type=round>
 *    <LI><strong>Text Argument</strong>: A text argument is any sequence of
 *        characters, excluding special characters (that is, whitespace
 *        characters and the characters used in modifier, relation and reset).
 *        If those characters are desired, you can put them in single quotes
 *        (e.g. ampersand => '&').<P>
 *    <LI><strong>Modifier</strong>: There is a single modifier,
 *        which is used to specify that all secondary differences are
 *        sorted backwards.
 *        <p>'@' : Indicates that secondary differences, such as accents, are 
 *                 sorted backwards, as in French.<P>
 *    <LI><strong>Relation</strong>: The relations are the following:
 *        <UL Type=square>
 *            <LI>'&lt;' : Greater, as a letter difference (primary)
 *            <LI>';' : Greater, as an accent difference (secondary)
 *            <LI>',' : Greater, as a case difference (tertiary)
 *            <LI>'=' : Equal
 *        </UL><P>
 *    <LI><strong>Reset</strong>: There is a single reset,
 *        which is used primarily for contractions and expansions, but which
 *        can also be used to add a modification at the end of a set of rules.
 *        <p>'&' : Indicates that the next rule follows the position to where
 *            the reset text-argument would be sorted.
 *
 * <P>Note : The ASCII punctuation characters 0x0020..0x002F, 0x003A..0x0040,
 * 0x005B..0x0060, 0x007B..0x007E are reserved for use in the syntax for
 * building patterns.  That is these all have to be quoted if they occur
 * as literals.
 * <p>
 * This sounds more complicated than it is in practice. For example, the
 * following are equivalent ways of expressing the same thing:
 * <pre>
 * .    a &lt; b &lt; c
 * .    a &lt; b & b &lt; c
 * .    a &lt; c & a &lt; b
 * </pre>
 * Notice that the order is important, as the subsequent item goes immediately
 * after the text-argument. The following are not equivalent:
 * <pre>
 * .    a &lt; b & a &lt; c
 * .    a &lt; c & a &lt; b
 * </pre>
 * Either the text-argument must already be present in the sequence, or some
 * initial substring of the text-argument must be present. (e.g. "a &lt; b & ae &lt;
 * e" is valid since "a" is present in the sequence before "ae" is reset). In
 * this latter case, "ae" is not entered and treated as a single character;
 * instead, "e" is sorted as if it were expanded to two characters: "a"
 * followed by an "e". This difference appears in natural languages: in
 * traditional Spanish "ch" is treated as though it contracts to a single
 * character (expressed as "c &lt; ch &lt; d"), while in traditional German "ä"
 * (a-umlaut) is treated as though it expands to two characters (expressed as
 * "a & ae ; ä &lt; b").
 * <p><strong>Ignorable Characters</strong>
 * <p>For ignorable characters, the first rule must start with a relation (the
 * examples we have used above are really fragments; "a &lt; b" really should be
 * "&lt; a &lt; b"). If, however, the first relation is not "&lt;", then all the 
 * text-arguments up to the first "&lt;" are ignorable. For example, ", - &lt; a &lt; b"
 * makes "-" an ignorable character, as we saw earlier in the word
 * "black-birds". In the samples for different languages, you see that most
 * accents are ignorable.
 * <p><strong>Normalization and Accents</strong>
 * <p>The Collator object automatically normalizes text internally to separate
 * accents from base characters where possible. This is done both when
 * processing the rules, and when comparing two strings. Collator also uses
 * the Unicode canonical mapping to ensure that combining sequences are sorted
 * properly (for more information, see <A HREF="http://www.aw.com/devpress">
 * The Unicode Standard, Version 2.0</A>.)</P>
 * <p><strong>Errors</strong>
 * <p>The following are errors:
 * <UL Type=round>
 *     <LI>A text-argument contains unquoted punctuation symbols
 *        (e.g. "a &lt; b-c &lt; d").
 *     <LI>A relation or reset character not followed by a text-argument
 *        (e.g. "a &lt; , b").
 *     <LI>A reset where the text-argument (or an initial substring of the
 *         text-argument) is not already in the sequence.
 *         (e.g. "a &lt; b & e &lt; f")
 * </UL>
 * If you produce one of these errors, a FormatException will be thrown by
 * RuleBasedCollator.
 * <pre>
 * .    Examples:
 * .    Simple:     "&lt; a &lt; b &lt; c &lt; d"
 * .    Norwegian:  "&lt; a,A&lt; b,B&lt; c,C&lt; d,D&lt; e,E&lt; f,F&lt; g,G&lt; h,H&lt; i,I&lt; j,J
 * .                 &lt; k,K&lt; l,L&lt; m,M&lt; n,N&lt; o,O&lt; p,P&lt; q,Q&lt; r,R&lt; s,S&lt; t,T
 * .                 &lt; u,U&lt; v,V&lt; w,W&lt; x,X&lt; y,Y&lt; z,Z
 * .                 &lt; å=a°,Å=A°
 * .                 ;aa,AA&lt; æ,Æ&lt; ø,Ø"
 * </pre>
 * <p>To create a table-based collation object, simply supply the collation
 * rules to the RuleBasedCollator contructor.  For example:
 * <pre>
 * .    ErrorCode status = ZERO_ERROR;
 * .    RuleBasedCollator *mySimple = new RuleBasedCollator(Simple, status);
 * </pre>
 * <p>Another example:
 * <pre>
 * .    ErrorCode status = ZERO_ERROR;
 * .    RuleBasedCollator *myNorwegian = new RuleBasedCollator(Norwegian, status);
 * </pre>
 * To add rules on top of an existing table, simply supply the orginal rules
 * and modifications to RuleBasedCollator constructor.  For example,
 * <pre>
 * .     Traditional Spanish (fragment): ... & C &lt; ch , cH , Ch , CH ...
 * .     German (fragment) : ...&lt; y , Y &lt; z , Z
 * .                         & AE, Ä & AE, ä 
 * .                         & OE , Ö & OE, ö 
 * .                         & UE , Ü & UE, ü 
 * .     Symbols (fragment): ...&lt; y, Y &lt; z , Z
 * .                         & Question-mark ; '?'
 * .                         & Ampersand ; '&'
 * .                         & Dollar-sign ; '$'
 * <p>To create a collation object for traditional Spanish, the user can take
 * the English collation rules and add the additional rules to the table.
 * For example:
 * <pre>
 * .     ErrorCode status = ZERO_ERROR;
 * .     UnicodeString rules(DEFAULTRULES);
 * .     rules += "& C &lt; ch, cH, Ch, CH";
 * .     RuleBasedCollator *mySpanish = new RuleBasedCollator(rules, status);
 * </pre>
 * <p>In order to sort symbols in the similiar order of sorting their
 * alphabetic equivalents, you can do the following,
 * <pre>
 * .     ErrorCode status = ZERO_ERROR;
 * .     UnicodeString rules(DEFAULTRULES);
 * .     rules += "& Question-mark ; '?' & Ampersand ; '&' & Dollar-sign ; '$' ";
 * .     RuleBasedCollator *myTable = new RuleBasedCollator(rules, status);
 * </pre>
 * <p>Another way of creating the table-based collation object, mySimple,
 * is:
 * <pre>
 * .     ErrorCode status = ZERO_ERROR;
 * .     RuleBasedCollator *mySimple = new
 * .           RuleBasedCollator(" &lt; a &lt; b & b &lt; c & c &lt; d", status);
 * </pre>
 * Or,
 * <pre>
 * .     ErrorCode status = ZERO_ERROR;
 * .     RuleBasedCollator *mySimple = new
 * .           RuleBasedCollator(" &lt; a &lt; b &lt; d & b &lt; c", status);
 * </pre>
 * Because " &lt; a &lt; b &lt; c &lt; d" is the same as "a &lt; b &lt; d & b &lt; c" or
 * "&lt; a &lt; b & b &lt; c & c &lt; d".
 *
 * <p>To combine collations from two locales,
 * <pre>
 * .     ErrorCode status = ZERO_ERROR;
 * .     // Create an en_US collation object
 * .     RuleBasedCollator *en_USCollation = (RuleBasedCollator*)
 * .         Collator::createInstance(Locale("en", "US", ""), status);
 * .     if (FAILURE(status)) return;
 * .     // Create a da_DK collation object
 * .     RuleBasedCollator *da_DKCollation = (RuleBasedCollator*)
 * .         Collator::getDefault(Locale("da", "DK", ""), status);
 * .     if (FAILURE(status)) { delete en_USCollation; return; }
 * .     // Combine the two
 * .     // First, get the collation rules from en_USCollation
 * .     const UnicodeString& en_USRules = en_USCollation->getRules();
 * .     // Second, get the collation rules from da_DKCollation
 * .     const UnicodeString& da_DKRules = da_DKCollation->getRules();
 * .     UnicodeString newRules += en_USRules;
 * .     newRules += da_DKRules;
 * .     RuleBasedCollator *newCollation = new RuleBasedCollator(newRules, status);
 * .     if (FAILURE(status)) { delete en_USCollation; delete da_DKCollation; return; }
 * .     // newCollation has the combined rules
 * .     
 * </pre>
 * <p>Another more interesting example would be to make changes on an existing
 * table to create a new collation object.  For example, add
 * "& C &lt; ch, cH, Ch, CH" to the en_USCollation object to create your own
 * English collation object,
 * <pre>
 * .     // Create a new collation object with additional rules
 * .     UnicodeString addRules(en_USRules);
 * .     addRules += "& C &lt; ch, cH, Ch, CH";
 * .      
 * .     RuleBasedCollator *myCollation = new RuleBasedCollator(addRules, status);
 * .     // myCollation contains the new rules
 * </pre>
 *
 * <p>The following example demonstrates how to change the order of
 * non-spacing accents,
 * <pre>
 * .     // old rule
 * .     UniChar contents[] = {
 * .         '=', 0x0301, ';', 0x0300, ';', 0x0302,
 * .         ';', 0x0308, ';', 0x0327; ',', 0x0303,    // main accents
 * .         ';', 0x0304, ';', 0x0305, ';', 0x0306,    // main accents
 * .         ';', 0x0307, ';', 0x0309, ';', 0x030A,    // main accents
 * .         ';', 0x030B, ';', 0x030C, ';', 0x030D,    // main accents
 * .         ';', 0x030E, ';', 0x030F, ';', 0x0310,    // main accents
 * .         ';', 0x0311, ';', 0x0312,                 // main accents
 * .         '&lt;', 'a', ',', 'A', ';', 'a', 'e', ',', 'A', 'E',
 * .         ';', 0x00e6, ',', 0x00c6, '&lt;', 'b', ',', 'B',
 * .         '&lt;', 'c', ',', 'C', '&lt;', 'e', ',', 'E', '&', 
 * .         'C', '&lt;', 'd', ',', 'D' };
 * .     UnicodeString oldRules(contents);
 * .     ErrorCode status = ZERO_ERROR;
 * .     // change the order of accent characters
 * .     UniChar addOn[] = { '&', ',', 0x0300, ';', 0x0308, ';', 0x0302 };
 * .     oldRules += addOn;
 * .     RuleBasedCollator *myCollation = new RuleBasedCollator(oldRules, status);
 * </pre>
 *
 * <p> The last example shows how to put new primary ordering in before the
 * default setting. For example, in Japanese collation, you can either sort
 * English characters before or after Japanese characters,
 * <pre>
 * .     ErrorCode status = ZERO_ERROR;
 * .     // get en_US collation rules
 * .     Collator *en_USCollation = Collator::createInstance(Locale::US, status);
 * .     // Always check the error code after each call.
 * .     if (FAILURE(status)) return;
 * .     // add a few Japanese character to sort before English characters
 * .     // suppose the last character before the first base letter 'a' in
 * .     // the English collation rule is 0x2212
 * .     UniChar jaString[] = { '&', 0x2212, '&lt;', 0x3041, ',', 0x3042, '&lt;', 0x3043, ',', 0x3044 };
 * .     UnicodeString rules;
 * .     rules += en_USCollation->getRules();
 * .     rules += jaString;
 * .     RuleBasedCollator *myJapaneseCollation = new RuleBasedCollator(rules, status);
 * .     
 * </pre>
 * <p><strong>NOTE</strong>: Typically, a collation object is created with
 * Collator::createInstance().
 * @see        Collator
 * @version    1.27 4/8/97
 * @author     Helena Shih
 */
#ifdef NLS_MAC
#pragma export on
#endif
class T_COLLATE_API RuleBasedCollator : public Collator {
    public : 

        // constructor/destructor
    /**
     * RuleBasedCollator constructor.  This takes the table rules and builds
     * a collation table out of them.  Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @see Locale
     * @param rules the collation rules to build the collation table from.
     * @exception FormatException A format exception
     * will be thrown if the build process of the rules fails. For
     * example, build rule "a &lt; b c &lt; d" will cause the constructor to
     * throw the FormatException.
     */
                                        RuleBasedCollator(  const   UnicodeString&  rules,
                                                                ErrorCode&      status);

    /** Destructor
     */
        virtual                         ~RuleBasedCollator();

    /**
     * Returns true if "other" is the same as "this".
     */
        virtual t_bool                  operator==(const Collator& other) const;

    /**
     * Returns true if "other" is not the same as "this".
     */
        virtual t_bool                  operator!=(const Collator& other) const;

    /**
     * Makes a shallow copy of the object.  The caller owns the returned object.
     * @return the cloned object.
     */
        virtual Collator*               clone() const;

    /**
     * Fills in the copy of the object.
     */
        virtual void                    copy(RuleBasedCollator& copyInto) const;

    /**
     * Creates a collation element iterator for the source string.  The 
     * caller of this method is responsible for the memory management of 
     * the return pointer.
     * @param source the source string.
     * @return the collation element iterator of the source string using this as
     * the based collator.
     */
        CollationElementIterator*       createCollationElementIterator(const UnicodeString& source) const;

    /**
     * The following method is obsolete in our current APIs.  Some methods
     * were renamed in JDK 1.1.  The older versions of the methods will be kept 
     * around for compatibility but will be made obsolete in the future.
     */

    // From createInstance
        CollationElementIterator*       createIterator(const UnicodeString& source) const;
    /**
     * Compares a range of character data stored in two different strings
     * based on the collation rules.  Returns
     * information about whether a string is less than, greater than or
     * equal to another string in a language.
     * This can be overriden in a subclass.
     * @param source the source string.
     * @param target the target string to be compared with the source stirng.
     * @return the comparison result.  GREATER if the source string is greater
     * than the target string, LESS if the source is less than the target.  Otherwise,
     * returns EQUAL.
     */
        virtual     EComparisonResult   compare(    const   UnicodeString&  source, 
                                                    const   UnicodeString&  target) const;
		
		
	//[update Bertrand A. DAMIBA 02/10/98] 
	/**
     * Compares a range of character data stored in two different strings
     * based on the collation rules up to the specified length.  Returns
     * information about whether a string is less than, greater than or
     * equal to another string in a language.
     * This can be overriden in a subclass.
     * @param source the source string.
     * @param target the target string to be compared with the source string.
	 * @param length compares up to the specified length
     * @return the comparison result.  GREATER if the source string is greater
     * than the target string, LESS if the source is less than the target.  Otherwise,
     * returns EQUAL.
     */	
		virtual     EComparisonResult   compare(    const   UnicodeString&  source, 
                                                    const   UnicodeString&  target,
													t_int32	length) const;

	//[end of update Bertrand A. DAMIBA 02/10/98] 

	/** Transforms a specified region of the string into a series of characters
     * that can be compared with CollationKey.compare.
     * This overrides Collator::getCollationKey.  It can be overriden
     * in a subclass.
     * @param source the source string.
     * @param key the transformed key of the source string.
     * @param status the error code status.
     * @return the transformed key.
     */
        virtual     CollationKey&       getCollationKey(    const   UnicodeString&  source,
                                                            CollationKey&   key,
                                                            ErrorCode&  status) const;
    /** 
     * Transforms the string into a unsigned short array that can be compared,
     * the caller owns the returned array.
     * <p>If the source string is null, a null collation key will be returned.
     * @param source the source string to be transformed into a sort key.
     * @param count returns the number of elements in the returned array.
     * @return the collation key value array of the string based on the collation rules.
     * @see CollationKey#compare
     */
        virtual UniChar*       createCollationKeyValues(   const   UnicodeString&  source,
                                                            t_int32& count,
                                                            ErrorCode&      status) const;
    /**
     * Generates the hash code for the rule-based collation object.
     * @return the hash code.
     */
        virtual     t_int32             hashCode() const;

    /**
     * Gets the table-based rules for the collation object.
     * @return returns the collation rules that the table collation object
     * was created from.
     */
        const       UnicodeString&      getRules() const;

    /**
     * Returns a unique class ID POLYMORPHICALLY.  Pure virtual override.
     * This method is to implement a simple version of RTTI, since not all
     * C++ compilers support genuine RTTI.  Polymorphic operator==() and
     * clone() methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     */
    virtual ClassID getDynamicClassID() const
        { return RuleBasedCollator::getStaticClassID(); }


    /**
     * Returns the class ID for this class.  This is useful only for
     * comparing to a return value from getDynamicClassID().  For example:
     *
     *      Base* polymorphic_pointer = createPolymorphicObject();
     *      if (polymorphic_pointer->getDynamicClassID() ==
     *          Derived::getStaticClassID()) ...
     *
     * @return          The class ID for all objects of this class.
     */
    static ClassID getStaticClassID() { return (ClassID)&fgClassID; }


    protected:
    /** Copy constructor
     */
                RuleBasedCollator(const RuleBasedCollator& other);

    /**
     * Assignment operator.
     */
        const   RuleBasedCollator&  operator=(const RuleBasedCollator& other);
    
/*****************************************************************************
 * PRIVATE
 *****************************************************************************/
    private:
        static      char                fgClassID;
        friend      class               CollationElementIterator;
        
    // Collator ONLY needs access to RuleBasedCollator(const Locale&, ErrorCode&)
        friend class Collator;
        
    // TableCollationData ONLY needs access to UNMAPPED
        friend class TableCollationData;


    /** Default constructor
     */
                                        RuleBasedCollator();

    /**
     * Create a table-based collation object with the given rules.
     * @see RuleBasedCollator#RuleBasedCollator
     * @exception FormatException If the rules format is incorrect.
     */
                    void                build(  const   UnicodeString&  rules,
                                                        ErrorCode&      success);

    /**
     * Look up for unmapped values in the expanded character table.
     */
                    void                commit();
    /**
     * Increment of the last order based on the collation strength.
     * @param s the collation strength.
     * @param lastOrder the last collation order.
     * @return the new collation order.
     */
                    t_int32             increment(  Collator::ECollationStrength    s, 
                                                    t_int32                         lastOrder);
    /**
     * Adds a character and its designated order into the collation table.
     * @param ch the Unicode character,
     * @param s the collation strength.
     * @param status the error code status.
     */
                    void                addOrder(   UniChar                         ch, 
                                                    Collator::ECollationStrength    s, 
                                                    ErrorCode&                      status);
    /**
     * Adds the expanding string into the collation table, for example, a-umlaut in German.
     * @param groupChars the contracting characters.
     * @param expChars the expanding characters.
     * @param s the collation strength
     * @param status the error code status.
     */
                    void                addExpandOrder(const    UnicodeString&          groupChars, 
                                                       const    UnicodeString&          expChars, 
                                                       Collator::ECollationStrength s,
                                                       ErrorCode&                       status);
    /**
     * Adds the contracting string into the collation table, for example, ch in Spanish.
     * @param groupChars the contracting characters.
     * @param s the collation strength.
     * @param status the error code status.
     */
                    void                addContractOrder(const  UnicodeString&          groupChars, 
                                                         Collator::ECollationStrength   s,
                                                         ErrorCode&                     status);
    /**
     *  Gets the entry of list of the contracting string in the collation
     *  table.
     *  @param ch the starting character of the contracting string
     *  @return the entry of contracting element which starts with the specified
     *  character in the list of contracting elements.
     */
     /* changed to getContractValuesFromUniChar to avoid type signature
	clashes on some UNIX platforms.  Only happens with NSPR 1.0 <tague>
     */
                    VectorOfPToContractElement* 
                                        getContractValuesFromUniChar(  UniChar     ch) const;
    /**
     *  Ges the entry of list of the contracting string in the collation
     *  table.
     *  @param n the index of the contract character list
     *  @return the entry of the contracting element of the specified index in the
     *  list.
     */
                    VectorOfPToContractElement* 
                                        getContractValues(  t_int32     index) const;
    /**
     *  Gets the entry of list of the expanding string that starts with the specified
     *  character in the collation table.
     *  @param ch the starting character of the expanding string
     *  @return the entry of the expanding-char element which starts with the
     *  specified character.
     */
                    VectorOfInt*        getExpandValuesFromUniChar(    UniChar     ch) const;
    /**
     *  Gets the entry of value list of the expanding string in the collation
     *  table at the specified index.
     *  @param idx the index of the expanding string value list
     *  @return the entry of the expanding-char element of the specified index in 
     *  the list.
     */
                    VectorOfInt*        getExpandValues(    t_int32     index) const;
    /**
     *  Gets the comarison order of a character from the collation table.
     *  @param ch the Unicode character
     *  @return the comparison order of a character.
     */
                    t_int32             getUnicodeOrder(    UniChar     ch) const;

    /**
     *  Gets the comarison order of a character from the collation table.
     *  @param list the contracting element table.
     *  @param name the contracting char string.
     *  @return the comparison order of the contracting character.
     */
                    t_int32             getEntry(   VectorOfPToContractElement*     list, 
                                                    const   UnicodeString&          name);
    /**
     * Check for the secondary and tertiary differences of source and
     * target comparison orders.
     * @param sOrder source collation order.
     * @param tOrder target collation order.
     * @param result the current collation result
     * @param s the current collation strength
     * @param skipSecCheck whether to skip the secondary check.
     * @return Collator.LESS if sOrder &lt; tOrder; EQUAL if sOrder == tOrder;
     * Collator.GREATER if sOrder > tOrder.
     */
                    Collator::EComparisonResult 
                                        checkSecTerDiff(    t_int32     sOrder, 
                                                            t_int32     tOrder, 
                                                            Collator::EComparisonResult result, 
                                                            Collator::ECollationStrength&   s,
                                                            t_bool     skipSecCheck = FALSE) const;

    /**
     * Flattens the given object persistently to a file.  The file name
     * argument should be a path name that can be passed directly to the
     * underlying OS.  Once a RuleBasedCollator has been written to a file,
     * it can be resurrected by calling the RuleBasedCollator(const char*)
     * constructor, which operates very quickly.
     * @param fileName the output file name.
     * @return TRUE if writing to the file was successful, FALSE otherwise.
     */
                    t_bool              writeToFile(const char* fileName) const; // True on success

    /**
     * Add this table collation to the cache.  This involves adding the
     * enclosed TableCollationData to the cache, and then marking our
     * pointer as "not owned" by setting dataIsOwned to false.
     * @param key the unique that represents this collation data object.
     */
                    void                addToCache(         const UnicodeString& key);

    /**
     * RuleBasedCollator constructor.  This constructor takes a locale.  The only
     * caller of this class should be Collator::createDefault().  If createDefault()
     * happens to know that the requested locale's collation is implemented as
     * a RuleBasedCollator, it can then call this constructor.  OTHERWISE IT SHOULDN'T,
     * since this constructor ALWAYS RETURNS A VALID COLLATION TABLE.  It does this
     * by falling back to defaults.
     */
                                        RuleBasedCollator(      const Locale& desiredLocale,
                                                        ErrorCode& status);
    /**
     * Internal constructFromXyx() methods.  These methods do object construction
     * from various sources.  They act like assignment operators; whatever used
     * to be in this object is discarded.  <P>FROM RULES.  This constructor turns
     * around and calls build().  <P>FROM CACHE.  This constructor tries to get the
     * requested cached TableCollationData object, and wrap us around it.  <P>FROM FILE.
     * There are two constructors named constructFromFile().  One takes a const char*:
     * this is a path name to be passed directly to the host OS, where a flattened
     * table collation (produced by writeToFile()) resides.  The other method takes
     * a Locale, and a UnicodeString locale file name.  The distinction is this:
     * the Locale is the locale we are seeking.  The file name is the name of the
     * data file (either binary, as produced by writeToFile(), or ASCII, as read
     * by ResourceBundle).  Within the file, if it is found, the method will look
     * for the given Locale.
     */
                void                constructFromRules( const UnicodeString& rules,
                                                        ErrorCode& status);
                void                constructFromFile(  const Locale&           locale,
                                                        const UnicodeString&    localeFileName,
                                                        t_bool                  tryBinaryFile,
                                                        ErrorCode&              status);
                void                constructFromFile(  const char* fileName,
                                                        ErrorCode& status);
                void                constructFromCache( const UnicodeString& key,
                                                        ErrorCode& status);

    /**
     * The streamIn and streamOut methods read and write objects of this
     * class as binary, platform-dependent data in the iostream.  The stream
     * must be in ios::binary mode for this to work.  These methods are not
     * intended for general public use; they are used by the framework to improve
     * performance by storing certain objects in binary files.
     */
                void                streamIn(FILE* is);
                void                streamOut(FILE* os) const;

    //--------------------------------------------------------------------------
    // Internal Static Utility Methods
    /**
     * Creates the path name with given information.
     * @param prefix the prefix of the file name.
     * @param name the actual file name.
     * @param suffix the suffix of the file name.
     * @return the generated file name.
     */
        static  char*               createPathName( const UnicodeString&    prefix,
                                                    const UnicodeString&    name,
                                                    const UnicodeString&    suffix);

    /**
     * Chops off the last portion of the locale name.  For example, from "en_US_CA"
     * to "en_US" and "en_US" to "en".
     * @param localeName the locale name.
     */
        static  void                chopLocale(UnicodeString&   localeName);

        //--------------------------------------------------------------------------
        // Constants

        static  const   t_int32             UNMAPPED;
        static  const   t_int32             CHARINDEX;  // need look up in .commit()
        static  const   t_int32             EXPANDCHARINDEX; // Expand index follows
        static  const   t_int32             CONTRACTCHARINDEX;  // contract indexes follow

        static  const   t_int32             PRIMARYORDERINCREMENT;
        static  const   t_int32             MAXIGNORABLE;
        static  const   t_int32             SECONDARYORDERINCREMENT;
        static  const   t_int32             TERTIARYORDERINCREMENT;
        static  const   t_int32             PRIMARYORDERMASK;
        static  const   t_int32             SECONDARYORDERMASK;
        static  const   t_int32             TERTIARYORDERMASK;
        static  const   t_int32             SECONDARYRESETMASK;
        static  const   t_int32             IGNORABLEMASK;
        static  const   t_int32             PRIMARYDIFFERENCEONLY;
        static  const   t_int32             SECONDARYDIFFERENCEONLY;
        static  const   t_int32             PRIMARYORDERSHIFT;
        static  const   t_int32             SECONDARYORDERSHIFT;
        static  const   t_int32             SORTKEYOFFSET;
        static  const   t_int32             CONTRACTCHAROVERFLOW;

        static const t_int16                FILEID;

        static       UnicodeString      DEFAULTRULES;

        static  const char*             kFilenameSuffix;
        static  const char*             kResourceBundleSuffix;

        static const t_uint32           kCompactionCycle;

        //--------------------------------------------------------------------------
        // Data Members

                    t_bool              isOverIgnore;
                    t_int32             currentOrder;
                    UniChar             lastChar;
                    MergeCollation*     mPattern;
                    UnicodeString       sbuffer;
                    UnicodeString       tbuffer;
                    UnicodeString       key;
                    CollationElementIterator *sourceCursor;
                    CollationElementIterator *targetCursor;
                    t_bool              dataIsOwned;
                    TableCollationData* data;
};
#ifdef NLS_MAC
#pragma export off
#endif

inline t_bool
RuleBasedCollator::operator!=(const Collator& other) const
{
    return !(*this == other);
}

#endif
