/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

/* nsAFlatString.h --- */

#ifndef nsAFlatString_h___
#define nsAFlatString_h___

#ifndef nsASingleFragmenttring_h___
#include "nsASingleFragmentString.h"
#endif

  /**
   * |nsAFlatC?String| is an abstract class.  Strings implementing
   * |nsAFlatC?String| have a buffer that is stored as a single fragment
   * and is NULL-terminated.  That buffer can be accessed for reading
   * using the |get| method.
   *
   * See also |nsASingleFragmentC?String| and |nsAC?String|, base
   * classes of |nsAFlatC?String|.
   */

class NS_COM nsAFlatString
    : public nsASingleFragmentString
  {
    public:
        // don't really want this to be virtual, and won't after |obsolete_nsString| is really dead
      virtual const char_type* get() const { const char_type* temp; return BeginReading(temp); }
  };

class NS_COM nsAFlatCString
    : public nsASingleFragmentCString
  {
    public:
        // don't really want this to be virtual, and won't after |obsolete_nsCString| is really dead
      virtual const char_type* get() const { const char_type* temp; return BeginReading(temp); }
  };

#endif /* !defined(nsAFlatString_h___) */
