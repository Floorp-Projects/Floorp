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
 * |nsPrintfCString| is syntactic sugar for a printf used as a
 * |const nsACString| with a small builtin autobuffer. In almost all cases
 * it is better to just use AppendPrintf. Example usage:
 *
 *   NS_WARNING(nsPrintfCString("Unexpected value: %f", 13.917).get());
 */
class nsPrintfCString : public nsFixedCString
  {
    typedef nsCString string_type;

    public:
      explicit nsPrintfCString( const char_type* format, ... )
        : nsFixedCString(mLocalBuffer, kLocalBufferSize, 0)
        {
          va_list ap;
          va_start(ap, format);
          AppendPrintf(format, ap);
          va_end(ap);
        }

    private:
      enum { kLocalBufferSize=15 };
      // ought to be large enough for most things ... a |long long| needs at most 20 (so you'd better ask)
      //  pinkerton suggests 7.  We should measure and decide what's appropriate

      char_type  mLocalBuffer[ kLocalBufferSize ];
  };

#endif // !defined(nsPrintfCString_h___)
