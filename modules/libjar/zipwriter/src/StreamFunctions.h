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
 * The Original Code is Zip Writer Component.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Mook <mook.moz+random.code@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

#ifndef _nsStreamFunctions_h_
#define _nsStreamFunctions_h_

#include "nscore.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

/*
 * ZIP file data is stored little-endian. These are helper functions to read and
 * write little endian data to/from a char buffer.
 * The off argument is incremented according to the number of bytes consumed
 * from the buffer.
 */
inline NS_HIDDEN_(void) WRITE8(char* buf, PRUint32* off, PRUint8 val)
{
  buf[(*off)++] = val & 0xff;
}

inline NS_HIDDEN_(void) WRITE16(char* buf, PRUint32* off, PRUint16 val)
{
  buf[(*off)++] = val & 0xff;
  buf[(*off)++] = (val >> 8) & 0xff;
}

inline NS_HIDDEN_(void) WRITE32(char* buf, PRUint32* off, PRUint32 val)
{
  buf[(*off)++] = val & 0xff;
  buf[(*off)++] = (val >> 8) & 0xff;
  buf[(*off)++] = (val >> 16) & 0xff;
  buf[(*off)++] = (val >> 24) & 0xff;
}

inline NS_HIDDEN_(PRUint8) READ8(char* buf, PRUint32* off)
{
  return (PRUint8)buf[(*off)++];
}

inline NS_HIDDEN_(PRUint16) READ16(char* buf, PRUint32* off)
{
  PRUint16 val = (PRUint16)buf[(*off)++] & 0xff;
  val |= ((PRUint16)buf[(*off)++] & 0xff) << 8;
  return val;
}

inline NS_HIDDEN_(PRUint32) READ32(char* buf, PRUint32* off)
{
  PRUint32 val = (PRUint32)buf[(*off)++] & 0xff;
  val |= ((PRUint32)buf[(*off)++] & 0xff) << 8;
  val |= ((PRUint32)buf[(*off)++] & 0xff) << 16;
  val |= ((PRUint32)buf[(*off)++] & 0xff) << 24;
  return val;
}

NS_HIDDEN_(nsresult) ZW_ReadData(nsIInputStream *aStream, char *aBuffer, PRUint32 aCount);

NS_HIDDEN_(nsresult) ZW_WriteData(nsIOutputStream *aStream, const char *aBuffer,
                      PRUint32 aCount);

#endif
