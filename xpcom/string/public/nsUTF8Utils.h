/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsUTF8Utils_h_
#define nsUTF8Utils_h_

// This file may be used in two ways: if MOZILLA_INTERNAL_API is defined, this
// file will provide signatures for the Mozilla abstract string types. It will
// use XPCOM assertion/debugging macros, etc.

#include "nscore.h"
#include "mozilla/SSE.h"

#include "nsCharTraits.h"

class UTF8traits
  {
    public:
      static bool isASCII(char c) { return (c & 0x80) == 0x00; }
      static bool isInSeq(char c) { return (c & 0xC0) == 0x80; }
      static bool is2byte(char c) { return (c & 0xE0) == 0xC0; }
      static bool is3byte(char c) { return (c & 0xF0) == 0xE0; }
      static bool is4byte(char c) { return (c & 0xF8) == 0xF0; }
      static bool is5byte(char c) { return (c & 0xFC) == 0xF8; }
      static bool is6byte(char c) { return (c & 0xFE) == 0xFC; }
  };

/**
 * Extract the next UCS-4 character from the buffer and return it.  The
 * pointer passed in is advanced to the start of the next character in the
 * buffer.  If non-null, the parameters err and overlong are filled in to
 * indicate that the character was represented by an overlong sequence, or
 * that an error occurred.
 */

class UTF8CharEnumerator
{
public:
  static PRUint32 NextChar(const char **buffer, const char *end,
                           bool *err)
  {
    NS_ASSERTION(buffer && *buffer, "null buffer!");

    const char *p = *buffer;
    *err = false;

    if (p >= end)
      {
        *err = true;

        return 0;
      }

    char c = *p++;

    if ( UTF8traits::isASCII(c) )
      {
        *buffer = p;
        return c;
      }

    PRUint32 ucs4;
    PRUint32 minUcs4;
    PRInt32 state = 0;

    if (!CalcState(c, ucs4, minUcs4, state)) {
        NS_ERROR("Not a UTF-8 string. This code should only be used for converting from known UTF-8 strings.");
        *err = true;

        return 0;
    }

    while ( state-- )
      {
        if (p == end)
          {
            *err = true;

            return 0;
          }

        c = *p++;

        if (!AddByte(c, state, ucs4))
          {
            *err = true;

            return 0;
          }
      }

      if ( ucs4 < minUcs4 )
        {
          // Overlong sequence
          ucs4 = UCS2_REPLACEMENT_CHAR;
        }
      else if ( ucs4 >= 0xD800 &&
                (ucs4 <= 0xDFFF || ucs4 >= UCS_END))
        {
          // Surrogates and code points outside the Unicode range.
          ucs4 = UCS2_REPLACEMENT_CHAR;
        }

    *buffer = p;
    return ucs4;
  }

private:
  static bool CalcState(char c, PRUint32& ucs4, PRUint32& minUcs4,
                          PRInt32& state)
  {
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
        return false;
      }

    return true;
  }

  static bool AddByte(char c, PRInt32 state, PRUint32& ucs4)
  {
    if ( UTF8traits::isInSeq(c) )
      {
        PRInt32 shift = state * 6;
        ucs4 |= (PRUint32(c) & 0x3F) << shift;
        return true;
      }

    return false;
  }
};


/**
 * Extract the next UCS-4 character from the buffer and return it.  The
 * pointer passed in is advanced to the start of the next character in the
 * buffer.  If non-null, the err parameter is filled in if an error occurs.
 */


