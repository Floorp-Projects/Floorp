/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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


  /**
   * helper function for down-casting a nsTSubstring to a nsTFixedString.
   */
inline const nsTFixedString_CharT*
AsFixedString( const nsTSubstring_CharT* s )
  {
    return NS_STATIC_CAST(const nsTFixedString_CharT*, s);
  }


  /**
   * this function is called to prepare mData for writing.  the given capacity
   * indicates the required minimum storage size for mData, in sizeof(char_type)
   * increments.  this function returns true if the operation succeeds.  it also
   * returns the old data and old flags members if mData is newly allocated.
   * the old data must be released by the caller.
   */
PRBool
nsTSubstring_CharT::MutatePrep( size_type capacity, char_type** oldData, PRUint32* oldFlags )
  {
    // initialize to no old data
    *oldData = nsnull;
    *oldFlags = 0;

    size_type curCapacity = Capacity();

    // |curCapacity == size_type(-1)| means that the buffer is immutable, so we
    // need to allocate a new buffer.  we cannot use the existing buffer even
    // though it might be large enough.

    if (curCapacity != size_type(-1))
      {
        if (capacity <= curCapacity)
          return PR_TRUE;

        if (curCapacity > 0)
          {
            // use doubling algorithm when forced to increase available capacity,
            // but always start out with exactly the requested amount.
            PRUint32 temp = curCapacity;
            while (temp < capacity)
              temp <<= 1;
            capacity = temp;
          }
      }

    //
    // several cases:
    //
    //  (1) we have a shared buffer (mFlags & F_SHARED)
    //  (2) we have an owned buffer (mFlags & F_OWNED)
    //  (3) we have a fixed buffer (mFlags & F_FIXED)
    //  (4) we have a readonly buffer
    //
    // requiring that we in some cases preserve the data before creating
    // a new buffer complicates things just a bit ;-)
    //

    size_type storageSize = (capacity + 1) * sizeof(char_type);

    // case #1
    if (mFlags & F_SHARED)
      {
        nsStringHeader* hdr = nsStringHeader::FromData(mData);
        if (!hdr->IsReadonly())
          {
            nsStringHeader *newHdr = nsStringHeader::Realloc(hdr, storageSize);
            if (newHdr)
              {
                hdr = newHdr;
                mData = (char_type*) hdr->Data();
                return PR_TRUE;
              }
            hdr->Release();
            // out of memory!!  put us in a consistent state at least.
            mData = NS_CONST_CAST(char_type*, char_traits::sEmptyBuffer);
            mLength = 0;
            SetDataFlags(F_TERMINATED);
            return PR_FALSE;
          }
      }

    char_type* newData;
    PRUint32 newDataFlags;

      // if we have a fixed buffer of sufficient size, then use it.  this helps
      // avoid heap allocations.
    if ((mFlags & F_CLASS_FIXED) && (capacity < AsFixedString(this)->mFixedCapacity))
      {
        newData = AsFixedString(this)->mFixedBuf;
        newDataFlags = F_TERMINATED | F_FIXED;
      }
    else
      {
        // if we reach here then, we must allocate a new buffer.  we cannot
        // make use of our F_OWNED or F_FIXED buffers because they are not
        // large enough.

        nsStringHeader* newHdr = nsStringHeader::Alloc(storageSize);
        if (!newHdr)
          return PR_FALSE; // we are still in a consistent state

        newData = (char_type*) newHdr->Data();
        newDataFlags = F_TERMINATED | F_SHARED;
      }

    // save old data and flags
    *oldData = mData;
    *oldFlags = mFlags;

    mData = newData;
    SetDataFlags(newDataFlags);

    // mLength does not change

    // though we are not necessarily terminated at the moment, now is probably
    // still the best time to set F_TERMINATED.

    return PR_TRUE;
  }

void
nsTSubstring_CharT::Finalize()
  {
    ::ReleaseData(mData, mFlags);
    // mData, mLength, and mFlags are purposefully left dangling
  }

