/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "minidump_callback.h"

#include <algorithm>
#include <cassert>

namespace google_breakpad {

BOOL CALLBACK MinidumpWriteDumpCallback(
    PVOID context,
    const PMINIDUMP_CALLBACK_INPUT callback_input,
    PMINIDUMP_CALLBACK_OUTPUT callback_output) {
  switch (callback_input->CallbackType) {
  case MemoryCallback: {
    MinidumpCallbackContext* callback_context =
        reinterpret_cast<MinidumpCallbackContext*>(context);

    if (callback_context->iter == callback_context->end)
      return FALSE;

    // Include the specified memory region.
    callback_output->MemoryBase = callback_context->iter->ptr;
    callback_output->MemorySize = callback_context->iter->length;
    callback_context->iter++;
    return TRUE;
  }

    // Include all modules.
  case IncludeModuleCallback:
  case ModuleCallback:
    return TRUE;

    // Include all threads.
  case IncludeThreadCallback:
  case ThreadCallback:
    return TRUE;

    // Stop receiving cancel callbacks.
  case CancelCallback:
    callback_output->CheckCancel = FALSE;
    callback_output->Cancel = FALSE;
    return TRUE;
  }
  // Ignore other callback types.
  return FALSE;
}

}  // namespace google_breakpad