class UTF16CharEnumerator
{
public:
  static PRUint32 NextChar(const PRUnichar **buffer, const PRUnichar *end,
                           bool *err = nsnull)
  {
    NS_ASSERTION(buffer && *buffer, "null buffer!");

    const PRUnichar *p = *buffer;

    if (p >= end)
      {
        NS_ERROR("No input to work with");
        if (err)
          *err = true;

        return 0;
      }

    PRUnichar c = *p++;

    if (!IS_SURROGATE(c)) // U+0000 - U+D7FF,U+E000 - U+FFFF
      {
        if (err)
          *err = false;
        *buffer = p;
        return c;
      }
    else if (NS_IS_HIGH_SURROGATE(c)) // U+D800 - U+DBFF
      {
        if (p == end)
          {
            // Found a high surrogate the end of the buffer. Flag this
            // as an error and return the Unicode replacement
            // character 0xFFFD.

            NS_WARNING("Unexpected end of buffer after high surrogate");

            if (err)
              *err = true;
            *buffer = p;
            return 0xFFFD;
          }

        // D800- DBFF - High Surrogate
        PRUnichar h = c;

        c = *p++;

        if (NS_IS_LOW_SURROGATE(c))
          {
            // DC00- DFFF - Low Surrogate
            // N = (H - D800) *400 + 10000 + (L - DC00)
            PRUint32 ucs4 = SURROGATE_TO_UCS4(h, c);
            if (err)
              *err = false;
            *buffer = p;
            return ucs4;
          }
        else
          {
            // Found a high surrogate followed by something other than
            // a low surrogate. Flag this as an error and return the
            // Unicode replacement character 0xFFFD.  Note that the
            // pointer to the next character points to the second 16-bit
            // value, not beyond it, as per Unicode 5.0.0 Chapter 3 C10,
            // only the first code unit of an illegal sequence must be
            // treated as an illegally terminated code unit sequence
            // (also Chapter 3 D91, "isolated [not paired and ill-formed]
            // UTF-16 code units in the range D800..DFFF are ill-formed").
            NS_WARNING("got a High Surrogate but no low surrogate");

            if (err)
              *err = true;
            *buffer = p - 1;
            return 0xFFFD;
          }
      }
    else // U+DC00 - U+DFFF
      {
        // DC00- DFFF - Low Surrogate

        // Found a low surrogate w/o a preceding high surrogate. Flag
        // this as an error and return the Unicode replacement
        // character 0xFFFD.

        NS_WARNING("got a low Surrogate but no high surrogate");
        if (err)
          *err = true;
        *buffer = p;
        return 0xFFFD;
      }

    if (err)
      *err = true;
    return 0;
  }
};


/**
 * A character sink (see |copy_string| in nsAlgorithm.h) for converting
 * UTF-8 to UTF-16
 */
class ConvertUTF8toUTF16
  {
    public:
      typedef char      value_type;
      typedef PRUnichar buffer_type;

    ConvertUTF8toUTF16( buffer_type* aBuffer )
        : mStart(aBuffer), mBuffer(aBuffer), mErrorEncountered(false) {}

    size_t Length() const { return mBuffer - mStart; }

    bool ErrorEncountered() const { return mErrorEncountered; }

    void NS_ALWAYS_INLINE write( const value_type* start, PRUint32 N )
      {
        if ( mErrorEncountered )
          return;

        // algorithm assumes utf8 units won't
        // be spread across fragments
        const value_type* p = start;
        const value_type* end = start + N;
        buffer_type* out = mBuffer;
        for ( ; p != end /* && *p */; )
          {
            bool err;
            PRUint32 ucs4 = UTF8CharEnumerator::NextChar(&p, end, &err);

            if ( err )
              {
                mErrorEncountered = true;
                mBuffer = out;
                return;
              }

            if ( ucs4 >= PLANE1_BASE )
              {
                *out++ = (buffer_type)H_SURROGATE(ucs4);
                *out++ = (buffer_type)L_SURROGATE(ucs4);
              }
            else
              {
                *out++ = ucs4;
              }
          }
        mBuffer = out;
      }

    void write_terminator()
      {
        *mBuffer = buffer_type(0);
      }

    private:
      buffer_type* const mStart;
      buffer_type* mBuffer;
      bool mErrorEncountered;
  };

/**
 * A character sink (see |copy_string| in nsAlgorithm.h) for computing
 * the length of the UTF-16 string equivalent to a UTF-8 string.
 */
