/*
********************************************************************************
*                                                                              *
* COPYRIGHT:                                                                   *
*   (C) Copyright Taligent, Inc.,  1997                                        *
*   (C) Copyright International Business Machines Corporation,  1997           *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.         *
*   US Government Users Restricted Rights - Use, duplication, or disclosure    *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                     *
*                                                                              *
********************************************************************************
*
* File MSGFMT.H
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/20/97    helena      Finished first cut of implementation.
********************************************************************************
*/
// *****************************************************************************
// This file was generated from the java source file MessageFormat.java
// *****************************************************************************
 
#ifndef _MSGFMT
#define _MSGFMT
 
#include "format.h"
#include "ptypes.h"
#include "locid.h"
class NumberFormat;

/**
 * Provides means to produce concatenated messages in language-neutral way.
 * Use this for all concatenations that show up to end users.
 * <P>
 * Takes a set of objects, formats them, then inserts the formatted
 * strings into the pattern at the appropriate places.
 * <P>
 * Here are some examples of usage:
 * <pre>
 * .      GregorianCalendar cal;
 * .      Formattable arguments[] = {
 * .          7L,
 * .          Formattable(Date(cal.getTime()), Formattable::kIsDate),
 * .          "a disturbance in the Force"
 * .      };
 * .          
 * .      String result = MessageFormat::format(
 * .           "At {1,time} on {1,date}, there was {2} on planet {0,integer}.",
 * .           arguments);
 * .          
 * .      &lt;output>: At 12:30 PM on Jul 3, 2053, there was a disturbance
 * .                 in the Force on planet 7.
 * </pre>  
 * Typically, the message format will come from resources, and the
 * arguments will be dynamically set at runtime.
 * <pre>
 * Example 2:
 * .      
 * .      Formattable testArgs[] = {3L, "MyDisk"};
 *    
 * .      MessageFormat* form = new MessageFormat(
 * .          "The disk \"{1}\" contains {0} file(s).");
 * .        
 * .      UnicodeString string;
 * .      cout &lt;&lt; form->format(testArgs, string) &lt;&lt; endl;
 * .      
 * .      // output, with different testArgs
 * .      output: The disk "MyDisk" contains 0 file(s).
 * .      output: The disk "MyDisk" contains 1 file(s).
 * .      output: The disk "MyDisk" contains 1,273 file(s).
 *  </pre>
 *
 *  The pattern is of the following form.  Legend:
 *  <pre>
 * .      {optional item}
 * .      (group that may be repeated)*
 *  </pre>
 *  Do not confuse optional items with items inside quotes braces, such
 *  as this: "{".  Quoted braces are literals.
 *  <pre>
 * .      messageFormatPattern := string ( "{" messageFormatElement "}" string )*
 * .       
 * .      messageFormatElement := argument { "," elementFormat }
 * .       
 * .      elementFormat := "time" { "," datetimeStyle }
 * .                     | "date" { "," datetimeStyle }
 * .                     | "number" { "," numberStyle }
 * .                     | "choice" "," choiceStyle
 * .  
 * .      datetimeStyle := "short"
 * .                     | "medium"
 * .                     | "long"
 * .                     | "full"
 * .                     | dateFormatPattern
 * .
 * .      numberStyle :=   "currency"
 * .                     | "percent"
 * .                     | "integer"
 * .                     | numberFormatPattern
 * . 
 * .      choiceStyle :=   choiceFormatPattern
 * </pre>
 * If there is no elementFormat, then the argument must be a string,
 * which is substituted. If there is no dateTimeStyle or numberStyle,
 * then the default format is used (e.g.  NumberFormat.getInstance(),
 * DateFormat.getDefaultTime() or DateFormat.getDefaultDate(). For
 * a ChoiceFormat, the pattern must always be specified, since there
 * is no default.
 * <P>
 * In strings, single quotes can be used to quote the "{" sign if
 * necessary. A real single quote is represented by ''.  Inside a
 * messageFormatElement, quotes are [not] removed. For example,
 * {1,number,$'#',##} will produce a number format with the pound-sign
 * quoted, with a result such as: "$#31,45".
 * <P>
 * If a pattern is used, then unquoted braces in the pattern, if any,
 * must match: that is, "ab {0} de" and "ab '}' de" are ok, but "ab
 * {0'}' de" and "ab } de" are not.
 * <P>
 * The argument is a number from 0 to 9, which corresponds to the
 * arguments presented in an array to be formatted.
 * <P>
 * It is ok to have unused arguments in the array.  With missing
 * arguments or arguments that are not of the right class for the
 * specified format, a failing ErrorCode result is set.
 * <P>
 * For more sophisticated patterns, you can use a ChoiceFormat to get
 * output such as:
 * <pre>
 * .   MessageFormat* form = new MessageFormat("The disk \"{1}\" contains {0}.");
 * .   double filelimits[] = {0,1,2};
 * .   UnicodeString filepart[] = {"no files","one file","{0,number} files"};
 * .   ChoiceFormat* fileform = new ChoiceFormat(filelimits, filepart, 3);
 * .   form->setFormat(1,fileform); // NOT zero, see below
 * .   
 * .   Formattable testArgs[] = {12373L, "MyDisk"};
 * .    
 * .   UnicodeString string;
 * .   cout &lt;&lt; form->format(testArgs, string) &lt;&lt; endl;
 * .   
 * .   // output, with different testArgs
 * .   output: The disk "MyDisk" contains no files.
 * .   output: The disk "MyDisk" contains one file.
 * .   output: The disk "MyDisk" contains 1,273 files.
 * </pre>
 * You can either do this programmatically, as in the above example,
 * or by using a pattern (see ChoiceFormat for more information) as in:
 * <pre>
 * .   form->applyPattern(
 * .     "There {0,choice,0#are no files|1#is one file|1&lt;are {0,number,integer} files}.");
 * </pre>
 * <P>
 * [Note:] As we see above, the string produced by a ChoiceFormat in
 * MessageFormat is treated specially; occurances of '{' are used to
 * indicated subformats, and cause recursion.  If you create both a
 * MessageFormat and ChoiceFormat programmatically (instead of using
 * the string patterns), then be careful not to produce a format that
 * recurses on itself, which will cause an infinite loop.
 * <P>
 * [Note:] Formats are numbered by order of variable in the string.
 * This is [not] the same as the argument numbering!
 * <pre>
 * .   For example: with "abc{2}def{3}ghi{0}...",
 * .   
 * .   format0 affects the first variable {2}
 * .   format1 affects the second variable {3}
 * .   format2 affects the second variable {0}
 * </pre>
 * and so on.
 */
