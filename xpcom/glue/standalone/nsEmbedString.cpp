/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is a small implementation of the nsAString and nsACString.
 *
 * The Initial Developer of the Original Code is
 *   Peter Annema <jaggernaut@netscape.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsEmbedString.h"
#include "nsMemory.h"

const PRUnichar gCommonEmptyBuffer[1] = { 0 };

nsEmbedString::nsEmbedString()
{
  Init();
}

nsEmbedString::nsEmbedString(const char_type* aString)
{
  Init();
  Assign(aString);
}

nsEmbedString::nsEmbedString(const char_type* aString, size_type aLength)
{
  Init();
  Assign(aString, aLength);
}

nsEmbedString::nsEmbedString(const nsEmbedString& aString)
{
  Init();
  Assign(aString);
}

nsEmbedString::nsEmbedString(const abstract_string_type& aReadable)
{
  Init();
  Assign(aReadable);
}

nsEmbedString::~nsEmbedString()
{
  Destroy();
}

void
nsEmbedString::Init()
{
  mStr = (char_type*)gCommonEmptyBuffer;
  mLength = 0;
  mCapacity = 0;
}

void
nsEmbedString::Destroy()
{
  Free();
}

void
nsEmbedString::Free()
{
  if (OwnsBuffer())
    nsMemory::Free(mStr);
}

PRBool
nsEmbedString::Realloc(size_type aNewSize)
{
  PRBool result = PR_TRUE;
  if (OwnsBuffer())
  {
    char_type* temp = (char_type*)nsMemory::Realloc(mStr, (aNewSize + 1) * sizeof(char_type));
    if (temp)
    {
      mStr = temp;
      mCapacity = aNewSize;
    }
    else
      result = PR_FALSE;
  }
  else
  {
    char_type* temp = (char_type*)nsMemory::Alloc((aNewSize + 1) * sizeof(char_type));
    if (temp)
    {
      memcpy(temp, mStr, mLength * sizeof(char_type));
      mStr = temp;
      mCapacity = aNewSize;
    }
    else
      result = PR_FALSE;
  }

  return result;
}

PRBool
nsEmbedString::OwnsBuffer() const
{
  return mStr != (char_type*)gCommonEmptyBuffer;
}

const nsEmbedString::char_type*
nsEmbedString::GetReadableFragment(const_fragment_type& aFragment, nsFragmentRequest aRequest, index_type aOffset) const
{
  switch (aRequest) {
    case kFirstFragment:
    case kLastFragment:
    case kFragmentAt:
    aFragment.mEnd = (aFragment.mStart = mStr) + mLength;
    return aFragment.mStart + aOffset;

    case kPrevFragment:
    case kNextFragment:
    default:
    return 0;
  }
}

nsEmbedString::char_type*
nsEmbedString::GetWritableFragment(fragment_type& aFragment, nsFragmentRequest aRequest, index_type aOffset)
{
  switch (aRequest) {
    case kFirstFragment:
    case kLastFragment:
    case kFragmentAt:
    aFragment.mEnd = (aFragment.mStart = mStr) + mLength;
    return aFragment.mStart + aOffset;

    case kPrevFragment:
    case kNextFragment:
    default:
    return 0;
  }
}

const nsEmbedString::buffer_handle_type*
nsEmbedString::GetFlatBufferHandle() const
{
  return NS_REINTERPRET_CAST(const buffer_handle_type*, 1);
}

void
nsEmbedString::SetLength(size_type aLength)
{
  if (aLength > mCapacity)
    GrowCapacity(aLength);

  mLength = aLength;
  if (mStr != (char_type*)gCommonEmptyBuffer)
    AddNullTerminator();
}

void
nsEmbedString::SetCapacity(size_type aNewCapacity)
{
  if (aNewCapacity)
  {
    if (aNewCapacity > mCapacity)
      GrowCapacity(aNewCapacity);

    // AddNullTerminator(); // doesn't make sense
  }
  else
  {
    Destroy();
    Init();
  }
}

PRBool
nsEmbedString::EnsureCapacity(size_type aNewCapacity)
{
  PRBool result = PR_TRUE;

  if (aNewCapacity > mCapacity)
  {
    result = Realloc(aNewCapacity);
    if (result)
      AddNullTerminator();
  }

  return result;
}

PRBool
nsEmbedString::GrowCapacity(size_type aNewCapacity)
{
  PRBool result = PR_TRUE;

  if (mCapacity)
  {
    size_type newCapacity = mCapacity;
    while (newCapacity < aNewCapacity)
      newCapacity <<= 1;
    aNewCapacity = newCapacity;
  }

  nsEmbedString temp;
  result = temp.EnsureCapacity(aNewCapacity);

  if (result)
  {
    if (mLength)
      temp.Assign(*this);

    Free();
    mStr = temp.mStr;
    temp.mStr = 0;
    mLength = temp.mLength;
    mCapacity = temp.mCapacity;
  }

  return result;
}

