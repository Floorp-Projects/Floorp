/*
*****************************************************************************************
*																						*
* COPYRIGHT:																			*
*	(C) Copyright Taligent, Inc.,  1997													*
*	(C) Copyright International Business Machines Corporation,	1997					*
*	Licensed Material - Program-Property of IBM - All Rights Reserved.					*
*	US Government Users Restricted Rights - Use, duplication, or disclosure				*
*	restricted by GSA ADP Schedule Contract with IBM Corp.								*
*																						*
*****************************************************************************************
*
* File BRKITER.H
*
* Modification History:
*
*	Date		Name		Description
*	02/18/97	aliu		Added typedef for TextCount.  Made DONE const.
*	05/07/97	aliu		Fixed DLL declaration.
*   07/09/97	jfitz		Renamed BreakIterator and interface synced with JDK
*****************************************************************************************
*/

#ifndef _BRKITER
#define _BRKITER

#include "unistring.h"
#include "locid.h"
class Locale;

/**
 * The BreakIterator class implements methods for finding the location
 * of boundaries in text. BreakIterator is an abstract base class.
 * Instances of BreakIterator maintain a current position and scan over
 * text returning the index of characters where boundaries occur.
 * <P>
 * Line boundary analysis determines where a text string can be broken
 * when line-wrapping. The mechanism correctly handles punctuation and
 * hyphenated words.
 * <P>
 * Sentence boundary analysis allows selection with correct
 * interpretation of periods within numbers and abbreviations, and
 * trailing punctuation marks such as quotation marks and parentheses.
 * <P>
 * Word boundary analysis is used by search and replace functions, as
 * well as within text editing applications that allow the user to
 * select words with a double click. Word selection provides correct
 * interpretation of punctuation marks within and following
 * words. Characters that are not part of a word, such as symbols or
 * punctuation marks, have word-breaks on both sides.
 * <P>
 * Character boundary analysis allows users to interact with
 * characters as they expect to, for example, when moving the cursor
 * through a text string. Character boundary analysis provides correct
 * navigation of through character strings, regardless of how the
 * character is stored.  For example, an accented character might be
 * stored as a base character and a diacritical mark. What users
 * consider to be a character can differ between languages.
 * <P>
 * This is the interface for all text boundaries.
 * <P>
 * Examples:
 * <P>
 * Print each element in order:
 * <pre>
 * .   void printEachForward(const BreakIterator& boundary) {
 * .      UnicodeString textChunk;
 * .      TextOffset start = boundary.first();
 * .      for (TextOffset end = boundary.next();
 * .        end != BreakIterator::DONE;
 * .        start = end, end = boundary.next())
 * .        {
 * .            cout &lt;&lt; boundary.getText()->extract(start, end, textChunk);
 * .        }
 * .   }
 * </pre>
 * Print each element in reverse order:
 * <pre>
 * .   void printEachbackward(const BreakIterator& boundary) {
 * .      UnicodeString textChunk;
 * .      TextOffset end = boundary.last();
 * .      for (TextOffset start = boundary.previous();
 * .        start != BreakIterator::DONE;
 * .        end = start, start = boundary.previous())
 * .        {
 * .            cout &lt;&lt; boundary.getText()->extract(start, end, textChunk);
 * .        }
 * .   }
 * </pre>
 */
class T_FINDWORD_API BreakIterator {
public:
	virtual ~BreakIterator();

    /**
     * Return true if another object is semantically equal to this
     * one. The other object should be an instance of the same subclass of
     * BreakIterator. Objects of different subclasses are considered
     * unequal.
     * <P>
     * Return true if this BreakIterator is at the same position in the
     * same text, and is the same class and type (word, line, etc.) of
     * BreakIterator, as the argument.  Text is considered the same if
     * it contains the same characters, it need not be the same
     * object, and styles are not considered.
     */
	virtual t_bool operator==(const BreakIterator&) const = 0;

	t_bool operator!=(const BreakIterator& rhs) const { return !operator==(rhs); }

	/**
	 * Return a polymorphic copy of this object.  This is an abstract
	 * method which subclasses implement.
	 */
	virtual BreakIterator* clone() const = 0;

	/**
	 * Return a polymorphic class ID for this object. Different subclasses
	 * will return distinct unequal values.
	 */
	virtual ClassID getDynamicClassID() const = 0;

	/**
	 * Get the text for which this object is finding the boundaries.
	 */
	virtual const UnicodeString* getText() const = 0;

	/**
	 * Set the text for which this object should find the boundaries.
	 */
	virtual void setText(const UnicodeString*) = 0;

	/**
	 * DONE is returned by previous() and next() after all valid
	 * boundaries have been returned.
	 */
	static const TextOffset DONE;

	/**
	 * Return the index of the first character in the text being scanned.
	 */
	virtual TextOffset first() = 0;

	/**
	 * Return the index immediately BEYOND the last character in the text being scanned.
	 */
	virtual TextOffset last() = 0;

	/**
	 * Return the boundary preceding the current boundary.
	 * @return The character index of the previous text boundary or DONE if all
	 * boundaries have been returned.
	 */
	virtual TextOffset previous() = 0;