#ifdef NLS_MAC
#pragma export on
#endif
class T_FORMAT_API MessageFormat : public Format {
public:
    enum EFormatNumber { MAX_FORMAT = 10 };
    /**
     * Construct a new MessageFormat using the given pattern.
     *
     * @param pattern   Pattern used to construct object.
     * @param status    Output param to receive success code.  If the
     *                  pattern cannot be parsed, set to failure code.
     */
    MessageFormat(const UnicodeString& pattern,
                  ErrorCode &status);

    /**
     * Copy constructor.
     */
    MessageFormat(const MessageFormat&);

    /**
     * Assignment operator.
     */
    const MessageFormat& operator=(const MessageFormat&);

    /**
     * Destructor.
     */
    virtual ~MessageFormat();

    /**
     * Clone this Format object polymorphically. The caller owns the
     * result and should delete it when done.
     */
    virtual Format* clone() const;

    /**
     * Return true if the given Format objects are semantically equal.
     * Objects of different subclasses are considered unequal.
     */
    virtual t_bool operator==(const Format& other) const;

    /**
     * Return true if the given Format objects are not semantically equal.
     * Objects of different subclasses are considered unequal.
     */
    virtual t_bool operator!=(const Format& other) const;

    /**
     * Sets the locale. This locale is used for fetching default number or date
     * format information.
     */
    virtual void setLocale(const Locale& theLocale);

    /**
     * Gets the locale. This locale is used for fetching default number or date
     * format information.
     */
    virtual const Locale& getLocale() const;

    /**
     * Apply the given pattern string to this message format.
     *
     * @param pattern   The pattern to be applied.
     * @param status    Output param set to success/failure code on
     *                  exit. If the pattern is invalid, this will be
     *                  set to a failure result.
     */
    virtual void applyPattern(const UnicodeString& pattern,
                              ErrorCode& status);

    /**
     * Gets the pattern. See the class description.
     */
    virtual UnicodeString& toPattern(UnicodeString& result) const;

    /**
     * Sets formats to use on parameters.
     * See the class description about format numbering.
     * The caller should not delete the Format objects after this call.
     */
    virtual void adoptFormats(Format** formatsToAdopt, t_int32 count);

