/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "nsClipboardWaylandAsync.h"
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

nsRetrievalContextWaylandAsync::nsRetrievalContextWaylandAsync(void)
    : mClipboardRequestNumber(0),
      mClipboardDataReceived(),
      mClipboardData(nullptr),
      mClipboardDataLength(0),
      mMutex("nsRetrievalContextWaylandAsync") {}

struct AsyncClipboardData {
  AsyncClipboardData(ClipboardDataType aDataType, int aClipboardRequestNumber,
                     RefPtr<nsRetrievalContextWaylandAsync> aRetrievalContex)
      : mClipboardRequestNumber(aClipboardRequestNumber),
        mRetrievalContex(std::move(aRetrievalContex)),
        mDataType(aDataType) {}
  int mClipboardRequestNumber;
  RefPtr<nsRetrievalContextWaylandAsync> mRetrievalContex;
  ClipboardDataType mDataType;
};

static void wayland_clipboard_contents_received(
    GtkClipboard* clipboard, GtkSelectionData* selection_data, gpointer data) {
  LOGCLIP(("wayland_clipboard_contents_received() selection_data = %p\n",
           selection_data));
  AsyncClipboardData* fastTrack = static_cast<AsyncClipboardData*>(data);
  fastTrack->mRetrievalContex->TransferAsyncClipboardData(
      fastTrack->mDataType, fastTrack->mClipboardRequestNumber, selection_data);
  delete fastTrack;
}

static void wayland_clipboard_text_received(GtkClipboard* clipboard,
                                            const gchar* text, gpointer data) {
  LOGCLIP(("wayland_clipboard_text_received() text = %p\n", text));
  AsyncClipboardData* fastTrack = static_cast<AsyncClipboardData*>(data);
  fastTrack->mRetrievalContex->TransferAsyncClipboardData(
      fastTrack->mDataType, fastTrack->mClipboardRequestNumber, (void*)text);
  delete fastTrack;
}

void nsRetrievalContextWaylandAsync::TransferAsyncClipboardData(
    ClipboardDataType aDataType, int aClipboardRequestNumber,
    const void* aData) {
  LOGCLIP(
      ("nsRetrievalContextWaylandAsync::TransferAsyncClipboardData(), "
       "aSelectionData = %p\n",
       aData));

  MOZ_RELEASE_ASSERT(mClipboardData == nullptr && mClipboardDataLength == 0,
                     "Clipboard contains old data?");

  if (mClipboardRequestNumber != aClipboardRequestNumber) {
    LOGCLIP(("    request number does not match!\n"));
  }
  LOGCLIP(("    request number matches\n"));

  int dataLength = 0;
  if (aDataType == CLIPBOARD_TARGETS || aDataType == CLIPBOARD_DATA) {
    dataLength = gtk_selection_data_get_length((GtkSelectionData*)aData);
  } else {
    dataLength = strlen((const char*)aData);
  }

  mClipboardDataReceived = true;

  // Negative size means no data or data error.
  if (dataLength <= 0) {
    LOGCLIP(("    zero dataLength, quit.\n"));
    return;
  }

  switch (aDataType) {
    case CLIPBOARD_TARGETS: {
      LOGCLIP(("    getting %d bytes of clipboard targets.\n", dataLength));
      gint n_targets = 0;
      GdkAtom* targets = nullptr;
      if (!gtk_selection_data_get_targets((GtkSelectionData*)aData, &targets,
                                          &n_targets) ||
          !n_targets) {
        // We failed to get targes
        return;
      }
      mClipboardData = reinterpret_cast<char*>(targets);
      mClipboardDataLength = n_targets;
      break;
    }
    case CLIPBOARD_TEXT: {
      LOGCLIP(("    getting %d bytes of text.\n", dataLength));
      mClipboardDataLength = dataLength;
      mClipboardData = reinterpret_cast<char*>(
          g_malloc(sizeof(char) * (mClipboardDataLength + 1)));
      memcpy(mClipboardData, aData, sizeof(char) * mClipboardDataLength);
      mClipboardData[mClipboardDataLength] = '\0';
      LOGCLIP(("    done, mClipboardData = %p\n", mClipboardData));
      break;
    }
    case CLIPBOARD_DATA: {
      LOGCLIP(("    getting %d bytes of data.\n", dataLength));
      mClipboardDataLength = dataLength;
      mClipboardData = reinterpret_cast<char*>(
          g_malloc(sizeof(char) * mClipboardDataLength));
      memcpy(mClipboardData,
             gtk_selection_data_get_data((GtkSelectionData*)aData),
             sizeof(char) * mClipboardDataLength);
      LOGCLIP(("    done, mClipboardData = %p\n", mClipboardData));
      break;
    }
  }
}