	/**
	 * Return the boundary following the current boundary.
	 * @return The character index of the next text boundary or DONE if all
	 * boundaries have been returned.
	 */
	virtual TextOffset next() = 0;

	/**
	 * Return character index of the text boundary that was most recently
	 * returned by next(), previous(), first(), or last()
	 * @return The boundary most recently returned.
	 */
	virtual TextOffset current() const = 0;

	/**
	 * Return the first boundary following the specified offset.
	 * The value returned is always greater than the offset or
	 * the value BreakIterator.DONE
	 * @param offset the offset to begin scanning.
	 * @return The first boundary after the specified offset.
	 */
	virtual TextOffset following(TextOffset offset) = 0;

	/**
	 * Return the nth boundary from the current boundary
	 * @param n which boundary to return.  A value of 0
	 * does nothing.  Negative values move to previous boundaries
	 * and positive values move to later boundaries.
	 * @return The index of the nth boundary from the current position, or
	 * DONE if there are fewer than |n| boundaries in the specfied direction.
	 */
	virtual TextOffset next(t_int32 n) = 0;

	/**
	 * Create BreakIterator for word-breaks using the given locale.
	 * Returns an instance of a BreakIterator implementing word breaks.
	 * WordBreak is useful for word selection (ex. double click)
	 * @param where the locale.	If a specific WordBreak is not
	 * avaliable for the specified locale, a default WordBreak is returned.
	 * @return A BreakIterator for word-breaks
	 */
	static BreakIterator* createWordInstance(const Locale& where = Locale::getDefault());

	/**
	 * Create BreakIterator for line-breaks using specified locale.
	 * Returns an instance of a BreakIterator implementing line breaks. Line
	 * breaks are logically possible line breaks, actual line breaks are
	 * usually determined based on display width.
	 * LineBreak is useful for word wrapping text.
	 * @param where the locale.	If a specific LineBreak is not
	 * avaliable for the specified locale, a default LineBreak is returned.
	 * @return A BreakIterator for line-breaks
	 */
	static BreakIterator* createLineInstance(const Locale& where = Locale::getDefault());

	/**
	 * Create BreakIterator for character-breaks using specified locale
	 * Returns an instance of a BreakIterator implementing character breaks.
	 * Character breaks are boundaries of combining character sequences.
	 * @param where the locale.	If a specific character break is not
	 * avaliable for the specified locale, a default character break is returned.
	 * @return A BreakIterator for character-breaks
	 */
	static BreakIterator* createCharacterInstance(const Locale& where = Locale::getDefault());

	/**
	 * Create BreakIterator for sentence-breaks using specified locale
	 * Returns an instance of a BreakIterator implementing sentence breaks.
	 * @param where the locale.	If a specific SentenceBreak is not
	 * avaliable for the specified locale, a default SentenceBreak is returned.
	 * @return A BreakIterator for sentence-breaks
	 */
	static BreakIterator* createSentenceInstance(const Locale& where = Locale::getDefault());

	/**
	 * Get the set of Locales for which TextBoundaries are installed
	 * @param count the output parameter of number of elements in the locale list
	 * @return available locales
	 */
	static const Locale* getAvailableLocales(t_int32& count);

	/**
	 * Get name of the object for the desired Locale, in the desired langauge.
	 * @param objectLocale must be from getAvailableLocales.
	 * @param displayLocale specifies the desired locale for output.
	 * @param name the fill-in parameter of the return value
	 * Uses best match.
	 * @return user-displayable name
	 */
	static UnicodeString& getDisplayName(const Locale& objectLocale,
										 const Locale& displayLocale,
										 UnicodeString& name);

	/**
	 * Get name of the object for the desired Locale, in the langauge of the
	 * default locale.
	 * @param objectLocale must be from getMatchingLocales
	 * @param name the fill-in parameter of the return value
	 * @return user-displayable name
	 */
	static UnicodeString& getDisplayName(const Locale& objectLocale,
										 UnicodeString& name);



// begin deprecated api.
	virtual TextOffset nextAfter(TextOffset offset) = 0;
	virtual TextOffset nthFromCurrent(t_int32 n) = 0;
    static BreakIterator* createWordBreak(const Locale& where = Locale::getDefault()) { return createWordInstance(); }
    static BreakIterator* createLineBreak(const Locale& where = Locale::getDefault()) { return createLineInstance(); }
    static BreakIterator* createCharacterBreak(const Locale& where = Locale::getDefault()) { return createCharacterInstance(); }
    static BreakIterator* createSentenceBreak(const Locale& where = Locale::getDefault()) { return createSentenceInstance(); }
// end deprecated api.

protected:
	BreakIterator();

private:
	/**
	 * The copy constructor and assignment operator have no real implementation.
	 * They are provided to make the compiler happy. Do not call.
	 */
    BreakIterator& operator=(const BreakIterator& other) { return *this; }
    BreakIterator (const BreakIterator& other) {}
};

#endif // _BRKITER
//eof
