/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

/* nsStringFragment.h --- machinery that makes string iterators work */

#ifndef nsStringFragment_h___
#define nsStringFragment_h___



  /**
   * An |nsFragmentRequest| is used to tell |GetReadableFragment| and
   * |GetWritableFragment| what to do.
   *
   * @see GetReadableFragment
   */

enum nsFragmentRequest { kPrevFragment, kFirstFragment, kLastFragment, kNextFragment, kFragmentAt };


  /**
   * A |nsReadableFragment| provides |const| access to a contiguous hunk of
   * string of homogenous units, e.g., bytes (|char|).  This doesn't mean it
   * represents a flat hunk.  It could be a variable length encoding, for
   * instance UTF-8.  And the fragment itself need not be zero-terminated.
   *
   * An |nsReadableFragment| is the underlying machinery that lets
   * |nsReadingIterator|s work.
   *
   * @see nsReadingIterator
   */

template <class CharT>
struct nsReadableFragment
  {
    const CharT*  mStart;
    const CharT*  mEnd;
    const void*   mFragmentIdentifier;

    nsReadableFragment()
        : mStart(0), mEnd(0), mFragmentIdentifier(0)
      {
        // nothing else to do here
      }
  };


  /**
   * A |nsWritableFragment| provides non-|const| access to a contiguous hunk of
   * string of homogenous units, e.g., bytes (|char|).  This doesn't mean it
   * represents a flat hunk.  It could be a variable length encoding, for
   * instance UTF-8.  And the fragment itself need not be zero-terminated.
   *
   * An |nsWritableFragment| is the underlying machinery that lets
   * |nsWritingIterator|s work.
   *
   * @see nsWritingIterator
   */

template <class CharT>
struct nsWritableFragment
  {
    CharT*    mStart;
    CharT*    mEnd;
    void*     mFragmentIdentifier;

    nsWritableFragment()
        : mStart(0), mEnd(0), mFragmentIdentifier(0)
      {
        // nothing else to do here
      }
  };

#endif /* !defined(nsStringFragment_h___) */
