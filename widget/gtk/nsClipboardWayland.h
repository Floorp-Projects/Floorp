/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardWayland_h_
#define __nsClipboardWayland_h_

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <nsTArray.h>

#include "mozilla/Mutex.h"
#include "nsIThread.h"
#include "mozilla/UniquePtr.h"
#include "nsClipboard.h"
#include "nsWaylandDisplay.h"

class DataOffer {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataOffer)

 public:
  explicit DataOffer(wl_data_offer* aDataOffer);

  virtual bool MatchesOffer(wl_data_offer* aDataOffer) {
    return aDataOffer == mWaylandDataOffer;
  }

  void AddMIMEType(const char* aMimeType);

  GdkAtom* GetTargets(int* aTargetNum);
  bool HasTarget(const char* aMimeType);

  char* GetData(const char* aMimeType, uint32_t* aContentLength);
  char* GetDataAsync(const char* aMimeType, uint32_t* aContentLength);

  void DragOfferAccept(const char* aMimeType);
  void SetDragStatus(GdkDragAction aPreferredAction);

  GdkDragAction GetSelectedDragAction();
  void SetSelectedDragAction(uint32_t aWaylandAction);

  void SetAvailableDragActions(uint32_t aWaylandActions);
  GdkDragAction GetAvailableDragActions();

  void DropDataEnter(GtkWidget* aGtkWidget, uint32_t aTime, nscoord aX,
                     nscoord aY);
  void DropMotion(uint32_t aTime, nscoord aX, nscoord aY);
  void GetLastDropInfo(uint32_t* aTime, nscoord* aX, nscoord* aY);

  GtkWidget* GetWidget() { return mGtkWidget; }
  GList* GetDragTargets();
  char* GetDragData(const char* aMimeType, uint32_t* aContentLength);

 protected:
  virtual ~DataOffer();

 private:
  virtual bool RequestDataTransfer(const char* aMimeType, int fd);

  char* GetDataInternal(const char* aMimeType, uint32_t* aContentLength);
  void GetDataAsyncInternal(const char* aMimeType);
  bool EnsureDataGetterThread();

 private:
  wl_data_offer* mWaylandDataOffer;

  nsTArray<GdkAtom> mTargetMIMETypes;
  mozilla::Mutex mMutex;
  uint32_t mAsyncContentLength;
  char* mAsyncContentData;
  mozilla::Atomic<bool> mGetterFinished;

  uint32_t mSelectedDragAction;
  uint32_t mAvailableDragActions;
  uint32_t mTime;
  GtkWidget* mGtkWidget;
  nscoord mX, mY;
};

class PrimaryDataOffer : public DataOffer {
 public:
  explicit PrimaryDataOffer(gtk_primary_selection_offer* aPrimaryDataOffer);
  explicit PrimaryDataOffer(zwp_primary_selection_offer_v1* aPrimaryDataOffer);

  virtual ~PrimaryDataOffer();

  bool MatchesOffer(wl_data_offer* aDataOffer) override {
    return aDataOffer == (wl_data_offer*)mPrimaryDataOfferGtk ||
           aDataOffer == (wl_data_offer*)mPrimaryDataOfferZwpV1;
  }

 private:
  bool RequestDataTransfer(const char* aMimeType, int fd) override;

  gtk_primary_selection_offer* mPrimaryDataOfferGtk;
  zwp_primary_selection_offer_v1* mPrimaryDataOfferZwpV1;
};

class nsRetrievalContextWayland : public nsRetrievalContext {
 public:
  nsRetrievalContextWayland();

  virtual const char* GetClipboardData(const char* aMimeType,
                                       int32_t aWhichClipboard,
                                       uint32_t* aContentLength) override;
  virtual const char* GetClipboardText(int32_t aWhichClipboard) override;
  virtual void ReleaseClipboardData(const char* aClipboardData) override;

  virtual GdkAtom* GetTargets(int32_t aWhichClipboard,
                              int* aTargetNum) override;
  virtual bool HasSelectionSupport(void) override;

  void RegisterNewDataOffer(wl_data_offer* aDataOffer);
  void RegisterNewDataOffer(gtk_primary_selection_offer* aPrimaryDataOffer);
  void RegisterNewDataOffer(zwp_primary_selection_offer_v1* aPrimaryDataOffer);

  void SetClipboardDataOffer(wl_data_offer* aDataOffer);
  void SetPrimaryDataOffer(gtk_primary_selection_offer* aPrimaryDataOffer);
  void SetPrimaryDataOffer(zwp_primary_selection_offer_v1* aPrimaryDataOffer);
  void AddDragAndDropDataOffer(wl_data_offer* aDataOffer);

  RefPtr<DataOffer> GetDragContext() { return mDragContext; }

  void ClearDragAndDropDataOffer();

  void TransferFastTrackClipboard(ClipboardDataType aDataType,
                                  int aClipboardRequestNumber,
                                  GtkSelectionData* aSelectionData);

 private:
  virtual ~nsRetrievalContextWayland() override;

  RefPtr<DataOffer> FindActiveOffer(wl_data_offer* aDataOffer,
                                    bool aRemove = false);
  void InsertOffer(RefPtr<DataOffer> aDataOffer);

 private:
  RefPtr<mozilla::widget::nsWaylandDisplay> mDisplay;

  // Data offers provided by Wayland data device
  nsTArray<RefPtr<DataOffer>> mActiveOffers;
  RefPtr<DataOffer> mClipboardOffer;
  RefPtr<DataOffer> mPrimaryOffer;
  RefPtr<DataOffer> mDragContext;

  mozilla::Atomic<int> mClipboardRequestNumber;
  char* mClipboardData;
  uint32_t mClipboardDataLength;

// Mime types used for text data at Gtk+, see request_text_received_func()
// at gtkclipboard.c.
#define TEXT_MIME_TYPES_NUM 3
  static const char* sTextMimeTypes[TEXT_MIME_TYPES_NUM];
};

#endif /* __nsClipboardWayland_h_ */
