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

/* nsStringFwd.h --- forward declarations for string classes */

#ifndef nsStringFwd_h___
#define nsStringFwd_h___

#ifndef nscore_h___
#include "nscore.h"
#endif


  /**
   * double-byte (PRUnichar) string types
   */

class nsAString;
class nsObsoleteAString;
class nsSubstring;
class nsSubstringTuple;
class nsString;
class nsAutoString;
class nsDependentString;
class nsDependentSubstring;
class nsPromiseFlatString;
class nsStringComparator;
class nsDefaultStringComparator;
class nsXPIDLString;


  /**
   * single-byte (char) string types
   */

class nsACString;
class nsObsoleteACString;
class nsCSubstring;
class nsCSubstringTuple;
class nsCString;
class nsCAutoString;
class nsDependentCString;
class nsDependentCSubstring;
class nsPromiseFlatCString;
class nsCStringComparator;
class nsDefaultCStringComparator;
class nsXPIDLCString;


  /**
   * typedefs for backwards compatibility
   */

typedef nsString              nsAFlatString;
typedef nsSubstring           nsASingleFragmentString;

typedef nsCString             nsAFlatCString;
typedef nsCSubstring          nsASingleFragmentCString;

  
#endif /* !defined(nsStringFwd_h___) */
