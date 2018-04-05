/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardWayland_h_
#define __nsClipboardWayland_h_

#include "nsIClipboard.h"
#include "wayland/gtk-primary-selection-client-protocol.h"

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <nsTArray.h>

struct FastTrackClipboard;

class DataOffer
{
public:
    void AddMIMEType(const char *aMimeType);

    GdkAtom* GetTargets(int* aTargetNum);
    bool  HasTarget(const char *aMimeType);

    char* GetData(wl_display* aDisplay, const char* aMimeType,
                  uint32_t* aContentLength);

    virtual ~DataOffer() {};
private:
    virtual bool RequestDataTransfer(const char* aMimeType, int fd) = 0;

    nsTArray<GdkAtom> mTargetMIMETypes;
};

class WaylandDataOffer : public DataOffer
{
public:
    WaylandDataOffer(wl_data_offer* aWaylandDataOffer);

private:
    virtual ~WaylandDataOffer();
    bool RequestDataTransfer(const char* aMimeType, int fd) override;

    wl_data_offer* mWaylandDataOffer;
};

class PrimaryDataOffer : public DataOffer
{
public:
    PrimaryDataOffer(gtk_primary_selection_offer* aPrimaryDataOffer);

private:
    virtual ~PrimaryDataOffer();
    bool RequestDataTransfer(const char* aMimeType, int fd) override;

    gtk_primary_selection_offer* mPrimaryDataOffer;
};

class nsRetrievalContextWayland : public nsRetrievalContext
{
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

    void RegisterDataOffer(wl_data_offer *aWaylandDataOffer);
    void RegisterDataOffer(gtk_primary_selection_offer *aPrimaryDataOffer);

    void SetClipboardDataOffer(wl_data_offer *aWaylandDataOffer);
    void SetPrimaryDataOffer(gtk_primary_selection_offer *aPrimaryDataOffer);

    void ClearDataOffers();

    void ConfigureKeyboard(wl_seat_capability caps);
    void TransferFastTrackClipboard(int aClipboardRequestNumber,
                                    GtkSelectionData *aSelectionData);

    void InitDataDeviceManager(wl_registry *registry, uint32_t id, uint32_t version);
    void InitPrimarySelectionDataDeviceManager(wl_registry *registry, uint32_t id);
    void InitSeat(wl_registry *registry, uint32_t id, uint32_t version, void *data);
    virtual ~nsRetrievalContextWayland() override;

private:
    bool                        mInitialized;
    wl_display                 *mDisplay;
    wl_seat                    *mSeat;
    wl_data_device_manager     *mDataDeviceManager;
    gtk_primary_selection_device_manager *mPrimarySelectionDataDeviceManager;
    wl_keyboard                *mKeyboard;

    // Data offers provided by Wayland data device
    GHashTable*                 mActiveOffers;
    nsAutoPtr<DataOffer>        mClipboardOffer;
    nsAutoPtr<DataOffer>        mPrimaryOffer;

    int                         mClipboardRequestNumber;
    char*                       mClipboardData;
    uint32_t                    mClipboardDataLength;

    // Mime types used for text data at Gtk+, see request_text_received_func()
    // at gtkclipboard.c.
    #define TEXT_MIME_TYPES_NUM 3
    static const char* sTextMimeTypes[TEXT_MIME_TYPES_NUM];
};

#endif /* __nsClipboardWayland_h_ */