class CalculateUTF8Length
  {
    public:
      typedef char value_type;

    CalculateUTF8Length() : mLength(0), mErrorEncountered(false) { }

    size_t Length() const { return mLength; }

    void NS_ALWAYS_INLINE write( const value_type* start, PRUint32 N )
      {
          // ignore any further requests
        if ( mErrorEncountered )
            return;

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
                // Because a UTF-8 sequence of 4 bytes represents a codepoint
                // greater than 0xFFFF, it will become a surrogate pair in the
                // UTF-16 string, so add 1 more to mLength.
                // This doesn't happen with is5byte and is6byte because they
                // are illegal UTF-8 sequences (greater than 0x10FFFF) so get
                // converted to a single replacement character.

                // However, there is one case when a 4 byte UTF-8 sequence will
                // only generate 2 UTF-16 bytes. If we have a properly encoded
                // sequence, but with an invalid value (too small or too big),
                // that will result in a replacement character being written
                // This replacement character is encoded as just 1 single
                // UTF-16 character, which is 2 bytes.

                // The below code therefore only adds 1 to mLength if the UTF8
                // data will produce a decoded character which is greater than
                // or equal to 0x010000 and less than 0x0110000.

                // A 4byte UTF8 character is encoded as
                // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                // Bit 1-3 on the first byte, and bit 5-6 on the second byte,
                // map to bit 17-21 in the final result. If these bits are
                // between 0x01 and 0x11, that means that the final result is
                // between 0x010000 and 0x110000. The below code reads these
                // bits out and assigns them to c, but shifted up 4 bits to
                // avoid having to shift twice.

                // It doesn't matter what to do in the case where p + 4 > end
                // since no UTF16 characters will be written in that case by
                // ConvertUTF8toUTF16. Likewise it doesn't matter what we do if
                // any of the surrogate bits are wrong since no UTF16
                // characters will be written in that case either.

                if (p + 4 <= end) {
                  PRUint32 c = ((PRUint32)(p[0] & 0x07)) << 6 |
                               ((PRUint32)(p[1] & 0x30));
                  if (c >= 0x010 && c < 0x110)
                    ++mLength;
                }

                p += 4;
            }
            else if ( UTF8traits::is5byte(*p) )
                p += 5;
            else if ( UTF8traits::is6byte(*p) )
                p += 6;
            else // error
              {
                ++mLength; // to account for the decrement below
                break;
              }
          }
        if ( p != end )
          {
            NS_ERROR("Not a UTF-8 string. This code should only be used for converting from known UTF-8 strings.");
            --mLength; // The last multi-byte char wasn't complete, discard it.
            mErrorEncountered = true;
          }
      }

    private:
      size_t mLength;
      bool mErrorEncountered;
  };

/**
 * A character sink (see |copy_string| in nsAlgorithm.h) for
 * converting UTF-16 to UTF-8. Treats invalid UTF-16 data as 0xFFFD
 * (0xEFBFBD in UTF-8).
 */