void
nsTSubstring_CharT::ReplacePrep( index_type cutStart, size_type cutLen, size_type fragLen )
  {
    // bound cut length
    cutLen = NS_MIN(cutLen, mLength - cutStart);

    PRUint32 newLen = mLength - cutLen + fragLen;

    char_type* oldData;
    PRUint32 oldFlags;
    if (!MutatePrep(newLen, &oldData, &oldFlags))
      return; // XXX out-of-memory error occured!

    if (oldData)
      {
        // determine whether or not we need to copy part of the old string
        // over to the new string.

        if (cutStart > 0)
          {
            // copy prefix from old string
            char_traits::copy(mData, oldData, cutStart);
          }

        if (cutStart + cutLen < mLength)
          {
            // copy suffix from old string to new offset
            size_type from = cutStart + cutLen;
            size_type fromLen = mLength - from;
            PRUint32 to = cutStart + fragLen;
            char_traits::copy(mData + to, oldData + from, fromLen);
          }

        ::ReleaseData(oldData, oldFlags);
      }
    else
      {
        // original data remains intact

        // determine whether or not we need to move part of the existing string
        // to make room for the requested hole.
        if (fragLen != cutLen && cutStart + cutLen < mLength)
          {
            PRUint32 from = cutStart + cutLen;
            PRUint32 fromLen = mLength - from;
            PRUint32 to = cutStart + fragLen;
            char_traits::move(mData + to, mData + from, fromLen);
          }
      }

    // add null terminator (mutable mData always has room for the null-
    // terminator).
    mData[newLen] = char_type(0);
    mLength = newLen;
  }

nsTSubstring_CharT::size_type
nsTSubstring_CharT::Capacity() const
  {
    // return size_type(-1) to indicate an immutable buffer

    size_type capacity;
    if (mFlags & F_SHARED)
      {
        // if the string is readonly, then we pretend that it has no capacity.
        nsStringHeader* hdr = nsStringHeader::FromData(mData);
        if (hdr->IsReadonly())
          capacity = size_type(-1);
        else
          capacity = (hdr->StorageSize() / sizeof(char_type)) - 1;
      }
    else if (mFlags & F_FIXED)
      {
        capacity = AsFixedString(this)->mFixedCapacity;
      }
    else if (mFlags & F_OWNED)
      {
        // we don't store the capacity of an adopted buffer because that would
        // require an additional member field.  the best we can do is base the
        // capacity on our length.  remains to be seen if this is the right
        // trade-off.
        capacity = mLength;
      }
    else
      {
        capacity = size_type(-1);
      }

    return capacity;
  }

void
nsTSubstring_CharT::EnsureMutable()
  {
    if (mFlags & (F_FIXED | F_OWNED))
      return;
    if ((mFlags & F_SHARED) && !nsStringHeader::FromData(mData)->IsReadonly())
      return;

    // promote to a shared string buffer
    Assign(string_type(mData, mLength));
  }

// ---------------------------------------------------------------------------

void
nsTSubstring_CharT::Assign( const char_type* data, size_type length )
  {
      // unfortunately, some callers pass null :-(
    if (!data)
      {
        Truncate();
        return;
      }

    if (length == size_type(-1))
      length = char_traits::length(data);

    if (IsDependentOn(data, data + length))
      {
        // take advantage of sharing here...
        Assign(string_type(data, length));
        return;
      }

    ReplacePrep(0, mLength, length);
    char_traits::copy(mData, data, length);
  }

void
nsTSubstring_CharT::AssignASCII( const char* data, size_type length )
  {
    // A Unicode string can't depend on an ASCII string buffer,
    // so this dependence check only applies to CStrings.
#ifdef CharT_is_char
    if (IsDependentOn(data, data + length))
      {
        // take advantage of sharing here...
        Assign(string_type(data, length));
        return;
      }
#endif

    ReplacePrep(0, mLength, length);
    char_traits::copyASCII(mData, data, length);
  }

void
nsTSubstring_CharT::AssignASCII( const char* data )
  {
    AssignASCII(data, strlen(data));
  }

