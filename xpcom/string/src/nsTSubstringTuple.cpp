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
   * computes the aggregate string length
   */

nsTSubstringTuple_CharT::size_type
nsTSubstringTuple_CharT::Length() const
  {
    PRUint32 len;
    if (mHead)
      len = mHead->Length();
    else
      len = TO_SUBSTRING(mFragA).Length();

    return len + TO_SUBSTRING(mFragB).Length();
  }


  /**
   * writes the aggregate string to the given buffer.  bufLen is assumed
   * to be equal to or greater than the value returned by the Length()
   * method.  the string written to |buf| is not null-terminated.
   */

void
nsTSubstringTuple_CharT::WriteTo( char_type *buf, PRUint32 bufLen ) const
  {
    const substring_type& b = TO_SUBSTRING(mFragB);

    NS_ASSERTION(bufLen >= b.Length(), "buffer too small");
    PRUint32 headLen = bufLen - b.Length();
    if (mHead)
      {
        mHead->WriteTo(buf, headLen);
      }
    else
      {
        const substring_type& a = TO_SUBSTRING(mFragA);

        NS_ASSERTION(a.Length() == headLen, "buffer incorrectly sized");
        char_traits::copy(buf, a.Data(), a.Length());
      }

    char_traits::copy(buf + headLen, b.Data(), b.Length());

#if 0
    // we need to write out data into |buf|, ending at |buf+bufLen|.  so our
    // data needs to precede |buf+bufLen| exactly.  we trust that the buffer
    // was properly sized!

    const substring_type& b = TO_SUBSTRING(mFragB);

    NS_ASSERTION(bufLen >= b.Length(), "buffer is too small");
    char_traits::copy(buf + bufLen - b.Length(), b.Data(), b.Length());

    bufLen -= b.Length();

    if (mHead)
      {
        mHead->WriteTo(buf, bufLen);
      }
    else
      {
        const substring_type& a = TO_SUBSTRING(mFragA);
        NS_ASSERTION(bufLen == a.Length(), "buffer is too small");
        char_traits::copy(buf, a.Data(), a.Length());
      }
#endif
  }


  /**
   * returns true if this tuple is dependent on (i.e., overlapping with)
   * the given char sequence.
   */

bool
nsTSubstringTuple_CharT::IsDependentOn( const char_type *start, const char_type *end ) const
  {
    // we start with the right-most fragment since it is faster to check.

    if (TO_SUBSTRING(mFragB).IsDependentOn(start, end))
      return PR_TRUE;

    if (mHead)
      return mHead->IsDependentOn(start, end);

    return TO_SUBSTRING(mFragA).IsDependentOn(start, end);
  }
