/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Annema <jaggernaut@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsUTF8Utils_h_
#define nsUTF8Utils_h_

class UTF8traits
  {
    public:
      static PRBool isASCII(char c) { return (c & 0x80) == 0x00; }
      static PRBool isInSeq(char c) { return (c & 0xC0) == 0x80; }
      static PRBool is2byte(char c) { return (c & 0xE0) == 0xC0; }
      static PRBool is3byte(char c) { return (c & 0xF0) == 0xE0; }
      static PRBool is4byte(char c) { return (c & 0xF8) == 0xF0; }
      static PRBool is5byte(char c) { return (c & 0xFC) == 0xF8; }
      static PRBool is6byte(char c) { return (c & 0xFE) == 0xFC; }
  };

#define PLANE1_BASE           0x00010000  
#define UCS2_REPLACEMENT_CHAR 0xfffd     

class ConvertUTF8toUCS2
  {
    public:
      typedef nsACString::char_type value_type;
      typedef nsAString::char_type  buffer_type;

    ConvertUTF8toUCS2( buffer_type* aBuffer )
        : mStart(aBuffer), mBuffer(aBuffer), mErrorEncountered(PR_FALSE) {}

    size_t Length() const { return mBuffer - mStart; }

    PRUint32 write( const value_type* start, PRUint32 N )
      {
        if ( mErrorEncountered )
          return N;

        // algorithm assumes utf8 units won't
        // be spread across fragments
        const value_type* p = start;
        const value_type* end = start + N;
        for ( ; p != end /* && *p */; )
          {
            char c = *p++;

            if ( UTF8traits::isASCII(c) )
              {
                *mBuffer++ = buffer_type(c);
                continue;
              }

            PRUint32 ucs4;
            PRUint32 minUcs4;
            PRInt32 state = 0;

            if ( UTF8traits::is2byte(c) )
              {
                ucs4 = (PRUint32(c) << 6) & 0x000007C0L;
                state = 1;
                minUcs4 = 0x00000080;
              }
            else if ( UTF8traits::is3byte(c) )
              {
                ucs4 = (PRUint32(c) << 12) & 0x0000F000L;
                state = 2;
                minUcs4 = 0x00000800;
              }
            else if ( UTF8traits::is4byte(c) )
              {
                ucs4 = (PRUint32(c) << 18) & 0x001F0000L;
                state = 3;
                minUcs4 = 0x00010000;
              }
            else if ( UTF8traits::is5byte(c) )
              {
                ucs4 = (PRUint32(c) << 24) & 0x03000000L;
                state = 4;
                minUcs4 = 0x00200000;
              }
            else if ( UTF8traits::is6byte(c) )
              {
                ucs4 = (PRUint32(c) << 30) & 0x40000000L;
                state = 5;
                minUcs4 = 0x04000000;
              }
            else
              {
                NS_ERROR("Not a UTF-8 string. This code should only be used for converting from known UTF-8 strings.");
                mErrorEncountered = PR_TRUE;
                return N;
              }

            while ( state-- )
              {
                c = *p++;

                if ( UTF8traits::isInSeq(c) )
                  {
                    PRInt32 shift = state * 6;
                    ucs4 |= (PRUint32(c) & 0x3F) << shift;
                  }
                else
                  {
                    NS_ERROR("not a UTF8 string");
                    mErrorEncountered = PR_TRUE;
                    return N;
                  }
              }

            if ( ucs4 < minUcs4 )
              {
                // Overlong sequence
                *mBuffer++ = UCS2_REPLACEMENT_CHAR;
              }
            else if ( ucs4 <= 0xD7FF )
              {
                *mBuffer++ = ucs4;
              }
            else if ( /* ucs4 >= 0xD800 && */ ucs4 <= 0xDFFF )
              {
                // Surrogates
                *mBuffer++ = UCS2_REPLACEMENT_CHAR;
              }
            else if ( ucs4 == 0xFFFE || ucs4 == 0xFFFF )
              {
                // Prohibited characters
                *mBuffer++ = UCS2_REPLACEMENT_CHAR;
              }
            else if ( ucs4 >= PLANE1_BASE )
              {
                if ( ucs4 >= 0x00110000 )
                  *mBuffer++ = UCS2_REPLACEMENT_CHAR;
                else {
                  // surrogate, see unicode specification 3.7 for following math.
                  ucs4 -= PLANE1_BASE;
                  *mBuffer++ = (PRUnichar)(ucs4 >> 10) | 0xd800u;
                  *mBuffer++ = (PRUnichar)(ucs4 & 0x3ff) | 0xdc00u;
                }
              }
            else
              {
                if ( ucs4 != 0xFEFF ) // ignore BOM
                    *mBuffer++ = ucs4;
              }
          }
        return p - start;
      }

    private:
      buffer_type* mStart;
      buffer_type* mBuffer;
      PRBool mErrorEncountered;
  };

class CalculateUTF8Length
  {
    public:
      typedef nsACString::char_type value_type;

    CalculateUTF8Length() : mLength(0), mErrorEncountered(PR_FALSE) { }

    size_t Length() const { return mLength; }

    PRUint32 write( const value_type* start, PRUint32 N )
      {
          // ignore any further requests
        if ( mErrorEncountered )
            return N;

        // algorithm assumes utf8 units won't
        // be spread across fragments
        const value_type* p = start;
        const value_type* end = start + N;
        for ( ; p < end /* && *p */; ++mLength )
          {
            if ( UTF8traits::isASCII(*p) )
                p += 1;
            else if ( UTF8traits::is2byte(*p) )
                p += 2;
            else if ( UTF8traits::is3byte(*p) )
                p += 3;
            else if ( UTF8traits::is4byte(*p) ) {
                p += 4;
                ++mLength;
            }
            else if ( UTF8traits::is5byte(*p) )
                p += 5;
            else if ( UTF8traits::is6byte(*p) )
                p += 6;
            else
              {
                break;
              }
          }
        if ( p != end )
          {
            NS_ERROR("Not a UTF-8 string. This code should only be used for converting from known UTF-8 strings.");
            mErrorEncountered = PR_TRUE;
            mLength = 0;
            return N;
          }
        return p - start;
      }

    private:
      size_t mLength;
      PRBool mErrorEncountered;
  };

#endif /* !defined(nsUTF8Utils_h_) */
