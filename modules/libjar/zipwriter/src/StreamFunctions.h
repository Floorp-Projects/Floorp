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
inline NS_HIDDEN_(void) WRITE8(PRUint8* buf, PRUint32* off, PRUint8 val)
{
  buf[(*off)++] = val;
}

inline NS_HIDDEN_(void) WRITE16(PRUint8* buf, PRUint32* off, PRUint16 val)
{
  WRITE8(buf, off, val & 0xff);
  WRITE8(buf, off, (val >> 8) & 0xff);
}

inline NS_HIDDEN_(void) WRITE32(PRUint8* buf, PRUint32* off, PRUint32 val)
{
  WRITE16(buf, off, val & 0xffff);
  WRITE16(buf, off, (val >> 16) & 0xffff);
}

inline NS_HIDDEN_(PRUint8) READ8(const PRUint8* buf, PRUint32* off)
{
  return buf[(*off)++];
}

inline NS_HIDDEN_(PRUint16) READ16(const PRUint8* buf, PRUint32* off)
{
  PRUint16 val = READ8(buf, off);
  val |= READ8(buf, off) << 8;
  return val;
}

inline NS_HIDDEN_(PRUint32) READ32(const PRUint8* buf, PRUint32* off)
{
  PRUint32 val = READ16(buf, off);
  val |= READ16(buf, off) << 16;
  return val;
}

inline NS_HIDDEN_(PRUint32) PEEK32(const PRUint8* buf)
{
  return (PRUint32)( (buf [0]      ) |
                     (buf [1] <<  8) |
                     (buf [2] << 16) |
                     (buf [3] << 24) );
}

NS_HIDDEN_(nsresult) ZW_ReadData(nsIInputStream *aStream, char *aBuffer, PRUint32 aCount);

NS_HIDDEN_(nsresult) ZW_WriteData(nsIOutputStream *aStream, const char *aBuffer,
                      PRUint32 aCount);

#endif
