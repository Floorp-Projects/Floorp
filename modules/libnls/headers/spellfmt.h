/*
 * source/format/spellfmt.h, international, international, 971113b 97/10/30
 *
 * (C) Copyright Taligent, Inc. 1996 - All Rights Reserved
 * (C) Copyright IBM Corp. 1996 - All Rights Reserved
 *
 * Portions copyright (c) 1996-1997 Sun Microsystems, Inc. All Rights Reserved.
 *
 *   The original version of this source code and documentation is copyrighted
 * and owned by Taligent, Inc., a wholly-owned subsidiary of IBM. These
 * materials are provided under terms of a License Agreement between Taligent
 * and Sun. This technology is protected by multiple US and International
 * patents. This notice and attribution to Taligent may not be removed.
 *   Taligent is a registered trademark of Taligent, Inc.
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for NON-COMMERCIAL purposes and without
 * fee is hereby granted provided that this copyright notice
 * appears in all copies. Please refer to the file "copyright.html"
 * for further important copyright and licensing information.
 *
 * SUN MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF
 * THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NON-INFRINGEMENT. SUN SHALL NOT BE LIABLE FOR
 * ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR
 * DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
 *
 */

#ifndef _SPELLFMT
#define _SPELLFMT

#include "ptypes.h"
#include "numfmt.h"

struct NumberSpelloutRule;
class SpelloutRuleVector;
 
/**
 * A class that spells out a numeric value in words (i.e., 123.45 becomes "one hundred
 * twenty-three point four five").  You tell the NumberSpelloutFormat how to spell out
 * numbers by passing its constructor a rule description string that it uses to build
 * a rule list, which is in turn used to format and parse numbers.
 * <P>
 * The rule description language works as follows.  Number spellout is specified through
 * the use of an ordered list of rules, each of which has:
 * <ul>
 *  <li>A <i>base value</i> that controls which numbers the rule is used for (a rule
 * applies to the range from its base value to the next rule's base value minus one).
 *  <li>A <i>power of 10</i> that controls how the substitutions behave.  Normally this
 * is the base value's common log, but it can be lower.
 *  <li><i>Rule text,</i> which forms the basis of the format() function's return value.
 *  <li>An optional <i>major substitution,</i> which specifies the position where text is
 * to be inserted into the result string.  The inserted text is the string you get from
 * using this rule set to format the value being formatted / the rule's power of 10.
 * For example, if you use "100: &lt;&lt; hundred >>;" to format 234, the major substitution
 * value is 2, and "two" gets inserted where the &lt;&lt; is.
 *  <li>An optional <i>minor substitution,</i> which also specifies a position where text
 * is to be inserted into the result string.  The inserted text is the string you get
 * from using thisrule set to format the value being formatted % the rule's power of 10.
 * For example, if you use "100: &lt;&lt; hundred >>;" to format 234, the minor substitution
 * value is 34, and "thirty-four" gets inserted where the >> is.
 * </ul>
 * In the description string, rules are separated by semicolons, and leading whitespace is
 * ignored.  The rule's base value may precede its text and is separated from it by a
 * colon (you can include commas or periods for readability, but they're ignored).  A
 * > between the base value and the colon reduces the rule's power of 10 by one.
 * <P>
 * Within the rule text, &lt;&lt; marks the position of the major substitution, and >> marks
 * the position of the minor substitution.  The rule text may include optional text in
 * brackets.  This text is only included when the minor substitution value is not zero
 * (the minor substitution itself is usually included in the brackets).
 * <P>
 * The rule description may also include a <i>negative number rule,</i> which specifies
 * how to format negative numbers.  The negative number rule begins with "-:" instead of
 * a base value, and the minor substitution tells where to put the result of formatting
 * the number's absolute value.
 * <P>
 * The rule description may also include a <i>decimal rule,</i> which specifies how
 * to format numbers that have fractional parts.  The decimal rule begins with ".:"
 * instead of a base value, the major substitution is replaced with the number's
 * integral part, and the minor substitution is replaced by the number's fractional
 * part, spelled out digit-by-digit.
 * <P>
 * The bracket notation actually expands into two rules in the rule list : one that doesn't 
 * include the stuff in the brackets, and one with one-higher base value that does.  In
 * other words,
 * <pre>
 * .    20:twenty[->>];
 * .    turns into
 * .    20:twenty;
 * .    21:twenty->>;
 * .    and 
 * .    100:&lt;&lt;hundred[>>];
 * .    turns into
 * .    100:&lt;&lt;hundred;
 * .    101:&lt;&lt;hundred>>;
 * .    To get an idea of how this owrks, the rules for U.S. English are as follows:
 * .    zero;one;two;three;four;five;six;seven;eight;nine;
 * .    ten;eleven;twelve;thirteen;fourteen;fifteen;sixteen;seventeen;eighteen;nineteen;
 * .    twenty[->>];
 * .    30:thirty[->>];
 * .    40:forty[->>];
 * .    50:fifty[->>];
 * .    60:sixty[->>];
 * .    70:seventy[->>];
 * .    80:eighty[->>];
 * .    90:ninety[->>];
 * .    100:&lt;&lt;hundred[>>];
 * .    1000:&lt;&lt;thousand[>>];
 * .    1000000:&lt;&lt;million[>>];
 * .    1000000000:&lt;&lt;billion[>>];
 * .    1000000000000:&lt;&lt;trillion[>>];
 * .    1000000000000000:OUT OF RANGE!
 * </pre>  
 * @see          NumberFormat
 * @version      1.22 9/17/97
 * @author       Richard Gillam
 */
