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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsPrintfCString_h___
#define nsPrintfCString_h___
 
#ifndef nsString_h___
#include "nsString.h"
#endif


  /**
   * |nsPrintfCString| lets you use a formated |printf| string as an |const nsACString|.
   *
   *   myCStr += nsPrintfCString("%f", 13.917);
   *     // ...a general purpose substitute for |AppendFloat|
   *
   * For longer patterns, you'll want to use the constructor that takes a length
   *
   *   nsPrintfCString(128, "%f, %f, %f, %f, %f, %f, %f, %i, %f", x, y, z, 3.2, j, k, l, 3, 3.1);
   *
   * Exceding the default size (which you must specify in the constructor, it is not determined)
   * causes an allocation, so avoid that.  If your formatted string exceeds the allocated space, it is
   * cut off at the size of the buffer, no error is reported (and no out-of-bounds writing occurs).
   * This class is intended to be useful for numbers and short
   * strings, not arbitrary formatting of other strings (e.g., with %s).  There is currently no
   * wide version of this class, since wide |printf| is not generally available.  That means
   * to get a wide version of your formatted data, you must, e.g.,
   *
   *   CopyASCIItoUTF16(nsPrintfCString("%f", 13.917"), myStr);
   *
   * That's another good reason to avoid this class for anything but numbers ... as strings can be
   * much more efficiently handled with |NS_LITERAL_[C]STRING| and |nsLiteral[C]String|.
   */

class NS_COM nsPrintfCString : public nsCString
  {
    typedef nsCString string_type;

    enum { kLocalBufferSize=15 };
      // ought to be large enough for most things ... a |long long| needs at most 20 (so you'd better ask)
      //  pinkerton suggests 7.  We should measure and decide what's appropriate

    public:
      // XXX don't these need to be declared CDECL ??
      explicit nsPrintfCString( const char_type* format, ... );
      nsPrintfCString( size_type n, const char_type* format, ...);

    private:
      char_type  mLocalBuffer[ kLocalBufferSize + 1 ];
  };

#endif // !defined(nsPrintfCString_h___)
