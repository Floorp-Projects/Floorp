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

/* nsAStringGenerator.h --- generators produce strings into a callers buffer, avoiding temporaries */

#ifndef nsAStringGenerator_h___
#define nsAStringGenerator_h___

#ifndef nsStringDefines_h___
#include "nsStringDefines.h"
#endif

#include "nscore.h"
  // for |PRUnichar|

#include "prtypes.h"
  // for |PRBool|, |PRUint32|


class nsAString;
class nsACString;

class NS_COM nsAStringGenerator
  {
    public:
      virtual ~nsAStringGenerator() { }

      virtual PRUnichar* operator()( PRUnichar* aDestBuffer ) const = 0;
      virtual PRUint32 Length() const = 0;
      virtual PRUint32 MaxLength() const = 0;
      virtual PRBool IsDependentOn( const nsAString& ) const = 0;
  };

class NS_COM nsACStringGenerator
  {
    public:
      virtual ~nsACStringGenerator() { }

      virtual char* operator()( char* aDestBuffer ) const = 0;
      virtual PRUint32 Length() const = 0;
      virtual PRUint32 MaxLength() const = 0;
      virtual PRBool IsDependentOn( const nsACString& ) const = 0;
  };

#endif // !defined(nsAStringGenerator_h___)
