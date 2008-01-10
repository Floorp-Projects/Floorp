/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christian Biesinger <cbiesinger@web.de>
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
 * ***** END LICENSE BLOCK ***** */

#ifndef nsReadLine_h__
#define nsReadLine_h__

#include "prmem.h"
#include "nsIInputStream.h"

/**
 * @file
 * Functions to read complete lines from an input stream.
 *
 * To properly use the helper function in here (NS_ReadLine) the caller
 * needs to declare a pointer to an nsLineBuffer, call
 * NS_InitLineBuffer on it, and pass it to NS_ReadLine every time it
 * wants a line out.
 *
 * When done, the pointer should be freed using PR_Free.
 */

/**
 * @internal
 * Buffer size. This many bytes will be buffered. If a line is longer than this,
 * the partial line will be appended to the out parameter of NS_ReadLine and the
 * buffer will be emptied.
 */
#define kLineBufferSize 4096

/**
 * @internal
 * Line buffer structure, buffers data from an input stream.
 */
template<typename CharT>
class nsLineBuffer {
  public:
  CharT buf[kLineBufferSize+1];
  CharT* start;
  CharT* current;
  CharT* end;
  PRBool empty;
};

/**
 * Initialize a line buffer for use with NS_ReadLine.
 *
 * @param aBufferPtr
 *        Pointer to pointer to a line buffer. Upon successful return,
 *        *aBufferPtr will contain a valid pointer to a line buffer, for use
 *        with NS_ReadLine. Use PR_Free when the buffer is no longer needed.
 *
 * @retval NS_OK Success.
 * @retval NS_ERROR_OUT_OF_MEMORY Not enough memory to allocate the line buffer.
 *
 * @par Example:
 * @code
 *    nsLineBuffer* lb;
 *    rv = NS_InitLineBuffer(&lb);
 *    if (NS_SUCCEEDED(rv)) {
 *      // do stuff...
 *      PR_Free(lb);
 *    }
 * @endcode
 */
template<typename CharT>
nsresult
NS_InitLineBuffer (nsLineBuffer<CharT> ** aBufferPtr) {
  *aBufferPtr = PR_NEW(nsLineBuffer<CharT>);
  if (!(*aBufferPtr))
    return NS_ERROR_OUT_OF_MEMORY;

  (*aBufferPtr)->start = (*aBufferPtr)->current = (*aBufferPtr)->end = (*aBufferPtr)->buf;
  (*aBufferPtr)->empty = PR_TRUE;
  return NS_OK;
}

/**
 * Read a line from an input stream. Lines are separated by '\r' (0x0D) or '\n'
 * (0x0A), or "\r\n" or "\n\r".
 *
 * @param aStream
 *        The stream to read from
 * @param aBuffer
 *        The line buffer to use. Must have been inited with
 *        NS_InitLineBuffer before. A single line buffer must not be used with
 *        different input streams.
 * @param aLine [out]
 *        The string where the line will be stored.
 * @param more [out]
 *        Whether more data is available in the buffer. If true, NS_ReadLine may
 *        be called again to read further lines. Otherwise, further calls to
 *        NS_ReadLine will return an error.
 *
 * @retval NS_OK
 *         Read successful
 * @retval error
 *         Input stream returned an error upon read. See
 *         nsIInputStream::read.
 */
template<typename CharT, class StreamType, class StringType>
nsresult
NS_ReadLine (StreamType* aStream, nsLineBuffer<CharT> * aBuffer,
             StringType & aLine, PRBool *more) {
  nsresult rv = NS_OK;
  PRUint32 bytesRead;
  *more = PR_TRUE;
  PRBool eolStarted = PR_FALSE;
  CharT eolchar = '\0';
  aLine.Truncate();
  while (1) { // will be returning out of this loop on eol or eof
    if (aBuffer->empty) { // buffer is empty.  Read into it.
      rv = aStream->Read(aBuffer->buf, kLineBufferSize, &bytesRead);
      if (NS_FAILED(rv)) // read failed
        return rv;
      if (bytesRead == 0) { // end of file
        *more = PR_FALSE;
        return NS_OK;
      }
      aBuffer->end = aBuffer->buf + bytesRead;
      aBuffer->empty = PR_FALSE;
      *(aBuffer->end) = '\0'; // null-terminate this thing
    }
    // walk the buffer looking for an end-of-line
    while (aBuffer->current < aBuffer->end) {
      if (eolStarted) {
          if ((eolchar == '\n' && *(aBuffer->current) == '\r') ||
              (eolchar == '\r' && *(aBuffer->current) == '\n')) { // line end
            (aBuffer->current)++;
            aBuffer->start = aBuffer->current;
          }
          eolStarted = PR_FALSE;
          return NS_OK;
      } else if (*(aBuffer->current) == '\n' ||
                 *(aBuffer->current) == '\r') { // line end
        eolStarted = PR_TRUE;
        eolchar = *(aBuffer->current);
        *(aBuffer->current) = '\0';
        aLine.Append(aBuffer->start);
        (aBuffer->current)++;
        aBuffer->start = aBuffer->current;
      } else {
        eolStarted = PR_FALSE;
        (aBuffer->current)++;
      }
    }

    // append whatever we currently have to the string
    aLine.Append(aBuffer->start);

    // we've run out of buffer.  Begin anew
    aBuffer->current = aBuffer->start = aBuffer->buf;
    aBuffer->empty = PR_TRUE;
    
    if (eolStarted) {  // have to read another char and possibly skip over it
      rv = aStream->Read(aBuffer->buf, 1, &bytesRead);
      if (NS_FAILED(rv)) // read failed
        return rv;
      if (bytesRead == 0) { // end of file
        *more = PR_FALSE;
        return NS_OK;
      }
      if ((eolchar == '\n' && *(aBuffer->buf) == '\r') ||
          (eolchar == '\r' && *(aBuffer->buf) == '\n')) {
        // Just return and all is good -- we've skipped the extra newline char
        return NS_OK;
      } else {
        // we have a byte that we should look at later
        aBuffer->empty = PR_FALSE;
        aBuffer->end = aBuffer->buf + 1;
        *(aBuffer->end) = '\0';
      }
    }
  }
}

#endif // nsReadLine_h__
