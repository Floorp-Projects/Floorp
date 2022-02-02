/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "nsClipboardWayland.h"
#include "nsSupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsImageToPixbuf.h"
#include "nsStringStream.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ScopeExit.h"
#include "nsWindow.h"

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

using namespace mozilla;
using namespace mozilla::widget;

nsRetrievalContextWayland::nsRetrievalContextWayland()
    : mMutex("nsRetrievalContextWayland") {}

struct AsyncClipboardData {
  AsyncClipboardData(ClipboardDataType aDataType, int aClipboardRequestNumber,
                     RefPtr<nsRetrievalContextWayland> aRetrievalContex)
      : mClipboardRequestNumber(aClipboardRequestNumber),
        mRetrievalContex(std::move(aRetrievalContex)),
        mDataType(aDataType) {}
  int mClipboardRequestNumber;
  RefPtr<nsRetrievalContextWayland> mRetrievalContex;
  ClipboardDataType mDataType;
};

static void wayland_clipboard_contents_received_async(
    GtkClipboard* clipboard, GtkSelectionData* selection_data, gpointer data) {
  LOGCLIP("wayland_clipboard_contents_received_async() selection_data = %p\n",
          selection_data);
  auto* fastTrack = static_cast<AsyncClipboardData*>(data);
  fastTrack->mRetrievalContex->TransferClipboardData(
      fastTrack->mDataType, fastTrack->mClipboardRequestNumber, selection_data);
  delete fastTrack;
}

static void wayland_clipboard_text_received(GtkClipboard* clipboard,
                                            const gchar* text, gpointer data) {
  LOGCLIP("wayland_clipboard_text_received() text = %p\n", text);
  auto* fastTrack = static_cast<AsyncClipboardData*>(data);
  fastTrack->mRetrievalContex->TransferClipboardData(
      fastTrack->mDataType, fastTrack->mClipboardRequestNumber, (void*)text);
  delete fastTrack;
}

void nsRetrievalContextWayland::TransferClipboardData(
    ClipboardDataType aDataType, int aClipboardRequestNumber,
    const void* aData) {
  LOGCLIP(
      "nsRetrievalContextWayland::TransferClipboardData(), "
      "aSelectionData = %p\n",
      aData);

  if (mClipboardRequestNumber != aClipboardRequestNumber) {
    LOGCLIP("    request number does not match!\n");
    return;
  }
  LOGCLIP("    request number matches\n");

  MOZ_RELEASE_ASSERT(mClipboardData.isNothing(),
                     "Clipboard contains old data?");

  uint32_t dataLength = 0;
  if (aDataType == ClipboardDataType::Targets ||
      aDataType == ClipboardDataType::Data) {
    dataLength = gtk_selection_data_get_length((GtkSelectionData*)aData);
  } else {
    dataLength = aData ? strlen((const char*)aData) : 0;
  }

  mClipboardData = Some(ClipboardData());

  // Negative size means no data or data error.
  if (dataLength <= 0) {
    LOGCLIP("    zero dataLength, quit.\n");
    return;
  }

  switch (aDataType) {
    case ClipboardDataType::Targets: {
      LOGCLIP("    getting %d bytes of clipboard targets.\n", dataLength);
      gint n_targets = 0;
      GdkAtom* targets = nullptr;
      if (!gtk_selection_data_get_targets((GtkSelectionData*)aData, &targets,
                                          &n_targets) ||
          !n_targets) {
        // We failed to get targets
        return;
      }
      mClipboardData->SetTargets(
          ClipboardTargets{GUniquePtr<GdkAtom>(targets), uint32_t(n_targets)});
      break;
    }
    case ClipboardDataType::Text: {
      LOGCLIP("    getting %d bytes of text.\n", dataLength);
      mClipboardData->SetText(
          Span(static_cast<const char*>(aData), dataLength));
      LOGCLIP("    done, mClipboardData = %p\n", mClipboardData->mData.get());
      break;
    }
    case ClipboardDataType::Data: {
      LOGCLIP("    getting %d bytes of data.\n", dataLength);
      mClipboardData->SetData(Span(
          gtk_selection_data_get_data((GtkSelectionData*)aData), dataLength));
      LOGCLIP("    done, mClipboardData = %p\n", mClipboardData->mData.get());
      break;
    }
  }
}