class T_FORMAT_API NumberSpelloutFormat : public NumberFormat {
public:
    /**
     * Constructs a NumberSpelloutFormat that formats and parses numbers according
     * to the default rule set (U.S. English).
     */
    NumberSpelloutFormat();

    /**
     * Constructs a NumberSpelloutFormat that formats and parses numbers according to
     * the rules specified in "description".
     * @param description A String containing a textual description of the rules to use
     * to format numbers.  For information on the format of this string, see the class
     * description.
     * @param err the error code.
     */
    NumberSpelloutFormat(const UnicodeString& description,
                         ErrorCode&           err);

    /**
     * Copy constructor.
     * @param that the copy origin.
     */
    NumberSpelloutFormat(const NumberSpelloutFormat& that);

    /**
     * Destructor.
     */
    ~NumberSpelloutFormat();

    /**
     * Overrides operator==, checks if obj is the same object as this.
     * @param obj the object to be compared with.
     * @return TRUE if the obj is the same as this, FALSE otherwise.
     */
    virtual t_bool operator==(const Format& obj) const;

    /**
     * Overrides Cloneable, creates an instance that is identical to this.
     * @return the created instance.
     */
    virtual Format* clone() const;

    /**
     * Formats a double number using this SpelloutNumberFormat instance and
     * copy the result to output buffer.
     * @param number the double number to be formatted with.
     * @param output the result buffer.
     * @return the result buffer.
     */
    UnicodeString& format(  double number,
                            UnicodeString& output) const;

    /**
     * Formats a long number using this SpelloutNumberFormat instance and
     * copy the result to output buffer.
     * @param number the long number to be formatted with.
     * @param output the result buffer.
     * @return the result buffer.
     */
    UnicodeString& format(  long number,
                            UnicodeString& output) const;
    /**
     * Appends a string representing "number" spelled out in words (according to this
     * format's rule list) to the end of toAppendTo.
     * @param number The number to format.
     * @param toAppendTo The StringBuffer to append the result to.
     * @param pos Ignored on input.  Set to point to the whole range covered by the
     * formatted number on output.
     * @return toAppendTo
     */
    virtual UnicodeString& format(double number,
                               UnicodeString& toAppendTo,
                               FieldPosition& pos) const;

    /**
     * Appends a string representing "number" spelled out in words (according to this
     * format's rule list) to the end of toAppendTo.
     * @param number The number to format.
     * @param toAppendTo The StringBuffer to append the result to.
     * @param pos Ignored on input.  Set to point to the whole range covered by the
     * formatted number on output.
     * @return toAppendTo
     */
    virtual UnicodeString& format(long number,
                               UnicodeString& toAppendTo,
                               FieldPosition& pos) const;

    /*Added in order not to hide the superclass implementation [Bertrand A. D. 01/20/98]*/
    virtual UnicodeString& format(const Formattable&,UnicodeString&,FieldPosition&,ErrorCode&) const;
    /*end of update [Bertrand A. D. 01/20/98]*/

    /**
     * Parses "text" and returns a Number containing the value represented by "text".
     * @param text The string to parse.
     * @param result The value represented by the string.  If possible, this will be an
     * instance of Long; otherwise, it will be an instance of Double.
     * @param status the error code status.
     */
    virtual void parse( const UnicodeString& text,
                        Formattable& result,
                        ErrorCode& status) const;

    /**
     * Parses "text" and returns a Number containing the value represented by "text".
     * @param text The string to parse.
     * @param parsePosition On entry, specifies the position in the string to begin parsing at.
     * The formatted number is expected to run from this position to the end of the
     * string.  On exit, if the parse succeeded, this will point to the string's past-the-end
     * posiion.  If the parse failed, it will have been left unchanged.
     * @return The value represented by the string.  If possible, this will be an
     * instance of Long; otherwise, it will be an instance of Double.
     */
    virtual void parse(const UnicodeString& text,
                        Formattable& result,
                        ParsePosition& parsePosition) const;

    /**
     * Return the class ID for this class.  This is useful only for
     * comparing to a return value from getDynamicClassID().  For example:
     * <pre>
     * .    Base* polymorphic_pointer = createPolymorphicObject();
     * .    if (polymorphic_pointer->getDynamicClassID() ==
     * .        Derived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     */
    static ClassID getStaticClassID() { return (ClassID)&fgClassID; }

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
    virtual ClassID getDynamicClassID() const { return getStaticClassID(); }

private:
    NumberSpelloutFormat&   operator=(const NumberSpelloutFormat&);
    
    //----------------------------------------------------------------------------
    // implementation of formatting algorithm
    //----------------------------------------------------------------------------

