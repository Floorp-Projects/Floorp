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

#ifdef XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE
nsTSubstring_CharT::nsTSubstring_CharT( char_type *data, size_type length,
                                        PRUint32 flags)
  : mData(data),
    mLength(length),
    mFlags(flags)
  {
    if (flags & F_OWNED) {
      STRING_STAT_INCREMENT(Adopt);
#ifdef NS_BUILD_REFCNT_LOGGING
      NS_LogCtor(mData, "StringAdopt", 1);
#endif
    }
  }
#endif /* XPCOM_STRING_CONSTRUCTOR_OUT_OF_LINE */

  /**
   * helper function for down-casting a nsTSubstring to a nsTFixedString.
   */
inline const nsTFixedString_CharT*
AsFixedString( const nsTSubstring_CharT* s )
  {
    return static_cast<const nsTFixedString_CharT*>(s);
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

    // If |capacity > size_type(-1)/2|, then our doubling algorithm may not be
    // able to allocate it.  Just bail out in cases like that.  We don't want
    // to be allocating 2GB+ strings anyway.
    if (capacity > size_type(-1)/2) {
      // Also assert for |capacity| equal to |size_type(-1)|, since we used to
      // use that value to flag immutability.
      NS_ASSERTION(capacity != size_type(-1), "Bogus capacity");
      return PR_FALSE;
    }

    // |curCapacity == 0| means that the buffer is immutable or 0-sized, so we
    // need to allocate a new buffer. We cannot use the existing buffer even
    // though it might be large enough.

    if (curCapacity != 0)
      {
        if (capacity <= curCapacity) {
          mFlags &= ~F_VOIDED;  // mutation clears voided flag
          return PR_TRUE;
        }

        if (curCapacity > 0)
          {
            // use doubling algorithm when forced to increase available
            // capacity.
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
        nsStringBuffer* hdr = nsStringBuffer::FromData(mData);
        if (!hdr->IsReadonly())
          {
            nsStringBuffer *newHdr = nsStringBuffer::Realloc(hdr, storageSize);
            if (!newHdr)
              return PR_FALSE; // out-of-memory (original header left intact)

            hdr = newHdr;
            mData = (char_type*) hdr->Data();
            mFlags &= ~F_VOIDED;  // mutation clears voided flag
            return PR_TRUE;
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

        nsStringBuffer* newHdr = nsStringBuffer::Alloc(storageSize);
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

PRBool
nsTSubstring_CharT::ReplacePrep( index_type cutStart, size_type cutLen, size_type fragLen )
  {
    // bound cut length
    cutLen = NS_MIN(cutLen, mLength - cutStart);

    PRUint32 newLen = mLength - cutLen + fragLen;

    char_type* oldData;
    PRUint32 oldFlags;
    if (!MutatePrep(newLen, &oldData, &oldFlags))
      return PR_FALSE; // out-of-memory

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

    return PR_TRUE;
  }

nsTSubstring_CharT::size_type
nsTSubstring_CharT::Capacity() const
  {
    // return 0 to indicate an immutable or 0-sized buffer

    size_type capacity;
    if (mFlags & F_SHARED)
      {
        // if the string is readonly, then we pretend that it has no capacity.
        nsStringBuffer* hdr = nsStringBuffer::FromData(mData);
        if (hdr->IsReadonly())
          capacity = 0;
        else {
          capacity = (hdr->StorageSize() / sizeof(char_type)) - 1;
        }
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
        capacity = 0;
      }

    return capacity;
  }

PRBool
nsTSubstring_CharT::EnsureMutable( size_type newLen )
  {
    if (newLen == size_type(-1) || newLen == mLength)
      {
        if (mFlags & (F_FIXED | F_OWNED))
          return PR_TRUE;
        if ((mFlags & F_SHARED) && !nsStringBuffer::FromData(mData)->IsReadonly())
          return PR_TRUE;

        // promote to a shared string buffer
        char_type* prevData = mData;
        Assign(mData, mLength);
        return mData != prevData;
      }
    else
      {
        SetLength(newLen);
        return mLength == newLen;
      }
  }

// ---------------------------------------------------------------------------

  // This version of Assign is optimized for single-character assignment.
void
nsTSubstring_CharT::Assign( char_type c )
  {
    if (ReplacePrep(0, mLength, 1))
      *mData = c;
  }


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

    if (ReplacePrep(0, mLength, length))
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

    if (ReplacePrep(0, mLength, length))
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

    if (!str.mLength)
      {
        Truncate();
        mFlags |= str.mFlags & F_VOIDED;
      }
    else if (str.mFlags & F_SHARED)
      {
        // nice! we can avoid a string copy :-)

        // |str| should be null-terminated
        NS_ASSERTION(str.mFlags & F_TERMINATED, "shared, but not terminated");

        ::ReleaseData(mData, mFlags);

        mData = str.mData;
        mLength = str.mLength;
        SetDataFlags(F_TERMINATED | F_SHARED);

        // get an owning reference to the mData
        nsStringBuffer::FromData(mData)->AddRef();
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

    // don't use ReplacePrep here because it changes the length
    char_type* oldData;
    PRUint32 oldFlags;
    if (MutatePrep(length, &oldData, &oldFlags)) {
      if (oldData)
        ::ReleaseData(oldData, oldFlags);

      tuple.WriteTo(mData, length);
      mData[length] = 0;
      mLength = length;
    }
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
#ifdef NS_BUILD_REFCNT_LOGGING
        // Treat this as construction of a "StringAdopt" object for leak
        // tracking purposes.        
        NS_LogCtor(mData, "StringAdopt", 1);
#endif // NS_BUILD_REFCNT_LOGGING
      }
    else
      {
        SetIsVoid(PR_TRUE);
      }
  }


  // This version of Replace is optimized for single-character replacement.
void
nsTSubstring_CharT::Replace( index_type cutStart, size_type cutLength, char_type c )
  {
    cutStart = PR_MIN(cutStart, Length());

    if (ReplacePrep(cutStart, cutLength, 1))
      mData[cutStart] = c;
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

    if (ReplacePrep(cutStart, cutLength, length) && length > 0)
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

    if (ReplacePrep(cutStart, cutLength, length) && length > 0)
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

    if (ReplacePrep(cutStart, cutLength, length) && length > 0)
      tuple.WriteTo(mData + cutStart, length);
  }

PRBool
nsTSubstring_CharT::SetCapacity( size_type capacity )
  {
    // capacity does not include room for the terminating null char

    // if our capacity is reduced to zero, then free our buffer.
    if (capacity == 0)
      {
        ::ReleaseData(mData, mFlags);
        mData = char_traits::sEmptyBuffer;
        mLength = 0;
        SetDataFlags(F_TERMINATED);
      }
    else
      {
        char_type* oldData;
        PRUint32 oldFlags;
        if (!MutatePrep(capacity, &oldData, &oldFlags))
          return PR_FALSE; // out-of-memory

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

    return PR_TRUE;
  }

void
nsTSubstring_CharT::SetLength( size_type length )
  {
    if (SetCapacity(length))
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
    return mLength == str.mLength && comp(mData, str.mData, mLength, str.mLength) == 0;
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
    return mLength == length && comp(mData, data, mLength, length) == 0;
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

void
nsTSubstring_CharT::StripChar( char_type aChar, PRInt32 aOffset )
  {
    if (mLength == 0 || aOffset >= PRInt32(mLength))
      return;

    EnsureMutable(); // XXX do this lazily?

    // XXX(darin): this code should defer writing until necessary.

    char_type* to   = mData + aOffset;
    char_type* from = mData + aOffset;
    char_type* end  = mData + mLength;

    while (from < end)
      {
        char_type theChar = *from++;
        if (aChar != theChar)
          *to++ = theChar;
      }
    *to = char_type(0); // add the null
    mLength = to - mData;
  }

void
nsTSubstring_CharT::StripChars( const char_type* aChars, PRUint32 aOffset )
  {
    if (aOffset >= PRUint32(mLength))
      return;

    EnsureMutable(); // XXX do this lazily?

    // XXX(darin): this code should defer writing until necessary.

    char_type* to   = mData + aOffset;
    char_type* from = mData + aOffset;
    char_type* end  = mData + mLength;

    while (from < end)
      {
        char_type theChar = *from++;
        const char_type* test = aChars;

        for (; *test && *test != theChar; ++test);

        if (!*test) {
          // Not stripped, copy this char.
          *to++ = theChar;
        }
      }
    *to = char_type(0); // add the null
    mLength = to - mData;
  }

void nsTSubstring_CharT::AppendPrintf( const char* format, ...)
  {
    char buf[32];
    va_list ap;
    va_start(ap, format);
    PRUint32 len = PR_vsnprintf(buf, sizeof(buf), format, ap);
    AppendASCII(buf, len);
    va_end(ap);
  }
