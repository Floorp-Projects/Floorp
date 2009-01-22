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

#include "nsPrintfCString.h"
#include <stdarg.h>
#include "prprf.h"


// though these classes define a fixed buffer, they do not set the F_FIXED
// flag.  this is because they are not intended to be modified after they have
// been constructed.  we could change this but it would require adding a new
// class to the hierarchy, one that both this class and nsCAutoString would
// inherit from.  for now, we populate the fixed buffer, and then let the
// nsCString code treat the buffer as if it were a dependent buffer.

nsPrintfCString::nsPrintfCString( const char_type* format, ... )
  : string_type(mLocalBuffer, 0, F_TERMINATED)
  {
    va_list ap;

    size_type logical_capacity = kLocalBufferSize;
    size_type physical_capacity = logical_capacity + 1;

    va_start(ap, format);
    mLength = PR_vsnprintf(mData, physical_capacity, format, ap);
    va_end(ap);
  }

nsPrintfCString::nsPrintfCString( size_type n, const char_type* format, ... )
  : string_type(mLocalBuffer, 0, F_TERMINATED)
  {
    va_list ap;

      // make sure there's at least |n| space
    size_type logical_capacity = kLocalBufferSize;
    if ( n > logical_capacity )
      {
        SetCapacity(n);
        if (Capacity() < n)
          return; // out of memory !!
        logical_capacity = n;
      }
    size_type physical_capacity = logical_capacity + 1;

    va_start(ap, format);
    mLength = PR_vsnprintf(mData, physical_capacity, format, ap);
    va_end(ap);
  }
