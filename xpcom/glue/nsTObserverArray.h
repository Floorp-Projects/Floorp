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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsTObserverArray_h___
#define nsTObserverArray_h___

#include "nsVoidArray.h"

class NS_COM_GLUE nsTObserverArray_base {
  public:
    class Iterator_base;
    friend class Iterator_base;

    class Iterator_base {
      protected:
        friend class nsTObserverArray_base;

        Iterator_base(PRInt32 aPosition, const nsTObserverArray_base& aArray)
          : mPosition(aPosition),
            mNext(aArray.mIterators),
            mArray(aArray) {
          aArray.mIterators = this;
        }

        ~Iterator_base() {
          NS_ASSERTION(mArray.mIterators == this,
                       "Iterators must currently be destroyed in opposite order "
                       "from the construction order. It is suggested that you "
                       "simply put them on the stack");
          mArray.mIterators = mNext;
        }

        // These functions exists solely to avoid having to make the
        // subclasses into friends of nsTObserverArray_base
        void* GetSafeElementAt(PRInt32 aIndex) {
          return mArray.mObservers.SafeElementAt(aIndex);
        }
        void* FastElementAt(PRInt32 aIndex) {
          return mArray.mObservers.FastElementAt(aIndex);
        }

        // The current position of the iterator. It's exact meaning differs
        // depending on if the array is iterated forwards or backwards. See
        // nsTObserverArray<T>::ForwardIterator and
        // nsTObserverArray<T>::ReverseIterator
        PRInt32 mPosition;

        // The next iterator currently iterating the same array
        Iterator_base* mNext;

        // The array we're iterating
        const nsTObserverArray_base& mArray;
    };

    /**
     * Removes all observers and collapses all iterators to the beginning of
     * the array. The result is that forward iterators will see all elements
     * in the array, and backward iterators will not see any more elements.
     */
    void Clear();

  protected:
    nsTObserverArray_base()
      : mIterators(nsnull) {
    }

    /**
     * Adjusts iterators after an element has been inserted or removed
     * from the array.
     * @param modPos     Position where elements were added or removed.
     * @param adjustment -1 if an element was removed, 1 if an element was
     *                   added.
     */
    void AdjustIterators(PRInt32 aModPos, PRInt32 aAdjustment);

    mutable Iterator_base* mIterators;
    nsVoidArray mObservers;
};

/**
 * An array of observers. Like a normal array, but supports iterators that are
 * stable even if the array is modified during iteration.
 * The template parameter is the type of observer the array will hold pointers
 * to.
 */

template<class T>
class nsTObserverArray : public nsTObserverArray_base {
  public:

    PRUint32 Count() const {
      return mObservers.Count();
    }

    /**
     * Adds an observer to the beginning of the array
     * @param aObserver Observer to add
     */
    PRBool PrependObserver(T* aObserver) {
      NS_PRECONDITION(!Contains(aObserver),
                      "Don't prepend if the observer is already in the list");

      PRBool res = mObservers.InsertElementAt(aObserver, 0);
      if (res) {
        AdjustIterators(0, 1);
      }
      return res;
    }

    /**
     * Adds an observer to the end of the array unless it already exists in
     * the array.
     * @param aObserver Observer to add
     * @return True on success, false otherwise
     */
    PRBool AppendObserverUnlessExists(T* aObserver) {
      return Contains(aObserver) || mObservers.AppendElement(aObserver);
    }

    /**
     * Adds an observer to the end of the array.
     * @param aObserver Observer to add
     * @return True on success, false otherwise
     */
    PRBool AppendObserver(T* aObserver) {
      return mObservers.AppendElement(aObserver);
    }

    /**
     * Removes an observer from the array
     * @param aObserver Observer to remove
     * @return True if observer was found and removed, false otherwise
     */
    PRBool RemoveObserver(T* aObserver) {
      PRInt32 index = mObservers.IndexOf(aObserver);
      if (index < 0) {
        return PR_FALSE;
      }

      mObservers.RemoveElementAt(index);
      AdjustIterators(index, -1);

      return PR_TRUE;
    }

    /**
     * Removes an observer from the array
     * @param aIndex Index of observer to remove
     */
    void RemoveObserverAt(PRUint32 aIndex) {
      if (aIndex < (PRUint32)mObservers.Count()) {
        mObservers.RemoveElementAt(aIndex);
        AdjustIterators(aIndex, -1);
      }
    }

    PRBool Contains(T* aObserver) const {
      return mObservers.IndexOf(aObserver) >= 0;
    }

    PRBool IsEmpty() const {
      return mObservers.Count() == 0;
    }

    T* SafeObserverAt(PRInt32 aIndex) const {
      return static_cast<T*>(mObservers.SafeElementAt(aIndex));
    }

    T* FastObserverAt(PRInt32 aIndex) const {
      return static_cast<T*>(mObservers.FastElementAt(aIndex));
    }

    /**
     * Iterators
     */

    // Iterates the array forward from beginning to end.
    // mPosition points to the element that will be returned on next call
    // to GetNext
    class ForwardIterator : public nsTObserverArray_base::Iterator_base {
      public:
        ForwardIterator(const nsTObserverArray<T>& aArray)
          : Iterator_base(0, aArray) {
        }
        ForwardIterator(const nsTObserverArray<T>& aArray, PRInt32 aPos)
          : Iterator_base(aPos, aArray) {
        }

        PRBool operator <(const ForwardIterator& aOther) {
          NS_ASSERTION(&mArray == &aOther.mArray,
                       "not iterating the same array");
          return mPosition < aOther.mPosition;
        }

        /**
         * Returns the next element and steps one step.
         * Returns null if there are no more observers. Once null is returned
         * the iterator becomes invalid and GetNext must not be called any more.
         * @return The next observer.
         */
        T* GetNext() {
          return static_cast<T*>(GetSafeElementAt(mPosition++));
        }
    };

    // EndLimitedIterator works like ForwardIterator, but will not iterate new
    // observers added to the array after the iterator was created.
    class EndLimitedIterator : private ForwardIterator {
      public:
        typedef typename nsTObserverArray<T>::ForwardIterator base_type;

        EndLimitedIterator(const nsTObserverArray<T>& aArray)
          : ForwardIterator(aArray),
            mEnd(aArray, aArray.Count()) {
        }

        /**
         * Returns the next element and steps one step.
         * Returns null if there are no more observers. Once null is returned
         * the iterator becomes invalid and GetNext must not be called any more.
         * @return The next observer.
         */
        T* GetNext() {
          return (*this < mEnd) ?
                 static_cast<T*>(FastElementAt(base_type::mPosition++)) :
                 nsnull;
        }

      private:
        ForwardIterator mEnd;
    };
};

// XXXbz I wish I didn't have to pass in the observer type, but I
// don't see a way to get it out of array_.
#define NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(array_, obstype_, func_, params_) \
  PR_BEGIN_MACRO                                                             \
    nsTObserverArray<obstype_>::ForwardIterator iter_(array_);               \
    nsCOMPtr<obstype_> obs_;                                                 \
    while ((obs_ = iter_.GetNext())) {                                       \
      obs_ -> func_ params_ ;                                                \
    }                                                                        \
  PR_END_MACRO

#endif // nsTObserverArray_h___