nsEmbedCString::nsEmbedCString()
{
  Init();
}

nsEmbedCString::nsEmbedCString(const char_type* aString)
{
  Init();
  Assign(aString);
}

nsEmbedCString::nsEmbedCString(const char_type* aString, size_type aLength)
{
  Init();
  Assign(aString, aLength);
}

nsEmbedCString::nsEmbedCString(const nsEmbedCString& aString)
{
  Init();
  Assign(aString);
}

nsEmbedCString::nsEmbedCString(const abstract_string_type& aReadable)
{
  Init();
  Assign(aReadable);
}

nsEmbedCString::~nsEmbedCString()
{
  Destroy();
}

void
nsEmbedCString::Init()
{
  mStr = (char_type*)gCommonEmptyBuffer;
  mLength = 0;
  mCapacity = 0;
}

void
nsEmbedCString::Destroy()
{
  Free();
}

void
nsEmbedCString::Free()
{
  if (OwnsBuffer())
    nsMemory::Free(mStr);
}

PRBool
nsEmbedCString::Realloc(size_type aNewSize)
{
  PRBool result = PR_TRUE;
  if (OwnsBuffer())
  {
    char_type* temp = (char_type*)nsMemory::Realloc(mStr, (aNewSize + 1) * sizeof(char_type));
    if (temp)
    {
      mStr = temp;
      mCapacity = aNewSize;
    }
    else
      result = PR_FALSE;
  }
  else
  {
    char_type* temp = (char_type*)nsMemory::Alloc((aNewSize + 1) * sizeof(char_type));
    if (temp)
    {
      memcpy(temp, mStr, mLength * sizeof(char_type));
      mStr = temp;
      mCapacity = aNewSize;
    }
    else
      result = PR_FALSE;
  }

  return result;
}

PRBool
nsEmbedCString::OwnsBuffer() const
{
  return mStr != (char_type*)gCommonEmptyBuffer;
}

const nsEmbedCString::char_type*
nsEmbedCString::GetReadableFragment(const_fragment_type& aFragment, nsFragmentRequest aRequest, index_type aOffset) const
{
  switch (aRequest) {
    case kFirstFragment:
    case kLastFragment:
    case kFragmentAt:
    aFragment.mEnd = (aFragment.mStart = mStr) + mLength;
    return aFragment.mStart + aOffset;

    case kPrevFragment:
    case kNextFragment:
    default:
    return 0;
  }
}

nsEmbedCString::char_type*
nsEmbedCString::GetWritableFragment(fragment_type& aFragment, nsFragmentRequest aRequest, index_type aOffset)
{
  switch (aRequest) {
    case kFirstFragment:
    case kLastFragment:
    case kFragmentAt:
    aFragment.mEnd = (aFragment.mStart = mStr) + mLength;
    return aFragment.mStart + aOffset;

    case kPrevFragment:
    case kNextFragment:
    default:
    return 0;
  }
}

const nsEmbedCString::buffer_handle_type*
nsEmbedCString::GetFlatBufferHandle() const
{
  return NS_REINTERPRET_CAST(const buffer_handle_type*, 1);
}

void
nsEmbedCString::SetLength(size_type aLength)
{
  if (aLength > mCapacity)
    GrowCapacity(aLength);

  mLength = aLength;
  if (mStr != (char_type*)gCommonEmptyBuffer)
    AddNullTerminator();
}

void
nsEmbedCString::SetCapacity(size_type aNewCapacity)
{
  if (aNewCapacity)
  {
    if (aNewCapacity > mCapacity)
      GrowCapacity(aNewCapacity);

    // AddNullTerminator(); // doesn't make sense
  }
  else
  {
    Destroy();
    Init();
  }
}

PRBool
nsEmbedCString::EnsureCapacity(size_type aNewCapacity)
{
  PRBool result = PR_TRUE;

  if (aNewCapacity > mCapacity)
  {
    result = Realloc(aNewCapacity);
    if (result)
      AddNullTerminator();
  }

  return result;
}

PRBool
nsEmbedCString::GrowCapacity(size_type aNewCapacity)
{
  PRBool result = PR_TRUE;

  if (mCapacity)
  {
    size_type newCapacity = mCapacity;
    while (newCapacity < aNewCapacity)
      newCapacity <<= 1;
    aNewCapacity = newCapacity;
  }

  nsEmbedCString temp;
  result = temp.EnsureCapacity(aNewCapacity);

  if (result)
  {
    if (mLength)
      temp.Assign(*this);

    Free();
    mStr = temp.mStr;
    temp.mStr = 0;
    mLength = temp.mLength;
    mCapacity = temp.mCapacity;
  }

  return result;
}