void
nsTSubstring_CharT::Assign( const self_type& str )
  {
    // |str| could be sharable.  we need to check its flags to know how to
    // deal with it.

    if (&str == this)
      return;

    if (str.mFlags & F_SHARED)
      {
        // nice! we can avoid a string copy :-)

        // |str| should be null-terminated
        NS_ASSERTION(str.mFlags & F_TERMINATED, "shared, but not terminated");

        ::ReleaseData(mData, mFlags);

        mData = str.mData;
        mLength = str.mLength;
        SetDataFlags(F_TERMINATED | F_SHARED);

        // get an owning reference to the mData
        nsStringHeader::FromData(mData)->AddRef();
      }
    else if (str.mFlags & F_VOIDED)
      {
        // inherit the F_VOIDED attribute
        SetIsVoid(PR_TRUE);
      }
    else
      {
        // else, treat this like an ordinary assignment.
        Assign(str.Data(), str.Length());
      }
  }

void
nsTSubstring_CharT::Assign( const substring_tuple_type& tuple )
  {
    if (tuple.IsDependentOn(mData, mData + mLength))
      {
        // take advantage of sharing here...
        Assign(string_type(tuple));
        return;
      }

    size_type length = tuple.Length();

    ReplacePrep(0, mLength, length);
    if (length)
      tuple.WriteTo(mData, length);
  }

  // this is non-inline to reduce codesize at the callsite
void
nsTSubstring_CharT::Assign( const abstract_string_type& readable )
  {
      // promote to string if possible to take advantage of sharing
    if (readable.mVTable == nsTObsoleteAString_CharT::sCanonicalVTable)
      Assign(*readable.AsSubstring());
    else
      Assign(readable.ToSubstring());
  }


void
nsTSubstring_CharT::Adopt( char_type* data, size_type length )
  {
    if (data)
      {
        ::ReleaseData(mData, mFlags);

        if (length == size_type(-1))
          length = char_traits::length(data);

        mData = data;
        mLength = length;
        SetDataFlags(F_TERMINATED | F_OWNED);

        STRING_STAT_INCREMENT(Adopt);
      }
    else
      {
        SetIsVoid(PR_TRUE);
      }
  }


void
nsTSubstring_CharT::Replace( index_type cutStart, size_type cutLength, const char_type* data, size_type length )
  {
      // unfortunately, some callers pass null :-(
    if (!data)
      {
        length = 0;
      }
    else
      {
        if (length == size_type(-1))
          length = char_traits::length(data);

        if (IsDependentOn(data, data + length))
          {
            nsTAutoString_CharT temp(data, length);
            Replace(cutStart, cutLength, temp);
            return;
          }
      }

    cutStart = PR_MIN(cutStart, Length());

    ReplacePrep(cutStart, cutLength, length);

    if (length > 0)
      char_traits::copy(mData + cutStart, data, length);
  }

void
nsTSubstring_CharT::ReplaceASCII( index_type cutStart, size_type cutLength, const char* data, size_type length )
  {
    if (length == size_type(-1))
      length = strlen(data);
    
    // A Unicode string can't depend on an ASCII string buffer,
    // so this dependence check only applies to CStrings.
#ifdef CharT_is_char
    if (IsDependentOn(data, data + length))
      {
        nsTAutoString_CharT temp(data, length);
        Replace(cutStart, cutLength, temp);
        return;
      }
#endif

    cutStart = PR_MIN(cutStart, Length());

    ReplacePrep(cutStart, cutLength, length);

    if (length > 0)
      char_traits::copyASCII(mData + cutStart, data, length);
  }

void
nsTSubstring_CharT::Replace( index_type cutStart, size_type cutLength, const substring_tuple_type& tuple )
  {
    if (tuple.IsDependentOn(mData, mData + mLength))
      {
        nsTAutoString_CharT temp(tuple);
        Replace(cutStart, cutLength, temp);
        return;
      }

    size_type length = tuple.Length();

    cutStart = PR_MIN(cutStart, Length());

    ReplacePrep(cutStart, cutLength, length);

    if (length > 0)
      tuple.WriteTo(mData + cutStart, length);
  }

void
nsTSubstring_CharT::Replace( index_type cutStart, size_type cutLength, const abstract_string_type& readable )
  {
    Replace(cutStart, cutLength, readable.ToSubstring());
  }

