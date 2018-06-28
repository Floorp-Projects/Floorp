/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import com.google.android.gms.cast.CastPresentation;
import com.google.android.gms.cast.CastRemoteDisplayLocalService;

import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

/*
 * Service to keep the remote display running even when the app goes into the background
 */
public class RemotePresentationService extends CastRemoteDisplayLocalService {

    private static final String LOGTAG = "RemotePresentationService";
    private CastPresentation presentation;
    private String deviceId;
    private int screenId;

    public void setDeviceId(String deviceId) {
        this.deviceId = deviceId;
    }

    public String getDeviceId() {
        return deviceId;
    }

    @Override
    public void onCreatePresentation(Display display) {
        createPresentation();
    }

    @Override
    public void onDismissPresentation() {
        dismissPresentation();
    }

    private void dismissPresentation() {
        if (presentation != null) {
            presentation.dismiss();
            presentation = null;
            ScreenManagerHelper.removeDisplay(screenId);
            MediaPlayerManager.getInstance().setPresentationMode(false);
        }
    }

    private void createPresentation() {
        dismissPresentation();

        MediaPlayerManager.getInstance().setPresentationMode(true);

        DisplayMetrics metrics = new DisplayMetrics();
        getDisplay().getMetrics(metrics);
        screenId = ScreenManagerHelper.addDisplay(ScreenManagerHelper.DISPLAY_VIRTUAL,
                                                  metrics.widthPixels,
                                                  metrics.heightPixels,
                                                  metrics.density);

        VirtualPresentation virtualPresentation = new VirtualPresentation(this, getDisplay());
        virtualPresentation.setDeviceId(deviceId);
        virtualPresentation.setScreenId(screenId);
        presentation = (CastPresentation) virtualPresentation;

        try {
            presentation.show();
        } catch (WindowManager.InvalidDisplayException ex) {
            Log.e(LOGTAG, "Unable to show presentation, display was removed.", ex);
            dismissPresentation();
        }
    }
}
