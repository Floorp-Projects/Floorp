/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardWayland_h_
#define __nsClipboardWayland_h_

#include "nsIClipboard.h"
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <nsTArray.h>

class nsRetrievalContextWayland : public nsRetrievalContext
{
public:
    nsRetrievalContextWayland();

    virtual const char* GetClipboardData(const char* aMimeType,
                                         int32_t aWhichClipboard,
                                         uint32_t* aContentLength) override;
    virtual void ReleaseClipboardData(const char* aClipboardData) override;

    virtual GdkAtom* GetTargets(int32_t aWhichClipboard,
                                int* aTargetNum) override;

    void SetDataOffer(wl_data_offer *aDataOffer);
    void AddMIMEType(const char *aMimeType);
    void ResetMIMETypeList(void);
    void ConfigureKeyboard(wl_seat_capability caps);

    void InitDataDeviceManager(wl_registry *registry, uint32_t id, uint32_t version);
    void InitSeat(wl_registry *registry, uint32_t id, uint32_t version, void *data);
    virtual ~nsRetrievalContextWayland() override;

private:
    bool                        mInitialized;
    wl_display                 *mDisplay;
    wl_seat                    *mSeat;
    wl_data_device_manager     *mDataDeviceManager;
    wl_data_offer              *mDataOffer;
    wl_keyboard                *mKeyboard;
    nsTArray<GdkAtom>           mTargetMIMETypes;
    gchar                      *mTextPlainLocale;
};

#endif /* __nsClipboardWayland_h_ */