    /**
     * Sets formats to use on parameters.
     * See the class description about format numbering.
     */
    virtual void setFormats(const Format** newFormats,t_int32 cnt);


    /**
     * Sets formats individually to use on parameters.
     * See the class description about format numbering.
     * The caller should not delete the Format object after this call.
     */
    virtual void adoptFormat(t_int32 formatNumber, Format* formatToAdopt);

    /**
     * Sets formats individually to use on parameters.
     * See the class description about format numbering.
     */
    virtual void setFormat(t_int32 variable, const Format& newFormat);


    /**
     * Gets formats that were set with setFormats.
     * See the class description about format numbering.
     */
    virtual const Format** getFormats(t_int32& count) const;

    /**
     * Returns pattern with formatted objects.  Does not take ownership
     * of the Formattable* array; just reads it and uses it to generate
     * the format string.
     *
     * @param source    An array of objects to be formatted & substituted.
     * @param result    Where text is appended.
     * @param ignore    No useful status is returned.
     */
    UnicodeString& format(  const Formattable* source,
                            t_int32 count,
                            UnicodeString& result,
                            FieldPosition& ignore,
                            ErrorCode& success) const;

    /**
     * Convenience routine.  Avoids explicit creation of
     * MessageFormat, but doesn't allow future optimizations.
     */
    static UnicodeString& format(   const UnicodeString& pattern,
                                    const Formattable* arguments,
                                    t_int32 count,
                                    UnicodeString& result, 
                                    ErrorCode& success);

    /**
     * Format an object to produce a message.  This method handles
     * Formattable objects of type kArray. If the Formattable
     * object type is not of type kArray, then it returns a failing
     * ErrorCode.
     *
     * @param obj           The object to format
     * @param toAppendTo    Where the text is to be appended
     * @param pos           On input: an alignment field, if desired.
     *                      On output: the offsets of the alignment field.
     * @param status        Output param filled with success/failure status.
     * @return              The value passed in as toAppendTo (this allows chaining,
     *                      as with UnicodeString::append())
     */
    virtual UnicodeString& format(const Formattable& obj,
                                  UnicodeString& toAppendTo,
                                  FieldPosition& pos,
                                  ErrorCode& status) const;

    /**
     * Parses the string.
     * <P>
     * Caveats: The parse may fail in a number of circumstances.  For
     * example:
     * <P>
     * If one of the arguments does not occur in the pattern.
     * <P>
     * If the format of an argument is loses information, such as with
     * a choice format where a large number formats to "many".
     * <P>
     * Does not yet handle recursion (where the substituted strings
     * contain {n} references.)
     * <P>
     * Will not always find a match (or the correct match) if some
     * part of the parse is ambiguous.  For example, if the pattern
     * "{1},{2}" is used with the string arguments {"a,b", "c"}, it
     * will format as "a,b,c".  When the result is parsed, it will
     * return {"a", "b,c"}.
     * <P>
     * If a single argument is formatted twice in the string, then the
     * later parse wins.
     *
     * @param source    String to be parsed.
     * @param status    On input, starting position for parse. On output,
     *                  final position after parse.
     * @param count     Output param to receive size of returned array.
     * @result          Array of Formattable objects, with length
     *                  'count', owned by the caller.
     */
    virtual Formattable* parse( const UnicodeString& source,
                                ParsePosition& status,
                                t_int32& count) const;

    /**
     * Parses the string. Does not yet handle recursion (where
     * the substituted strings contain {n} references.)
     *
     * @param source    String to be parsed.
     * @param count     Output param to receive size of returned array.
     * @param status    Output param to receive success/error code.
     * @result          Array of Formattable objects, with length
     *                  'count', owned by the caller.
     */
    virtual Formattable* parse( const UnicodeString& source,
                                t_int32& count,
                                ErrorCode& status) const;

