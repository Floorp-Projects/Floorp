/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsStringIterator_h___
#define nsStringIterator_h___

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
#endif

#ifndef nsAlgorithm_h___
#include "nsAlgorithm.h"
#endif

#ifndef nsDebug_h___
#include "nsDebug.h"
#endif

  /**
   * @see nsTAString
   */

template <class CharT>
class nsReadingIterator
  {
    public:
      typedef nsReadingIterator<CharT>    self_type;
      typedef ptrdiff_t                   difference_type;
      typedef CharT                       value_type;
      typedef const CharT*                pointer;
      typedef const CharT&                reference;

    private:
      friend class nsAString;
      friend class nsACString;
      friend class nsSubstring;
      friend class nsCSubstring;

        // unfortunately, the API for nsReadingIterator requires that the
        // iterator know its start and end positions.  this was needed when
        // we supported multi-fragment strings, but now it is really just
        // extra baggage.  we should remove mStart and mEnd at some point.

      const CharT* mStart;
      const CharT* mEnd;
      const CharT* mPosition;

    public:
      nsReadingIterator() { }
      // nsReadingIterator( const nsReadingIterator<CharT>& );                    // auto-generated copy-constructor OK
      // nsReadingIterator<CharT>& operator=( const nsReadingIterator<CharT>& );  // auto-generated copy-assignment operator OK

      inline void normalize_forward() {}
      inline void normalize_backward() {}

      pointer
      start() const
        {
          return mStart;
        }

      pointer
      end() const
        {
          return mEnd;
        }

      pointer
      get() const
        {
          return mPosition;
        }
      
      CharT
      operator*() const
        {
          return *get();
        }

#if 0
        // An iterator really deserves this, but some compilers (notably IBM VisualAge for OS/2)
        //  don't like this when |CharT| is a type without members.
      pointer
      operator->() const
        {
          return get();
        }
#endif

      self_type&
      operator++()
        {
          ++mPosition;
          return *this;
        }

      self_type
      operator++( int )
        {
          self_type result(*this);
          ++mPosition;
          return result;
        }

      self_type&
      operator--()
        {
          --mPosition;
          return *this;
        }

      self_type
      operator--( int )
        {
          self_type result(*this);
          --mPosition;
          return result;
        }

      difference_type
      size_forward() const
        {
          return mEnd - mPosition;
        }

      difference_type
      size_backward() const
        {
          return mPosition - mStart;
        }

      self_type&
      advance( difference_type n )
        {
          if (n > 0)
            {
              difference_type step = NS_MIN(n, size_forward());

              NS_ASSERTION(step>0, "can't advance a reading iterator beyond the end of a string");

              mPosition += step;
            }
          else if (n < 0)
            {
              difference_type step = NS_MAX(n, -size_backward());

              NS_ASSERTION(step<0, "can't advance (backward) a reading iterator beyond the end of a string");

              mPosition += step;
            }
          return *this;
        }
  };

  /**
   * @see nsTAString
   */

template <class CharT>
class nsWritingIterator
  {
    public:
      typedef nsWritingIterator<CharT>   self_type;
      typedef ptrdiff_t                  difference_type;
      typedef CharT                      value_type;
      typedef CharT*                     pointer;
      typedef CharT&                     reference;

    private:
      friend class nsAString;
      friend class nsACString;
      friend class nsSubstring;
      friend class nsCSubstring;

        // unfortunately, the API for nsWritingIterator requires that the
        // iterator know its start and end positions.  this was needed when
        // we supported multi-fragment strings, but now it is really just
        // extra baggage.  we should remove mStart and mEnd at some point.

      CharT* mStart;
      CharT* mEnd;
      CharT* mPosition;

    public:
      nsWritingIterator() { }
      // nsWritingIterator( const nsWritingIterator<CharT>& );                    // auto-generated copy-constructor OK
      // nsWritingIterator<CharT>& operator=( const nsWritingIterator<CharT>& );  // auto-generated copy-assignment operator OK

      inline void normalize_forward() {}
      inline void normalize_backward() {}

      pointer
      start() const
        {
          return mStart;
        }

      pointer
      end() const
        {
          return mEnd;
        }

      pointer
      get() const
        {
          return mPosition;
        }
      
      reference
      operator*() const
        {
          return *get();
        }

#if 0
        // An iterator really deserves this, but some compilers (notably IBM VisualAge for OS/2)
        //  don't like this when |CharT| is a type without members.
      pointer
      operator->() const
        {
          return get();
        }
#endif

      self_type&
      operator++()
        {
          ++mPosition;
          return *this;
        }

      self_type
      operator++( int )
        {
          self_type result(*this);
          ++mPosition;
          return result;
        }

      self_type&
      operator--()
        {
          --mPosition;
          return *this;
        }

      self_type
      operator--( int )
        {
          self_type result(*this);
          --mPosition;
          return result;
        }

      difference_type
      size_forward() const
        {
          return mEnd - mPosition;
        }

      difference_type
      size_backward() const
        {
          return mPosition - mStart;
        }

      self_type&
      advance( difference_type n )
        {
          if (n > 0)
            {
              difference_type step = NS_MIN(n, size_forward());

              NS_ASSERTION(step>0, "can't advance a writing iterator beyond the end of a string");

              mPosition += step;
            }
          else if (n < 0)
            {
              difference_type step = NS_MAX(n, -size_backward());

              NS_ASSERTION(step<0, "can't advance (backward) a writing iterator beyond the end of a string");

              mPosition += step;
            }
          return *this;
        }

      PRUint32
      write( const value_type* s, PRUint32 n )
        {
          NS_ASSERTION(size_forward() > 0, "You can't |write| into an |nsWritingIterator| with no space!");

          nsCharTraits<value_type>::move(mPosition, s, n);
          advance( difference_type(n) );
          return n;
        }
  };

template <class CharT>
inline
PRBool
operator==( const nsReadingIterator<CharT>& lhs, const nsReadingIterator<CharT>& rhs )
  {
    return lhs.get() == rhs.get();
  }

template <class CharT>
inline
PRBool
operator!=( const nsReadingIterator<CharT>& lhs, const nsReadingIterator<CharT>& rhs )
  {
    return lhs.get() != rhs.get();
  }


  //
  // |nsWritingIterator|s
  //

template <class CharT>
inline
PRBool
operator==( const nsWritingIterator<CharT>& lhs, const nsWritingIterator<CharT>& rhs )
  {
    return lhs.get() == rhs.get();
  }

template <class CharT>
inline
PRBool
operator!=( const nsWritingIterator<CharT>& lhs, const nsWritingIterator<CharT>& rhs )
  {
    return lhs.get() != rhs.get();
  }

#endif /* !defined(nsStringIterator_h___) */
