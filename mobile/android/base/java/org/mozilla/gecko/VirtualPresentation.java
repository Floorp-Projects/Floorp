/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoView;

import com.google.android.gms.cast.CastPresentation;

import android.content.Context;
import android.os.Bundle;
import android.view.Display;
import android.view.ViewGroup.LayoutParams;
import android.widget.RelativeLayout;


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