    /**
     * Parse a string to produce an object.  This methods handles
     * parsing of message strings into arrays of Formattable objects.
     * Does not yet handle recursion (where the substituted strings
     * contain %n references.)
     * <P>
     * Before calling, set parse_pos.index to the offset you want to
     * start parsing at in the source. After calling, parse_pos.index
     * is the end of the text you parsed.  If error occurs, index is
     * unchanged.
     * <P>
     * When parsing, leading whitespace is discarded (with successful
     * parse), while trailing whitespace is left as is.
     * <P>
     * See Format::parseObject() for more.
     *
     * @param source    The string to be parsed into an object.
     * @param result    Formattable to be set to the parse result.
     *                  If parse fails, return contents are undefined.
     * @param parse_pos The position to start parsing at. Upon return
     *                  this param is set to the position after the
     *                  last character successfully parsed. If the
     *                  source is not parsed successfully, this param
     *                  will remain unchanged.
     * @return          A newly created Formattable* object, or NULL
     *                  on failure.  The caller owns this and should
     *                  delete it when done.
     */
    virtual void parseObject(const UnicodeString& source,
                             Formattable& result,
                             ParsePosition& parse_pos) const;

public:
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
    virtual ClassID getDynamicClassID() const;

    /**
     * Return the class ID for this class.  This is useful only for
     * comparing to a return value from getDynamicClassID().  For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .      Derived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     */
    static ClassID getStaticClassID() { return (ClassID)&fgClassID; }

private:
    static char fgClassID;
    static NumberFormat* fgNumberFormat;

    Locale locale;
    UnicodeString pattern;
    // later, allow more than ten items
    Format *formats[MAX_FORMAT];
    t_int32 *offsets;
    t_int32 count;
    t_int32 *argumentNumbers;
    t_int32 maxOffset;
    UnicodeString buffer;

    /**
     * Internal routine used by format.
     * @param recursionProtection Initially zero. Bits 0..9 are used to indicate
     * that a parameter has already been seen, to avoid recursion.  Currently
     * unused.
     */
    static const int LIST_LENGTH;
    static const UnicodeString typeList[];
    static const UnicodeString modifierList[];
    static const UnicodeString dateModifierList[];

    /**
     * Internal ctor that allows the locale specification.
     * @param pattern   Pattern used to construct object.
     * @param newLocale the new locale.
     * @param status    Output param to receive success code.  If the
     *                  pattern cannot be parsed, set to failure code.
     */
    MessageFormat(const UnicodeString& pattern,
                             const Locale& newLocale,
                             ErrorCode& success);

    /** 
     * Finds the word s, in the keyword list and returns the located index.
     * @param s the keyword to be searched for.
     * @param list the list of keywords to be searched with.
     * @return the index of the list which matches the keyword s.
     */
    static int findKeyword( const UnicodeString& s, 
                            const UnicodeString* list);

    /**
     * Formats the array of arguments and copies the result into the result buffer,
     * updates the field position.
     * @param arguments the formattable objects array.
     * @param cnt the array count.
     * @param status field position status.
     * @param recursionProtection Initially zero. Bits 0..9 are used to indicate
     * that a parameter has already been seen, to avoid recursion.  Currently
     * unused.
     * @param success the error code status.
     */
    UnicodeString&  format( const Formattable* arguments, 
                            t_int32 cnt, 
                            UnicodeString& result, 
                            FieldPosition& status, 
                            int recursionProtection,
                            ErrorCode& success) const;

    /**
     * Checks the segments for the closest matched format instance and
     * updates the format array with the new format instance.
     * @param position the last processed offset in the pattern 
     * @param offsetNumber the offset number of the last processed segment
     * @param segments the string that contains the parsed pattern segments.
     * @param success the error code
     */
    void            makeFormat( int position, 
                                int offsetNumber, 
                                UnicodeString* segments, 
                                ErrorCode& success);

    /**
     * Checks the range of the source text to quote the special
     * characters, { and ' and copy to target buffer.
     * @param source
     * @param start the text offset to start the process of in the source string
     * @param end the text offset to end the process of in the source string
     * @param target the result buffer
     */
    static void copyAndFixQuotes(const UnicodeString& source, int start, int end, UnicodeString& target);

    /**
     * Converts a string to an integer value using a default NumberFormat object
     * which is static (shared by all MessageFormat instances).  This replaces
     * a call to wtoi().
     * @param string the source string to convert with
     * @param status the error code.
     * @return the converted number.
     */
    static t_int32 stoi(const UnicodeString& string, ErrorCode& status);
};
#ifdef NLS_MAC
#pragma export off
#endif
 
inline ClassID 
MessageFormat::getDynamicClassID() const
{ 
    return MessageFormat::getStaticClassID(); 
}

inline t_bool
MessageFormat::operator!= (const Format& that) const
{
    return !(*this == that);
}

#endif // _MSGFMT
//eof