    /**
     * The primary body of the formatting algorithm.
     * @param x The value to format.
     * @param result The StringBuffer into which to insert the result.
     * @param pos The position in "result" where the result should be inserted.
     */
    void doFormat(double            x,
                  UnicodeString&    result,
                  TextOffset        pos) const;

    /**
     * The body of the formatAsDigits() algorithm.
     * @param x The value to format.
     * @param result The StringBuffer into which to insert the result.
     * @param pos The position in "result" where the result is to be inserted.
     */
    void doFormatAsDigits(double            x,
                          UnicodeString&    result,
                          TextOffset        pos) const;

    /**
     * Returns the number of digits after the decimal point in a double number x.
     * @param x the double number
     */
    static int digitsAfterDecimal(double x);

    //----------------------------------------------------------------------------
    // implementation of parsing algorithm
    //----------------------------------------------------------------------------

    /**
     * The main body of the parse algorithm.
     * @param s The sring to parse.
     * @param startAt The position of the first character to consider.  Parsing
     * proceeds from startAt to the <i>beginning</i> of the string.
     * @param pos On exit, this is filled in with the position of the first character in
     * s that was not matched by this call.
     * @param endWithSub If true, match only rules that end with a substitution.  If false,
     * match only rules that <i>don't</i> end with a substitution.
     * @param loBoundP10 Match only rules with a power of 10 greater than or equal to
     * this value.
     * @param hiBoundP10 Match only rules with a power of 10 less than or equal to this value.
     * @return -1 as the error value, otherwise, the parsed value.
     */
    double doParse(const UnicodeString& s,
                  TextOffset startAt,
                  ParsePosition& pos,
                  t_bool endWithSub,
                  t_int16 loBoundP10,
                  t_int16 hiBoundP10) const;

    /**
     * Caled by parse() to look for the text in the negative-number rule.
     * @param s The string to parse
     * @return 0 if the negative-number rule didn't match; otherwise, the value
     * represented by the string.
     */
    double parseNegative(const UnicodeString& s) const;

    /**
     * Called by parse() to match the decimal rule.
     * @param s The string to parse.
     * @return 0 if the string doesn't match the decimal rule.  Otherwise, the value
     * represented by the string.
     */
    double parseDecimal(const UnicodeString& s) const;

    /*
     * Used by parseDecimal() to parse the fractional part of the string.
     * @param s The string to parse.
     * @return The (fractional) value of the string.
     */
    double parseFractionalPart(const UnicodeString& s) const;

    //----------------------------------------------------------------------------
    // implementation functions for rule-description parsing
    //----------------------------------------------------------------------------

    /**
     * Called by the constructor to build the formatter's rule list.
     * @param description A String containing a textual description of the rules to use
     * to format numbers.  For information on the format of this string, see the class
     * description.
     * @param err the error code.
     */
    void buildRuleList(const UnicodeString& description,
                       ErrorCode&           err);

    /*
     * Fills in tempRuleList with a group of new NumberSpelloutRules, one for
     * each semicolon-delimited substring of "description".
     * @param description A String containing a textual description of the rules to use
     * to format numbers.  For information on the format of this string, see the class
     * description.
     * @param tempRuleList, the result rule list.
     * @param err the error code.
     */
    void buildRawRuleList(const UnicodeString& description,
                          SpelloutRuleVector&  tempRuleList,
                          ErrorCode&           err);

    /*
     * If the rule text starts with a number, sets the rule's base value to that number
     * and removes the number from the rule text.  If the rule text doesn't start with
     * a number, sets the rule's base value to nextBaseValue.  Also handles the "-:" and
     * ".:" notation for the negative-number and decimal rules, and sets up the rule's
     * power of 10.
     * @param rule the spell-out rule
     * @param nextBaseValue the base value of the next rule
     * @param err the error code.
     */
    void parseBaseValue(NumberSpelloutRule& rule,
                        double&             nextBaseValue,
                        ErrorCode&          err);

    /*
     * If the rule contains an expression in brackets, splits it into two rules: one
     * without the bracketed text, and another one, with a base value one higher, that
     * does include with bracketed text.  The new rule is inserted right after the
     * original rule in ruleList.
     * @param rule the spell-out rule
     * @param ruleList the rule list
     * @param lineNum the number of rule in the list 
     * @param err the error code.

     */
    void parseBracketExpression(NumberSpelloutRule& rule,
                                SpelloutRuleVector& ruleList,
                                int                 lineNum,
                                ErrorCode&          err);


    /*
     * Sets up the rule's substitutions by looking for the &lt;&lt; and >> markers in the
     * rule text.  Removes the &lt;&lt; and >> markers.
     * @param rule the spell-out rule
     */
    void parseSubstitutions(NumberSpelloutRule& rule);

    NumberSpelloutRule* ruleList;
    t_int16             numRules;
    NumberSpelloutRule* negativeNumberRule;
    NumberSpelloutRule* decimalRule;

    static const UnicodeString DEFAULT_SPELLOUT_DESCRIPTION;

    static char fgClassID;
};

#endif
