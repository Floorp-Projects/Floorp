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

#ifndef nsAString_h___
#define nsAString_h___

#ifndef nsStringFwd_h___
#include "nsStringFwd.h"
#endif

#ifndef nsStringIterator_h___
#include "nsStringIterator.h"
#endif

#ifndef nsObsoleteAString_h___
#include "nsObsoleteAString.h"
#endif

// If some platform(s) can't handle our template that matches literal strings,
// then we'll disable it on those platforms.
#ifndef NS_DISABLE_LITERAL_TEMPLATE
#  if (defined(_MSC_VER) && (_MSC_VER < 1310)) || (defined(__SUNPRO_CC) & (__SUNPRO_CC < 0x560))
#    define NS_DISABLE_LITERAL_TEMPLATE
#  endif
#endif /* !NS_DISABLE_LITERAL_TEMPLATE */

#include <string.h>

  // declare nsAString
#include "string-template-def-unichar.h"
#include "nsTAString.h"
#include "string-template-undef.h"


  // declare nsACString
#include "string-template-def-char.h"
#include "nsTAString.h"
#include "string-template-undef.h"


  /**
   * ASCII case-insensitive comparator.  (for Unicode case-insensitive
   * comparision, see nsUnicharUtils.h)
   */
class NS_COM nsCaseInsensitiveCStringComparator
    : public nsCStringComparator
  {
    public:
      typedef char char_type;

      virtual int operator()( const char_type*, const char_type*, PRUint32 length ) const;
      virtual int operator()( char_type, char_type ) const;
  };


  // included here for backwards compatibility
#ifndef nsSubstringTuple_h___
#include "nsSubstringTuple.h"
#endif

#endif // !defined(nsAString_h___)
