/**
 * (C) Copyright Taligent, Inc.  1996 - All Rights Reserved

 This source code and documentation are Taligent Confidential materials.
 These materials are provided for internal evaluation purposes only.  This
 notice may not be removed.  This technology is protected by multiple US and
 International patents.
 */

#ifndef _PTYPES
#include "ptypes.h"
#endif

#ifndef _UNISTRING
#include "unistring.h"
#endif

/**
 * Abstract class defining a protcol for accessing characters in a text-storage object.
  <P>Examples:<P>

  Traverse the text from start to finish
  <pre>
 .void traverseForward(CharacterIterator& iter)
 .{
 .  for (UniChar c = iter.first(); c != CharacterIterator::DONE; c = iter.next()) {
 .      processChar(c);
 .  }
 .}
  </pre>

  Traverse the text backwards, from end to start
  <pre>
 .void traverseBackward(CharacterIterator& iter)
 .{
 .  for (UniChar c = iter.last(); c != CharacterIterator::DONE; c = iter.prev()) {
 .     processChar(c);
 .  }
 .}
  </pre>

  Traverse both forward and backward from a given position in the text.
  <pre>
 .void traverseOut(CharacterIterator& iter, t_int32 pos)
 .{
 .  UnicodeString temp;
 .  UnicodeString temp2;
      
 .  for (UniChar c = iter.setIndex(pos); c != CharacterIterator::DONE && notBoundary(c); c = iter.next()) {}
 .  t_int32 end = iter.getIndex();
 .  for (c = iter.setIndex(pos); c != CharacterIterator::DONE && notBoundary(c); c = iter.prev()) {}
 .  t_int32 start = iter.getIndex();
 .  iter.getText(temp);
 .  processSection(temp.extract(start,end, temp2));
 .}
  </pre>
 */
#ifdef NLS_MAC
#pragma export on
#endif

class T_UTILITY_API CharacterIterator
{
	public:
		/**
		 * Value returned by most of CharacterIterator's functions when the iterator
		 * has reached the limits of its iteration.
		 */
		static const UniChar	DONE;

		/**
		 * Returns true when both iterators refer to the same character in the same
		 * character-storage object.
		 */
		virtual t_bool			operator==(const CharacterIterator& that) const = 0;
		
		/**
		 * Returns true when the iterators refer to different text-storage objects, or
		 * to different characters in the same text-storage object.
		 */
		t_bool					operator!=(const CharacterIterator& that) const { return !operator==(that); }

		/**
		 * Returns a pointer to a new CharacterIterator of the same concrete class as this
		 * one, and referring to the same character in the same text-storage object as
		 * this one.  The caller is responsible for deleting the new clone.
		 */
		virtual CharacterIterator*
								clone() const = 0;

		/**
		 * Generates a hash code for this iterator.
		 */
		virtual t_int32			hashCode() const = 0;
		
		/**
		 * Sets the iterator to refer to the first character in its iteration range, and
		 * returns that character,
		 */
		virtual UniChar			first() = 0;
		
		/**
		 * Sets the iterator to refer to the last character in its iteration range, and
		 * returns that character.
		 */
		virtual UniChar			last() = 0;
		
		/**
		 * Sets the iterator to refer to the "position"-th character in the text-storage
		 * object the iterator refers to, and returns that character.
		 */
		virtual UniChar			setIndex(TextOffset position) = 0;

		/**
		 * Returns the character the iterator currently refers to.
		 */
		virtual UniChar			current() const = 0;
		
		/**
		 * Advances to the next character in the iteration range (toward last()), and
		 * returns that character.  If there are no more characters to return, returns DONE.
		 */
		virtual UniChar			next() = 0;
		
		/**
		 * Advances to the previous character in the iteration rance (toward first()), and
		 * returns that character.  If there are no more characters to return, returns DONE.
		 */
		virtual UniChar			previous() = 0;

		/**
		 * Returns the numeric index in the underlying text-storage object of the character
		 * returned by first().  Since it's possible to create an iterator that iterates
		 * across only part of a text-storage object, this number isn't necessarily 0.
		 */
		virtual TextOffset		startIndex() const = 0;
		
		/**
		 * Returns the numeric index in the underlying text-storage object of the position
		 * immediately BEYOND the character returned by last().
		 */
		virtual TextOffset		endIndex() const = 0;
		
		/**
		 * Returns the numeric index in the underlying text-storage object of the character
		 * the iterator currently refers to (i.e., the character returned by current()).
		 */
		virtual TextOffset		getIndex() const = 0;

		/**
		 * Copies the text under iteration into the UnicodeString referred to by "result".
		 * @param result Receives a copy of the text under iteration.
		 */
		virtual void			getText(UnicodeString&	result) = 0;

        /**
         * Returns a ClassID for this CharacterIterator ("poor man's RTTI").<P>
         * Despite the fact that this function is public, DO NOT CONSIDER IT PART OF
         * CHARACTERITERATOR'S API! 
         */
		virtual ClassID			getDynamicClassID() const = 0;

	protected:
								CharacterIterator() {}
								CharacterIterator(const CharacterIterator&) {}
		CharacterIterator&		operator=(const CharacterIterator&) { return *this; }

};

#ifdef NLS_MAC
#pragma export off
#endif
