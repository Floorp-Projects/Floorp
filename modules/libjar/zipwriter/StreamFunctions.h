/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _nsStreamFunctions_h_
#define _nsStreamFunctions_h_

#include "nscore.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

/*
 * ZIP file data is stored little-endian. These are helper functions to read and
 * write little endian data to/from a char buffer.
 * The off argument, where present, is incremented according to the number of
 * bytes consumed from the buffer.
 */
inline void WRITE8(uint8_t* buf, uint32_t* off, uint8_t val)
{
  buf[(*off)++] = val;
}

inline void WRITE16(uint8_t* buf, uint32_t* off, uint16_t val)
{
  WRITE8(buf, off, val & 0xff);
  WRITE8(buf, off, (val >> 8) & 0xff);
}

inline void WRITE32(uint8_t* buf, uint32_t* off, uint32_t val)
{
  WRITE16(buf, off, val & 0xffff);
  WRITE16(buf, off, (val >> 16) & 0xffff);
}

inline uint8_t READ8(const uint8_t* buf, uint32_t* off)
{
  return buf[(*off)++];
}

inline uint16_t READ16(const uint8_t* buf, uint32_t* off)
{
  uint16_t val = READ8(buf, off);
  val |= READ8(buf, off) << 8;
  return val;
}

inline uint32_t READ32(const uint8_t* buf, uint32_t* off)
{
  uint32_t val = READ16(buf, off);
  val |= READ16(buf, off) << 16;
  return val;
}

inline uint32_t PEEK32(const uint8_t* buf)
{
  return (uint32_t)( (buf [0]      ) |
                     (buf [1] <<  8) |
                     (buf [2] << 16) |
                     (buf [3] << 24) );
}

nsresult ZW_ReadData(nsIInputStream *aStream, char *aBuffer, uint32_t aCount);

nsresult ZW_WriteData(nsIOutputStream *aStream, const char *aBuffer,
                      uint32_t aCount);

#endif
