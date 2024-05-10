/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboardWayland.h"

#include "AsyncGtkClipboardRequest.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ScopeExit.h"
#include "prtime.h"

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

using namespace mozilla;

nsRetrievalContextWayland::nsRetrievalContextWayland() = default;

ClipboardTargets nsRetrievalContextWayland::GetTargetsImpl(
    int32_t aWhichClipboard) {
  MOZ_CLIPBOARD_LOG("nsRetrievalContextWayland::GetTargetsImpl()\n");

  return WaitForClipboardData(ClipboardDataType::Targets, aWhichClipboard)
      .ExtractTargets();
}

ClipboardData nsRetrievalContextWayland::GetClipboardData(
    const char* aMimeType, int32_t aWhichClipboard) {
  MOZ_CLIPBOARD_LOG("nsRetrievalContextWayland::GetClipboardData() mime %s\n",
                    aMimeType);

  return WaitForClipboardData(ClipboardDataType::Data, aWhichClipboard,
                              aMimeType);
}

GUniquePtr<char> nsRetrievalContextWayland::GetClipboardText(
    int32_t aWhichClipboard) {
  GdkAtom selection = GetSelectionAtom(aWhichClipboard);

  MOZ_CLIPBOARD_LOG(
      "nsRetrievalContextWayland::GetClipboardText(), clipboard %s\n",
      (selection == GDK_SELECTION_PRIMARY) ? "Primary" : "Selection");

  return WaitForClipboardData(ClipboardDataType::Text, aWhichClipboard)
      .ExtractText();
}

ClipboardData nsRetrievalContextWayland::WaitForClipboardData(
    ClipboardDataType aDataType, int32_t aWhichClipboard,
    const char* aMimeType) {
  MOZ_CLIPBOARD_LOG(
      "nsRetrievalContextWayland::WaitForClipboardData, MIME %s\n", aMimeType);

  AsyncGtkClipboardRequest request(aDataType, aWhichClipboard, aMimeType);
  int iteration = 1;

  PRTime entryTime = PR_Now();
  while (!request.HasCompleted()) {
    if (iteration++ > kClipboardFastIterationNum) {
      /* sleep for 10 ms/iteration */
      PR_Sleep(PR_MillisecondsToInterval(10));
      if (PR_Now() - entryTime > kClipboardTimeout) {
        MOZ_CLIPBOARD_LOG(
            "  failed to get async clipboard data in time limit\n");
        break;
      }
    }
    MOZ_CLIPBOARD_LOG("doing iteration %d msec %ld ...\n", (iteration - 1),
                      (long)((PR_Now() - entryTime) / 1000));
    gtk_main_iteration();
  }

  return request.TakeResult();
}