class ConvertUTF16toUTF8
  {
    public:
      typedef PRUnichar value_type;
      typedef char      buffer_type;

    // The error handling here is more lenient than that in
    // |ConvertUTF8toUTF16|, but it's that way for backwards
    // compatibility.

    ConvertUTF16toUTF8( buffer_type* aBuffer )
        : mStart(aBuffer), mBuffer(aBuffer) {}

    size_t Size() const { return mBuffer - mStart; }

    void NS_ALWAYS_INLINE write( const value_type* start, PRUint32 N )
      {
        buffer_type *out = mBuffer; // gcc isn't smart enough to do this!

        for (const value_type *p = start, *end = start + N; p < end; ++p )
          {
            value_type c = *p;
            if (! (c & 0xFF80)) // U+0000 - U+007F
              {
                *out++ = (char)c;
              }
            else if (! (c & 0xF800)) // U+0100 - U+07FF
              {
                *out++ = 0xC0 | (char)(c >> 6);
                *out++ = 0x80 | (char)(0x003F & c);
              }
            else if (!IS_SURROGATE(c)) // U+0800 - U+D7FF,U+E000 - U+FFFF
              {
                *out++ = 0xE0 | (char)(c >> 12);
                *out++ = 0x80 | (char)(0x003F & (c >> 6));
                *out++ = 0x80 | (char)(0x003F & c );
              }
            else if (NS_IS_HIGH_SURROGATE(c)) // U+D800 - U+DBFF
              {
                // D800- DBFF - High Surrogate
                value_type h = c;

                ++p;
                if (p == end)
                  {
                    // Treat broken characters as the Unicode
                    // replacement character 0xFFFD (0xEFBFBD in
                    // UTF-8)
                    *out++ = '\xEF';
                    *out++ = '\xBF';
                    *out++ = '\xBD';

                    NS_WARNING("String ending in half a surrogate pair!");

                    break;
                  }
                c = *p;

                if (NS_IS_LOW_SURROGATE(c))
                  {
                    // DC00- DFFF - Low Surrogate
                    // N = (H - D800) *400 + 10000 + ( L - DC00 )
                    PRUint32 ucs4 = SURROGATE_TO_UCS4(h, c);

                    // 0001 0000-001F FFFF
                    *out++ = 0xF0 | (char)(ucs4 >> 18);
                    *out++ = 0x80 | (char)(0x003F & (ucs4 >> 12));
                    *out++ = 0x80 | (char)(0x003F & (ucs4 >> 6));
                    *out++ = 0x80 | (char)(0x003F & ucs4);
                  }
                else
                  {
                    // Treat broken characters as the Unicode
                    // replacement character 0xFFFD (0xEFBFBD in
                    // UTF-8)
                    *out++ = '\xEF';
                    *out++ = '\xBF';
                    *out++ = '\xBD';

                    // The pointer to the next character points to the second
                    // 16-bit value, not beyond it, as per Unicode 5.0.0
                    // Chapter 3 C10, only the first code unit of an illegal
                    // sequence must be treated as an illegally terminated
                    // code unit sequence (also Chapter 3 D91, "isolated [not
                    // paired and ill-formed] UTF-16 code units in the range
                    // D800..DFFF are ill-formed").
                    p--;

                    NS_WARNING("got a High Surrogate but no low surrogate");
                  }
              }
            else // U+DC00 - U+DFFF
              {
                // Treat broken characters as the Unicode replacement
                // character 0xFFFD (0xEFBFBD in UTF-8)
                *out++ = '\xEF';
                *out++ = '\xBF';
                *out++ = '\xBD';

                // DC00- DFFF - Low Surrogate
                NS_WARNING("got a low Surrogate but no high surrogate");
              }
          }

        mBuffer = out;
      }

    void write_terminator()
      {
        *mBuffer = buffer_type(0);
      }

    private:
      buffer_type* const mStart;
      buffer_type* mBuffer;
  };

/**
 * A character sink (see |copy_string| in nsAlgorithm.h) for computing
 * the number of bytes a UTF-16 would occupy in UTF-8. Treats invalid
 * UTF-16 data as 0xFFFD (0xEFBFBD in UTF-8).
 */
