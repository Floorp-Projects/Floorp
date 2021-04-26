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

#include "mozilla/UniquePtr.h"
#include "nsClipboard.h"
#include "nsWaylandDisplay.h"

struct FastTrackClipboard;

class DataOffer {
 public:
  void AddMIMEType(const char* aMimeType);

  GdkAtom* GetTargets(int* aTargetNum);
  bool HasTarget(const char* aMimeType);

  char* GetData(wl_display* aDisplay, const char* aMimeType,
                uint32_t* aContentLength);

  virtual ~DataOffer() = default;

 private:
  virtual bool RequestDataTransfer(const char* aMimeType, int fd) = 0;

 protected:
  nsTArray<GdkAtom> mTargetMIMETypes;
};

class WaylandDataOffer : public DataOffer {
 public:
  explicit WaylandDataOffer(wl_data_offer* aWaylandDataOffer);

  void DragOfferAccept(const char* aMimeType, uint32_t aTime);
  void SetDragStatus(GdkDragAction aPreferredAction, uint32_t aTime);

  GdkDragAction GetSelectedDragAction();
  void SetSelectedDragAction(uint32_t aWaylandAction);

  void SetAvailableDragActions(uint32_t aWaylandActions);
  GdkDragAction GetAvailableDragActions();

  void SetWaylandDragContext(nsWaylandDragContext* aDragContext);
  nsWaylandDragContext* GetWaylandDragContext();

  virtual ~WaylandDataOffer();

 private:
  bool RequestDataTransfer(const char* aMimeType, int fd) override;

  wl_data_offer* mWaylandDataOffer;
  RefPtr<nsWaylandDragContext> mDragContext;
  uint32_t mSelectedDragAction;
  uint32_t mAvailableDragActions;
};

class PrimaryDataOffer : public DataOffer {
 public:
  explicit PrimaryDataOffer(gtk_primary_selection_offer* aPrimaryDataOffer);
  explicit PrimaryDataOffer(zwp_primary_selection_offer_v1* aPrimaryDataOffer);
  void SetAvailableDragActions(uint32_t aWaylandActions){};

  virtual ~PrimaryDataOffer();

 private:
  bool RequestDataTransfer(const char* aMimeType, int fd) override;

  gtk_primary_selection_offer* mPrimaryDataOfferGtk;
  zwp_primary_selection_offer_v1* mPrimaryDataOfferZwpV1;
};

class nsWaylandDragContext : public nsISupports {
  NS_DECL_ISUPPORTS

 public:
  nsWaylandDragContext(WaylandDataOffer* aWaylandDataOffer,
                       wl_display* aDisplay);

  void DropDataEnter(GtkWidget* aGtkWidget, uint32_t aTime, nscoord aX,
                     nscoord aY);
  void DropMotion(uint32_t aTime, nscoord aX, nscoord aY);
  void GetLastDropInfo(uint32_t* aTime, nscoord* aX, nscoord* aY);

  void SetDragStatus(GdkDragAction aPreferredAction);
  GdkDragAction GetAvailableDragActions();

  GtkWidget* GetWidget() { return mGtkWidget; }
  GList* GetTargets();
  char* GetData(const char* aMimeType, uint32_t* aContentLength);

 private:
  virtual ~nsWaylandDragContext() = default;

  mozilla::UniquePtr<WaylandDataOffer> mDataOffer;
  wl_display* mDisplay;
  uint32_t mTime;
  GtkWidget* mGtkWidget;
  nscoord mX, mY;
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

  void RegisterNewDataOffer(wl_data_offer* aWaylandDataOffer);
  void RegisterNewDataOffer(gtk_primary_selection_offer* aPrimaryDataOffer);
  void RegisterNewDataOffer(zwp_primary_selection_offer_v1* aPrimaryDataOffer);

  void SetClipboardDataOffer(wl_data_offer* aWaylandDataOffer);
  void SetPrimaryDataOffer(gtk_primary_selection_offer* aPrimaryDataOffer);
  void SetPrimaryDataOffer(zwp_primary_selection_offer_v1* aPrimaryDataOffer);
  void AddDragAndDropDataOffer(wl_data_offer* aWaylandDataOffer);
  nsWaylandDragContext* GetDragContext();

  void ClearDragAndDropDataOffer();

  void TransferFastTrackClipboard(ClipboardDataType aDataType,
                                  int aClipboardRequestNumber,
                                  GtkSelectionData* aSelectionData);

  virtual ~nsRetrievalContextWayland() override;

 private:
  bool mInitialized;
  RefPtr<mozilla::widget::nsWaylandDisplay> mDisplay;

  // Data offers provided by Wayland data device
  GHashTable* mActiveOffers;
  mozilla::UniquePtr<DataOffer> mClipboardOffer;
  mozilla::UniquePtr<DataOffer> mPrimaryOffer;
  RefPtr<nsWaylandDragContext> mDragContext;

  int mClipboardRequestNumber;
  char* mClipboardData;
  uint32_t mClipboardDataLength;

// Mime types used for text data at Gtk+, see request_text_received_func()
// at gtkclipboard.c.
#define TEXT_MIME_TYPES_NUM 3
  static const char* sTextMimeTypes[TEXT_MIME_TYPES_NUM];
};

#endif /* __nsClipboardWayland_h_ */