ClipboardTargets nsRetrievalContextWayland::GetTargets(
    int32_t aWhichClipboard) {
  LOGCLIP("nsRetrievalContextWayland::GetTargets()\n");

  if (!mMutex.TryLock()) {
    LOGCLIP("  nsRetrievalContextWayland is already used!\n");
    return {};
  }
  auto releaseLock = MakeScopeExit([&] { mMutex.Unlock(); });

  int clipboardRequest = PrepareNewClipboardRequest();

  GdkAtom selection = GetSelectionAtom(aWhichClipboard);
  gtk_clipboard_request_contents(
      gtk_clipboard_get(selection), gdk_atom_intern("TARGETS", FALSE),
      wayland_clipboard_contents_received_async,
      new AsyncClipboardData(ClipboardDataType::Targets, clipboardRequest,
                             this));

  return WaitForClipboardContent().ExtractTargets();
}

ClipboardData nsRetrievalContextWayland::GetClipboardData(
    const char* aMimeType, int32_t aWhichClipboard) {
  LOGCLIP("nsRetrievalContextWayland::GetClipboardData() mime %s\n", aMimeType);

  if (!mMutex.TryLock()) {
    LOGCLIP("  nsRetrievalContextWayland is already used!\n");
    return {};
  }
  auto releaseLock = MakeScopeExit([&] { mMutex.Unlock(); });

  int clipboardRequest = PrepareNewClipboardRequest();

  GdkAtom selection = GetSelectionAtom(aWhichClipboard);
  gtk_clipboard_request_contents(
      gtk_clipboard_get(selection), gdk_atom_intern(aMimeType, FALSE),
      wayland_clipboard_contents_received_async,
      new AsyncClipboardData(ClipboardDataType::Data, clipboardRequest, this));

  return WaitForClipboardContent();
}

GUniquePtr<char> nsRetrievalContextWayland::GetClipboardText(
    int32_t aWhichClipboard) {
  GdkAtom selection = GetSelectionAtom(aWhichClipboard);

  LOGCLIP("nsRetrievalContextWayland::GetClipboardText(), clipboard %s\n",
          (selection == GDK_SELECTION_PRIMARY) ? "Primary" : "Selection");

  if (!mMutex.TryLock()) {
    LOGCLIP("  nsRetrievalContextWayland is already used!\n");
    return nullptr;
  }
  auto releaseLock = MakeScopeExit([&] { mMutex.Unlock(); });

  int clipboardRequest = PrepareNewClipboardRequest();
  gtk_clipboard_request_text(
      gtk_clipboard_get(selection), wayland_clipboard_text_received,
      new AsyncClipboardData(ClipboardDataType::Text, clipboardRequest, this));

  return WaitForClipboardContent().mData;
}

int nsRetrievalContextWayland::PrepareNewClipboardRequest() {
  MOZ_DIAGNOSTIC_ASSERT(mClipboardData.isNothing(),
                        "Clipboard contains old data?");
  mClipboardData.reset();
  return ++mClipboardRequestNumber;
}

ClipboardData nsRetrievalContextWayland::WaitForClipboardContent() {
  int iteration = 1;

  PRTime entryTime = PR_Now();
  while (mClipboardData.isNothing()) {
    if (iteration++ > kClipboardFastIterationNum) {
      /* sleep for 10 ms/iteration */
      PR_Sleep(PR_MillisecondsToInterval(10));
      if (PR_Now() - entryTime > kClipboardTimeout) {
        LOGCLIP("  failed to get async clipboard data in time limit\n");
        break;
      }
    }
    LOGCLIP("doing iteration %d msec %ld ...\n", (iteration - 1),
            (long)((PR_Now() - entryTime) / 1000));
    gtk_main_iteration();
  }

  if (mClipboardData.isNothing()) {
    return {};
  }
  return mClipboardData.extract();
}
