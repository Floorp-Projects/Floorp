/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Scott Collins <scc@mozilla.org>
 *
 * Contributor(s):
 */

#ifndef nsStringIO_h___
#define nsStringIO_h___

#include "nsAReadableString.h"
#include <stdio.h>


template <class CharT>
class nsFileCharSink
  {
    public:
      typedef CharT value_type;

    public:
      nsFileCharSink( FILE* aOutputFile ) : mOutputFile(aOutputFile) { }

      PRUint32
      write( const value_type* s, PRUint32 n )
        {
          return fwrite(s, sizeof(CharT), n, mOutputFile);
        }

    private:
      FILE* mOutputFile;
  };


template <class CharT>
inline
void
fprint_string( FILE* aFile, const basic_nsAReadableString<CharT>& aString )
  {
    nsReadingIterator<CharT> fromBegin, fromEnd;
    nsFileCharSink<CharT> toBegin(aFile);
    copy_string(aString.BeginReading(fromBegin), aString.EndReading(fromEnd), toBegin);
  }


template <class CharT>
inline
void
print_string( const basic_nsAReadableString<CharT>& aString )
  {
    fprint_string(stdout, aString);
  }


#endif // !defined(nsStringIO_h___)
