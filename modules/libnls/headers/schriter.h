/*
(C) Copyright Taligent, Inc. 1996 - All Rights Reserved
(C) Copyright IBM Corp. 1996 - All Rights Reserved

  The original version of this source code and documentation is copyrighted and
owned by Taligent, Inc., a wholly-owned subsidiary of IBM. These materials are
provided under terms of a License Agreement between Taligent and Sun. This
technology is protected by multiple US and International patents. This notice and
attribution to Taligent may not be removed.
  Taligent is a registered trademark of Taligent, Inc.
*/

#include "ptypes.h"
#include "chariter.h"

/**
 * A concrete subclass of CharacterIterator that iterates over the characters in
 * a UnicodeString.  It's possible not only to create an iterator that iterates over
 * an entire UnicodeString, but also to create only that iterates over only a subrange
 * of a UnicodeString (iterators over different subranges of the same UnicodeString
 * don't compare equal).
 */

#ifdef NLS_MAC
#pragma export on
#endif

class T_UTILITY_API StringCharacterIterator : public CharacterIterator {
	public:
		/**
		 * Create an iterator over the UnicodeString referred to by "text".
		 * The iteration range is the whole string, and the starting position is 0.
		 */
								StringCharacterIterator(const UnicodeString& text);

		/**
		 * Create an iterator over the UnicodeString referred to by "text".
		 * The iteration range is the whole string, and the starting position is
		 * specified by "pos".  If "pos" is outside the valid iteration range, the
		 * behavior of this object is undefined.
		 */
								StringCharacterIterator(const UnicodeString&	text,
														TextOffset				pos);

		/**
		 * Create an iterator over the UnicodeString referred to by "text".
		 * The iteration range begins with the character specified by "begin" and
		 * ends with the character BEFORE the character specfied by "end".  The
		 * starting position is specified by "pos".  If "begin" and "end" don't form
		 * a valid range on "text" (i.e., begin >= end or either is negative or
		 * greater than text.size()), or "pos" is outside the range defined by "begin"
		 * and "end", the behavior of this iterator is undefined.
		 */
								StringCharacterIterator(const UnicodeString&	text,
														TextOffset				begin,
														TextOffset				end,
														TextOffset				pos);

		/**
		 * Copy constructor.  The new iterator iterates over the same range of the same
		 * string as "that", and its initial position is the same as "that"'s current
		 * position.
		 */
								StringCharacterIterator(const StringCharacterIterator&	that);
		/**
		 * Assignment operator.  *this is altered to iterate over the sane range of the
		 * same string as "that", and refers to the same character within that string
		 * as "that" does.
		 */
		StringCharacterIterator&
								operator=(const StringCharacterIterator&	that);

		/**
		 * Returns true if the iterators iterate over the same range of the same
		 * string and are pointing at the same character.
		 */
		virtual	t_bool			operator==(const CharacterIterator& that) const;

		/**
		 * Generates a hash code for this iterator.
		 */
		virtual t_int32			hashCode() const;

		/**
		 * Returns a new StringCharacterIterator referring to the same character in the
		 * same range of the same string as this one.  The caller must delete the new
		 * iterator.
		 */
		virtual CharacterIterator* clone() const;

		/**
		 * Sets the iterator to refer to the first character in its iteration range, and
		 * returns that character,
		 */
		virtual UniChar			first();

		/**
		 * Sets the iterator to refer to the last character in its iteration range, and
		 * returns that character.
		 */
		virtual UniChar			last();

		/**
		 * Sets the iterator to refer to the "position"-th character in the UnicodeString
		 * the iterator refers to, and returns that character.  If the index is outside
		 * the iterator's iteration range, the behavior of the iterator is undefined.
		 */
		virtual UniChar			setIndex(TextOffset pos);

		/**
		 * Returns the character the iterator currently refers to.
		 */
		virtual UniChar			current() const;

		/**
		 * Advances to the next character in the iteration range (toward last()), and
		 * returns that character.  If there are no more characters to return, returns DONE.
		 */
		virtual UniChar			next();

		/**
		 * Advances to the previous character in the iteration rance (toward first()), and
		 * returns that character.  If there are no more characters to return, returns DONE.
		 */
		virtual UniChar			previous();

		/**
		 * Returns the numeric index of the first character in this iterator's iteration
		 * range.
		 */
		virtual TextOffset		startIndex() const;

		/**
		 * Returns the numeric index of the character immediately BEYOND the last character
		 * in this iterator's iteration range.
		 */
		virtual TextOffset		endIndex() const;

		/**
		 * Returns the numeric index in the underlying UnicodeString of the character
		 * the iterator currently refers to (i.e., the character returned by current()).
		 */
		virtual TextOffset		getIndex() const;

		/**
		 * Copies the UnicodeString under iteration into the UnicodeString referred to by
		 * "result".  Even if this iterator iterates across only a part of this string,
		 * the whole string is copied.
		 * @param result Receives a copy of the text under iteration.
		 */
		virtual void			getText(UnicodeString& result);

		/**
		 * Return a class ID for this object (not really public)
		 */
		virtual ClassID			getDynamicClassID() const { return getStaticClassID(); }

		/**
		 * Return a class ID for this class (not really public)
		 */
		static ClassID			getStaticClassID() { return (ClassID)(&fgClassID); }

	private:
								StringCharacterIterator();
		
		const UnicodeString*	text;
		TextOffset				pos;
		TextOffset				begin;
		TextOffset				end;

		static char				fgClassID;
};

#ifdef NLS_MAC
#pragma export off
#endif
