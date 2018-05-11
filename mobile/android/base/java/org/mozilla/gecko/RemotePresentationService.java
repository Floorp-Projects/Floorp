/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.ScreenManagerHelper;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoView;

import com.google.android.gms.cast.CastMediaControlIntent;
import com.google.android.gms.cast.CastPresentation;
import com.google.android.gms.cast.CastRemoteDisplayLocalService;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesUtil;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v7.media.MediaControlIntent;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter.RouteInfo;
import android.support.v7.media.MediaRouter;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.widget.RelativeLayout;

import java.util.HashMap;
import java.util.Map;

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

class VirtualPresentation extends CastPresentation {
    private static final String LOGTAG = "VirtualPresentation";
    private static final String PRESENTATION_VIEW_URI = "chrome://browser/content/PresentationView.xul";

    private RelativeLayout layout;
    private GeckoView view;
    private String deviceId;
    private int screenId;

    public VirtualPresentation(Context context, Display display) {
        super(context, display);
    }

    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }
    public void setScreenId(int screenId) { this.screenId = screenId; }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        /*
         * NOTICE: The context get from getContext() is different to the context
         * of the application. Presentaion has its own context to get correct
         * resources.
         */

        final GeckoSession session = new GeckoSession();
        session.getSettings().setString(GeckoSessionSettings.CHROME_URI,
                                        PRESENTATION_VIEW_URI + "#" + deviceId);
        session.getSettings().setInt(GeckoSessionSettings.SCREEN_ID, screenId);

        // Create new GeckoView
        view = new GeckoView(getContext());
        view.setSession(session, GeckoApplication.ensureRuntime(getContext()));
        view.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT,
                                              LayoutParams.MATCH_PARENT));

        // Create new layout to put the GeckoView
        layout = new RelativeLayout(getContext());
        layout.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT,
                                                LayoutParams.MATCH_PARENT));
        layout.addView(view);

        setContentView(layout);
    }
}
