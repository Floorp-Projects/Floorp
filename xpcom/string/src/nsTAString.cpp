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
   * Some comments on this implementation...
   *
   * This class is a bridge between the old string implementation and the new
   * string implementation.  If mVTable points to the canonical vtable for the
   * new string implementation, then we can cast directly to the new string
   * classes (helped by the AsSubstring() methods).  However, if mVTable is not
   * ours, then we need to call through the vtable to satisfy the nsTAString
   * methods.
   *
   * In most cases we will avoid the vtable.
   */


nsTAString_CharT::~nsTAString_CharT()
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Finalize();
    else
      AsObsoleteString()->~nsTObsoleteAString_CharT();
  }


nsTAString_CharT::size_type
nsTAString_CharT::Length() const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->Length();

    return AsObsoleteString()->Length();
  }

PRBool
nsTAString_CharT::Equals( const self_type& readable ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->Equals(readable);

    return ToSubstring().Equals(readable);
  }

PRBool
nsTAString_CharT::Equals( const self_type& readable, const comparator_type& comparator ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->Equals(readable, comparator);

    return ToSubstring().Equals(readable, comparator);
  }

PRBool
nsTAString_CharT::Equals( const char_type* data ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->Equals(data);

    return ToSubstring().Equals(data);
  }

PRBool
nsTAString_CharT::Equals( const char_type* data, const comparator_type& comparator ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->Equals(data, comparator);

    return ToSubstring().Equals(data, comparator);
  }

PRBool
nsTAString_CharT::EqualsASCII( const char* data, size_type len ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->EqualsASCII(data, len);

    return ToSubstring().EqualsASCII(data, len);
  }

PRBool
nsTAString_CharT::EqualsASCII( const char* data ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->EqualsASCII(data);

    return ToSubstring().EqualsASCII(data);
  }

PRBool
nsTAString_CharT::LowerCaseEqualsASCII( const char* data, size_type len ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->LowerCaseEqualsASCII(data, len);

    return ToSubstring().LowerCaseEqualsASCII(data, len);
  }

PRBool
nsTAString_CharT::LowerCaseEqualsASCII( const char* data ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->LowerCaseEqualsASCII(data);

    return ToSubstring().LowerCaseEqualsASCII(data);
  }

PRBool
nsTAString_CharT::IsVoid() const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->IsVoid();

    return AsObsoleteString()->IsVoid();
  }

void
nsTAString_CharT::SetIsVoid( PRBool val )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->SetIsVoid(val);
    else
      AsObsoleteString()->SetIsVoid(val);
  }

PRBool
nsTAString_CharT::IsTerminated() const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->IsTerminated();

    return AsObsoleteString()->GetFlatBufferHandle() != nsnull;
  }

CharT
nsTAString_CharT::First() const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->First();

    return ToSubstring().First();
  }

CharT
nsTAString_CharT::Last() const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->Last();

    return ToSubstring().Last();
  }

nsTAString_CharT::size_type
nsTAString_CharT::CountChar( char_type c ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->CountChar(c);

    return ToSubstring().CountChar(c);
  }

PRInt32
nsTAString_CharT::FindChar( char_type c, index_type offset ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->FindChar(c, offset);

    return ToSubstring().FindChar(c, offset);
  }

void
nsTAString_CharT::SetCapacity( size_type size )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->SetCapacity(size);
    else
      AsObsoleteString()->SetCapacity(size);
  }

void
nsTAString_CharT::SetLength( size_type size )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->SetLength(size);
    else
      AsObsoleteString()->SetLength(size);
  }

void
nsTAString_CharT::Assign( const self_type& readable )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Assign(readable);
    else
      AsObsoleteString()->do_AssignFromReadable(readable);
  }

void
nsTAString_CharT::Assign( const substring_tuple_type& tuple )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Assign(tuple);
    else
      AsObsoleteString()->do_AssignFromReadable(nsTAutoString_CharT(tuple));
  }

void
nsTAString_CharT::Assign( const char_type* data )
  {
    // null check and SetLength(0) cases needed for backwards compat

    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Assign(data);
    else if (data)
      AsObsoleteString()->do_AssignFromElementPtr(data);
    else
      AsObsoleteString()->SetLength(0);
  }

void
nsTAString_CharT::Assign( const char_type* data, size_type length )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Assign(data, length);
    else
      AsObsoleteString()->do_AssignFromElementPtrLength(data, length);
  }

void
nsTAString_CharT::AssignASCII( const char* data )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->AssignASCII(data);
    else
      {
#ifdef CharT_is_char
        AsObsoleteString()->do_AssignFromElementPtr(data);
#else
        nsTAutoString_CharT temp;
        temp.AssignASCII(data);
        AsObsoleteString()->do_AssignFromReadable(temp);
#endif
      }
  }

void
nsTAString_CharT::AssignASCII( const char* data, size_type length )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->AssignASCII(data, length);
    else
      {
#ifdef CharT_is_char
        AsObsoleteString()->do_AssignFromElementPtrLength(data, length);
#else
        nsTAutoString_CharT temp;
        temp.AssignASCII(data, length);
        AsObsoleteString()->do_AssignFromReadable(temp);
#endif
      }
  }

