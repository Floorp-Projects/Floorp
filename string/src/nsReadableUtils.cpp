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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Scott Collins <scc@mozilla.org>
 *
 * Contributor(s):
 */

#include "nsReadableUtils.h"

char*
ToNewCString( const nsAReadableString& aSourceString )
  {
  }

char*
ToNewCString( const nsAReadableCString& aSourceCString )
  {
  }

PRUnichar*
ToNewUnicode( const nsAReadableString& aSourceString )
  {
  }

PRUnichar*
ToNewUnicode( const nsAReadableCString& aSourceCString )
  {
  }

PRBool
IsASCII( const nsAReadableString& aSourceString )
  {
    const PRUnichar NOT_ASCII = ~0x007F;


    // Don't want to use |copy_string| for this task, since we can stop at the first non-ASCII character

    nsAReadableString::const_iterator iter = aSourceString.BeginReading();
    nsAReadableString::const_iterator done_reading = aSourceString.EndReading();

      // for each chunk of |aSourceString|...
    while ( iter != done_reading )
      {
        PRUint32 chunk_size = iter.size_forward();
        const PRUnichar* c = iter.get();
        const PRUnichar* chunk_end = c + chunk_size;

          // for each character in this chunk...
        while ( c < chunk_end )
          if ( *c++ & NOT_ASCII )
            return PR_FALSE;

        iter += chunk_size;
      }

    return PR_TRUE;
  }