void
nsTSubstring_CharT::SetCapacity( size_type capacity )
  {
    // capacity does not include room for the terminating null char

    // if our capacity is reduced to zero, then free our buffer.
    if (capacity == 0)
      {
        ::ReleaseData(mData, mFlags);
        mData = NS_CONST_CAST(char_type*, char_traits::sEmptyBuffer);
        mLength = 0;
        SetDataFlags(F_TERMINATED);
      }
    else
      {
        char_type* oldData;
        PRUint32 oldFlags;
        if (!MutatePrep(capacity, &oldData, &oldFlags))
          return; // XXX out-of-memory error occured!

        // compute new string length
        size_type newLen = NS_MIN(mLength, capacity);

        if (oldData)
          {
            // preserve old data
            if (mLength > 0)
              char_traits::copy(mData, oldData, newLen);

            ::ReleaseData(oldData, oldFlags);
          }

        // adjust mLength if our buffer shrunk down in size
        if (newLen < mLength)
          mLength = newLen;

        // always null-terminate here, even if the buffer got longer.  this is
        // for backwards compat with the old string implementation.
        mData[capacity] = char_type(0);
      }
  }

void
nsTSubstring_CharT::SetLength( size_type length )
  {
    SetCapacity(length);
    mLength = length;
  }

void
nsTSubstring_CharT::SetIsVoid( PRBool val )
  {
    if (val)
      {
        Truncate();
        mFlags |= F_VOIDED;
      }
    else
      {
        mFlags &= ~F_VOIDED;
      }
  }

PRBool
nsTSubstring_CharT::Equals( const self_type& str ) const
  {
    return mLength == str.mLength && char_traits::compare(mData, str.mData, mLength) == 0;
  }

PRBool
nsTSubstring_CharT::Equals( const self_type& str, const comparator_type& comp ) const
  {
    return mLength == str.mLength && comp(mData, str.mData, mLength) == 0;
  }

PRBool
nsTSubstring_CharT::Equals( const abstract_string_type& readable ) const
  {
    const char_type* data;
    size_type length = readable.GetReadableBuffer(&data);

    return mLength == length && char_traits::compare(mData, data, mLength) == 0;
  }

PRBool
nsTSubstring_CharT::Equals( const abstract_string_type& readable, const comparator_type& comp ) const
  {
    const char_type* data;
    size_type length = readable.GetReadableBuffer(&data);

    return mLength == length && comp(mData, data, mLength) == 0;
  }

PRBool
nsTSubstring_CharT::Equals( const char_type* data ) const
  {
    // unfortunately, some callers pass null :-(
    if (!data)
      {
        NS_NOTREACHED("null data pointer");
        return mLength == 0;
      }

    // XXX avoid length calculation?
    size_type length = char_traits::length(data);
    return mLength == length && char_traits::compare(mData, data, mLength) == 0;
  }

PRBool
nsTSubstring_CharT::Equals( const char_type* data, const comparator_type& comp ) const
  {
    // unfortunately, some callers pass null :-(
    if (!data)
      {
        NS_NOTREACHED("null data pointer");
        return mLength == 0;
      }

    // XXX avoid length calculation?
    size_type length = char_traits::length(data);
    return mLength == length && comp(mData, data, mLength) == 0;
  }

PRBool
nsTSubstring_CharT::EqualsASCII( const char* data, size_type len ) const
  {
    return mLength == len && char_traits::compareASCII(mData, data, len) == 0;
  }

PRBool
nsTSubstring_CharT::EqualsASCII( const char* data ) const
  {
    return char_traits::compareASCIINullTerminated(mData, mLength, data) == 0;
  }

PRBool
nsTSubstring_CharT::LowerCaseEqualsASCII( const char* data, size_type len ) const
  {
    return mLength == len && char_traits::compareLowerCaseToASCII(mData, data, len) == 0;
  }

PRBool
nsTSubstring_CharT::LowerCaseEqualsASCII( const char* data ) const
  {
    return char_traits::compareLowerCaseToASCIINullTerminated(mData, mLength, data) == 0;
  }

nsTSubstring_CharT::size_type
nsTSubstring_CharT::CountChar( char_type c ) const
  {
    const char_type *start = mData;
    const char_type *end   = mData + mLength;

    return NS_COUNT(start, end, c);
  }

PRInt32
nsTSubstring_CharT::FindChar( char_type c, index_type offset ) const
  {
    if (offset < mLength)
      {
        const char_type* result = char_traits::find(mData + offset, mLength - offset, c);
        if (result)
          return result - mData;
      }
    return -1;
  }