void
nsTAString_CharT::Assign( char_type c )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Assign(c);
    else
      AsObsoleteString()->do_AssignFromElement(c);
  }

void
nsTAString_CharT::Append( const self_type& readable )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Append(readable);
    else
      AsObsoleteString()->do_AppendFromReadable(readable);
  }

void
nsTAString_CharT::Append( const substring_tuple_type& tuple )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Append(tuple);
    else
      AsObsoleteString()->do_AppendFromReadable(nsTAutoString_CharT(tuple));
  }

void
nsTAString_CharT::Append( const char_type* data )
  {
    // null check case needed for backwards compat

    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Append(data);
    else if (data)
      AsObsoleteString()->do_AppendFromElementPtr(data);
  }

void
nsTAString_CharT::Append( const char_type* data, size_type length )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Append(data, length);
    else
      AsObsoleteString()->do_AppendFromElementPtrLength(data, length);
  }

void
nsTAString_CharT::AppendASCII( const char* data )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->AppendASCII(data);
    else
      {
#ifdef CharT_is_char
        AsObsoleteString()->do_AppendFromElementPtr(data);
#else
        nsTAutoString_CharT temp;
        temp.AssignASCII(data);
        AsObsoleteString()->do_AppendFromReadable(temp);
#endif
      }
  }

void
nsTAString_CharT::AppendASCII( const char* data, size_type length )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->AppendASCII(data, length);
    else
      {
#ifdef CharT_is_char
        AsObsoleteString()->do_AppendFromElementPtrLength(data, length);
#else
        nsTAutoString_CharT temp;
        temp.AssignASCII(data, length);
        AsObsoleteString()->do_AppendFromReadable(temp);
#endif
      }
  }

void
nsTAString_CharT::Append( char_type c )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Append(c);
    else
      AsObsoleteString()->do_AppendFromElement(c);
  }

void
nsTAString_CharT::Insert( const self_type& readable, index_type pos )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Insert(readable, pos);
    else
      AsObsoleteString()->do_InsertFromReadable(readable, pos);
  }

void
nsTAString_CharT::Insert( const substring_tuple_type& tuple, index_type pos )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Insert(tuple, pos);
    else
      AsObsoleteString()->do_InsertFromReadable(nsTAutoString_CharT(tuple), pos);
  }

void
nsTAString_CharT::Insert( const char_type* data, index_type pos )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Insert(data, pos);
    else
      AsObsoleteString()->do_InsertFromElementPtr(data, pos);
  }

void
nsTAString_CharT::Insert( const char_type* data, index_type pos, size_type length )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Insert(data, pos, length);
    else
      AsObsoleteString()->do_InsertFromElementPtrLength(data, pos, length);
  }

void
nsTAString_CharT::Insert( char_type c, index_type pos )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Insert(c, pos);
    else
      AsObsoleteString()->do_InsertFromElement(c, pos);
  }

void
nsTAString_CharT::Cut( index_type cutStart, size_type cutLength )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Cut(cutStart, cutLength);
    else
      AsObsoleteString()->Cut(cutStart, cutLength);
  }

void
nsTAString_CharT::Replace( index_type cutStart, size_type cutLength, const self_type& readable )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Replace(cutStart, cutLength, readable);
    else
      AsObsoleteString()->do_ReplaceFromReadable(cutStart, cutLength, readable);
  }

void
nsTAString_CharT::Replace( index_type cutStart, size_type cutLength, const substring_tuple_type& tuple )
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      AsSubstring()->Replace(cutStart, cutLength, tuple);
    else
      AsObsoleteString()->do_ReplaceFromReadable(cutStart, cutLength, nsTAutoString_CharT(tuple));
  }

nsTAString_CharT::size_type
nsTAString_CharT::GetReadableBuffer( const char_type **data ) const
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      {
        const substring_type* str = AsSubstring();
        *data = str->mData;
        return str->mLength;
      }

    obsolete_string_type::const_fragment_type frag;
    AsObsoleteString()->GetReadableFragment(frag, obsolete_string_type::kFirstFragment, 0);
    *data = frag.mStart;
    return (frag.mEnd - frag.mStart);
  }

nsTAString_CharT::size_type
nsTAString_CharT::GetWritableBuffer(char_type **data)
  {
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      {
        substring_type* str = AsSubstring();
        str->BeginWriting(*data);
        return str->Length();
      }

    obsolete_string_type::fragment_type frag;
    AsObsoleteString()->GetWritableFragment(frag, obsolete_string_type::kFirstFragment, 0);
    *data = frag.mStart;
    return (frag.mEnd - frag.mStart);
  }

PRBool
nsTAString_CharT::IsDependentOn(const char_type* start, const char_type *end) const
  {
      // this is an optimization...
    if (mVTable == obsolete_string_type::sCanonicalVTable)
      return AsSubstring()->IsDependentOn(start, end);

    return ToSubstring().IsDependentOn(start, end);
  }

const nsTAString_CharT::substring_type
nsTAString_CharT::ToSubstring() const
  {
    const char_type* data;
    size_type length = GetReadableBuffer(&data);
    return substring_type(NS_CONST_CAST(char_type*, data), length, 0);
  }
