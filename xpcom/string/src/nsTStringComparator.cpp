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

NS_COM int NS_FASTCALL
Compare( const nsTSubstring_CharT::base_string_type& lhs, const nsTSubstring_CharT::base_string_type& rhs, const nsTStringComparator_CharT& comp )
  {
    typedef nsTSubstring_CharT::size_type size_type;

    if ( &lhs == &rhs )
      return 0;

    nsTSubstring_CharT::const_iterator leftIter, rightIter;
    lhs.BeginReading(leftIter);
    rhs.BeginReading(rightIter);

    size_type lLength = leftIter.size_forward();
    size_type rLength = rightIter.size_forward();
    size_type lengthToCompare = NS_MIN(lLength, rLength);

    int result;
    if ( (result = comp(leftIter.get(), rightIter.get(), lengthToCompare, lengthToCompare)) == 0 )
      {
        if ( lLength < rLength )
          result = -1;
        else if ( rLength < lLength )
          result = 1;
        else
          result = 0;
      }

    return result;
  }

int
nsTDefaultStringComparator_CharT::operator()( const char_type* lhs, const char_type* rhs, PRUint32 lLength, PRUint32 rLength) const
  {
    return (lLength == rLength) ? nsCharTraits<CharT>::compare(lhs, rhs, lLength) :
           (lLength > rLength) ? 1 : -1;
  }