class CalculateUTF8Size
  {
    public:
      typedef PRUnichar value_type;

    CalculateUTF8Size()
      : mSize(0) { }

    size_t Size() const { return mSize; }

    void NS_ALWAYS_INLINE write( const value_type* start, PRUint32 N )
      {
        // Assume UCS2 surrogate pairs won't be spread across fragments.
        for (const value_type *p = start, *end = start + N; p < end; ++p )
          {
            value_type c = *p;
            if (! (c & 0xFF80)) // U+0000 - U+007F
              mSize += 1;
            else if (! (c & 0xF800)) // U+0100 - U+07FF
              mSize += 2;
            else if (0xD800 != (0xF800 & c)) // U+0800 - U+D7FF,U+E000 - U+FFFF
              mSize += 3;
            else if (0xD800 == (0xFC00 & c)) // U+D800 - U+DBFF
              {
                ++p;
                if (p == end)
                  {
                    // Treat broken characters as the Unicode
                    // replacement character 0xFFFD (0xEFBFBD in
                    // UTF-8)
                    mSize += 3;

                    NS_WARNING("String ending in half a surrogate pair!");

                    break;
                  }
                c = *p;

                if (0xDC00 == (0xFC00 & c))
                  mSize += 4;
                else
                  {
                    // Treat broken characters as the Unicode
                    // replacement character 0xFFFD (0xEFBFBD in
                    // UTF-8)
                    mSize += 3;

                    // The next code unit is the second 16-bit value, not
                    // the one beyond it, as per Unicode 5.0.0 Chapter 3 C10,
                    // only the first code unit of an illegal sequence must
                    // be treated as an illegally terminated code unit
                    // sequence (also Chapter 3 D91, "isolated [not paired and
                    // ill-formed] UTF-16 code units in the range D800..DFFF
                    // are ill-formed").
                    p--;

                    NS_WARNING("got a high Surrogate but no low surrogate");
                  }
              }
            else // U+DC00 - U+DFFF
              {
                // Treat broken characters as the Unicode replacement
                // character 0xFFFD (0xEFBFBD in UTF-8)
                mSize += 3;

                NS_WARNING("got a low Surrogate but no high surrogate");
              }
          }
      }

    private:
      size_t mSize;
  };

#ifdef MOZILLA_INTERNAL_API
/**
 * A character sink that performs a |reinterpret_cast|-style conversion
 * from char to PRUnichar.
 */
class LossyConvertEncoding8to16
  {
    public:
      typedef char      value_type;
      typedef char      input_type;
      typedef PRUnichar output_type;

    public:
      LossyConvertEncoding8to16( PRUnichar* aDestination ) :
        mDestination(aDestination) { }

      void
      write( const char* aSource, PRUint32 aSourceLength )
        {
#ifdef MOZILLA_MAY_SUPPORT_SSE2
          if (mozilla::supports_sse2())
            {
              write_sse2(aSource, aSourceLength);
              return;
            }
#endif
          const char* done_writing = aSource + aSourceLength;
          while ( aSource < done_writing )
            *mDestination++ = (PRUnichar)(unsigned char)(*aSource++);
        }

      void
      write_sse2( const char* aSource, PRUint32 aSourceLength );

      void
      write_terminator()
        {
          *mDestination = (PRUnichar)(0);
        }

    private:
      PRUnichar* mDestination;
  };

/**
 * A character sink that performs a |reinterpret_cast|-style conversion
 * from PRUnichar to char.
 */
class LossyConvertEncoding16to8
  {
    public:
      typedef PRUnichar value_type;
      typedef PRUnichar input_type;
      typedef char      output_type;

      LossyConvertEncoding16to8( char* aDestination ) : mDestination(aDestination) { }

      void
      write( const PRUnichar* aSource, PRUint32 aSourceLength)
        {
#ifdef MOZILLA_MAY_SUPPORT_SSE2
          if (mozilla::supports_sse2())
            {
              write_sse2(aSource, aSourceLength);
              return;
            }
#endif
            const PRUnichar* done_writing = aSource + aSourceLength;
            while ( aSource < done_writing )
              *mDestination++ = (char)(*aSource++);
        }

#ifdef MOZILLA_MAY_SUPPORT_SSE2
      void
      write_sse2( const PRUnichar* aSource, PRUint32 aSourceLength );
#endif

      void
      write_terminator()
        {
          *mDestination = '\0';
        }

    private:
      char *mDestination;
  };
#endif // MOZILLA_INTERNAL_API

#endif /* !defined(nsUTF8Utils_h_) */