GdkAtom* nsRetrievalContextWaylandAsync::GetTargets(int32_t aWhichClipboard,
                                                    int* aTargetNum) {
  LOGCLIP(("nsRetrievalContextWaylandAsync::GetTargets()\n"));

  if (!mMutex.TryLock()) {
    LOGCLIP(("  nsRetrievalContextWaylandAsync is already used!\n"));
    *aTargetNum = 0;
    return nullptr;
  }

  // GetTargets() does not use ReleaseClipboardData() so we always need to
  // unlock nsRetrievalContextWaylandAsync.
  auto unlock = mozilla::MakeScopeExit([&] { mMutex.Unlock(); });

  MOZ_RELEASE_ASSERT(mClipboardData == nullptr && mClipboardDataLength == 0,
                     "Clipboard contains old data?");

  GdkAtom selection = GetSelectionAtom(aWhichClipboard);
  mClipboardDataReceived = false;
  mClipboardRequestNumber++;
  gtk_clipboard_request_contents(
      gtk_clipboard_get(selection), gdk_atom_intern("TARGETS", FALSE),
      wayland_clipboard_contents_received,
      new AsyncClipboardData(CLIPBOARD_TARGETS, mClipboardRequestNumber, this));

  if (!WaitForClipboardContent()) {
    *aTargetNum = 0;
    return nullptr;
  }

  // mClipboardDataLength is only signed integer, see
  // nsRetrievalContextWaylandAsync::TransferAsyncClipboardData()
  *aTargetNum = (int)mClipboardDataLength;
  GdkAtom* targets = static_cast<GdkAtom*>((void*)mClipboardData);

  // We don't hold the target list internally but we transfer the ownership.
  mClipboardData = nullptr;
  mClipboardDataLength = 0;

  return targets;
}

const char* nsRetrievalContextWaylandAsync::GetClipboardData(
    const char* aMimeType, int32_t aWhichClipboard, uint32_t* aContentLength) {
  LOGCLIP(("nsRetrievalContextWaylandAsync::GetClipboardData() mime %s\n",
           aMimeType));

  if (!mMutex.TryLock()) {
    LOGCLIP(("  nsRetrievalContextWaylandAsync is already used!\n"));
    *aContentLength = 0;
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(mClipboardData == nullptr && mClipboardDataLength == 0,
                     "Clipboard contains old data?");

  GdkAtom selection = GetSelectionAtom(aWhichClipboard);
  mClipboardDataReceived = false;
  mClipboardRequestNumber++;
  gtk_clipboard_request_contents(
      gtk_clipboard_get(selection), gdk_atom_intern(aMimeType, FALSE),
      wayland_clipboard_contents_received,
      new AsyncClipboardData(CLIPBOARD_DATA, mClipboardRequestNumber, this));

  if (!WaitForClipboardContent()) {
    *aContentLength = 0;
    mMutex.Unlock();
    return nullptr;
  }

  *aContentLength = mClipboardDataLength;
  return reinterpret_cast<const char*>(mClipboardData);
}

const char* nsRetrievalContextWaylandAsync::GetClipboardText(
    int32_t aWhichClipboard) {
  GdkAtom selection = GetSelectionAtom(aWhichClipboard);

  LOGCLIP(("nsRetrievalContextWaylandAsync::GetClipboardText(), clipboard %s\n",
           (selection == GDK_SELECTION_PRIMARY) ? "Primary" : "Selection"));

  if (!mMutex.TryLock()) {
    LOGCLIP(("  nsRetrievalContextWaylandAsync is already used!\n"));
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(mClipboardData == nullptr && mClipboardDataLength == 0,
                     "Clipboard contains old data?");

  mClipboardDataReceived = false;
  mClipboardRequestNumber++;
  gtk_clipboard_request_text(
      gtk_clipboard_get(selection), wayland_clipboard_text_received,
      new AsyncClipboardData(CLIPBOARD_TEXT, mClipboardRequestNumber, this));

  if (!WaitForClipboardContent()) {
    mMutex.Unlock();
    return nullptr;
  }
  return reinterpret_cast<const char*>(mClipboardData);
}

bool nsRetrievalContextWaylandAsync::WaitForClipboardContent() {
  PRTime entryTime = PR_Now();
  while (!mClipboardDataReceived) {
    // check the number of iterations
    LOGCLIP(("doing iteration...\n"));
    PR_Sleep(20 * PR_TicksPerSecond() / 1000); /* sleep for 20 ms/iteration */
    if (PR_Now() - entryTime > kClipboardTimeout) {
      LOGCLIP(("  failed to get async clipboard data in time limit\n"));
      break;
    }
    gtk_main_iteration();
  }
  return mClipboardDataReceived && mClipboardData != nullptr;
}

void nsRetrievalContextWaylandAsync::ReleaseClipboardData(
    const char* aClipboardData) {
  LOGCLIP(("nsRetrievalContextWaylandAsync::ReleaseClipboardData [%p]\n",
           aClipboardData));
  if (aClipboardData != mClipboardData) {
    NS_WARNING("Wayland clipboard: Releasing unknown clipboard data!");
  }
  g_free((void*)mClipboardData);
  mClipboardDataLength = 0;
  mClipboardData = nullptr;

  mMutex.Unlock();
}
