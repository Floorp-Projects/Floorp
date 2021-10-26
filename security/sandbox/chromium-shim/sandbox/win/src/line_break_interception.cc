/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandbox/win/src/line_break_interception.h"

#include <winnls.h>

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/line_break_common.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"

namespace sandbox {

static const int kBreakSearchRange = 32;

ResultCode GetComplexLineBreaksProxy(const wchar_t* aText, uint32_t aLength,
                                     uint8_t* aBreakBefore) {
  // Make sure that a test length for kMaxBrokeredLen hasn't been set too small
  // allowing for a surrogate pair at the end of a chunk as well.
  DCHECK(kMaxBrokeredLen > kBreakSearchRange + 1);

  void* memory = GetGlobalIPCMemory();
  if (!memory) {
    return SBOX_ERROR_NO_SPACE;
  }

  memset(aBreakBefore, false, aLength);

  SharedMemIPCClient ipc(memory);

  uint8_t* breakBeforeIter = aBreakBefore;
  const wchar_t* textIterEnd = aText + aLength;
  do {
    // Next chunk is either the remaining text or kMaxBrokeredLen long.
    const wchar_t* textIter = aText + (breakBeforeIter - aBreakBefore);
    const wchar_t* chunkEnd = textIter + kMaxBrokeredLen;
    if (chunkEnd < textIterEnd) {
      // Make sure we don't split a surrogate pair.
      if (IS_HIGH_SURROGATE(*(chunkEnd - 1))) {
        --chunkEnd;
      }
    } else {
      // This chunk handles all the (remaining) text.
      chunkEnd = textIterEnd;
    }

    uint32_t len = chunkEnd - textIter;
    // CountedBuffer takes a wchar_t* even though it doesn't change the buffer.
    CountedBuffer textBuf(const_cast<wchar_t*>(textIter),
                          sizeof(wchar_t) * len);
    InOutCountedBuffer breakBeforeBuf(breakBeforeIter, len);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IpcTag::GETCOMPLEXLINEBREAKS, textBuf, len,
                                breakBeforeBuf, &answer);
    if (SBOX_ALL_OK != code) {
      return code;
    }

    if (answer.win32_result) {
      ::SetLastError(answer.win32_result);
      return SBOX_ERROR_GENERIC;
    }

    if (chunkEnd == textIterEnd) {
      break;
    }

    // We couldn't process all of the text in one go, so back up by 32 chars
    // and look for a break, then continue from that position.
    uint8_t* processedToEnd = breakBeforeIter + len;
    breakBeforeIter = processedToEnd - kBreakSearchRange;
    while (!*breakBeforeIter) {
      if (++breakBeforeIter == processedToEnd) {
        break;
      }
    }
  } while (true);

  // Whether we can break before the first character is decided by our caller
  aBreakBefore[0] = false;

  return SBOX_ALL_OK;
}

}  // namespace sandbox
