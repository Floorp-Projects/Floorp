/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONObject;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class GeckoView extends LayerView
    implements GeckoEventListener, ContextGetter {

    private static final String LOGTAG = "GeckoView";

    public GeckoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.GeckoView);
        String url = a.getString(R.styleable.GeckoView_url);
        boolean doInit = a.getBoolean(R.styleable.GeckoView_doinit, true);
        a.recycle();

        if (!doInit)
            return;

        // If running outside of a GeckoActivity (eg, from a library project),
        // load the native code and disable content providers
        if (!(context instanceof GeckoActivity)) {
            // Set the GeckoInterface if the context is an activity and the GeckoInterface
            // has not already been set
            if (context instanceof Activity && getGeckoInterface() == null) {
                setGeckoInterface(new BaseGeckoInterface(context));
            }

            Clipboard.init(context);
            GeckoLoader.loadMozGlue();
            BrowserDB.setEnableContentProviders(false);
        }

        if (url != null) {
            GeckoThread.setUri(url);
            GeckoThread.setAction(Intent.ACTION_VIEW);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(url));
        }
        GeckoAppShell.setContextGetter(this);
        if (context instanceof Activity) {
            Tabs tabs = Tabs.getInstance();
            tabs.attachToContext(context);
        }
        GeckoAppShell.registerEventListener("Gecko:Ready", this);

        ThreadUtils.setUiThread(Thread.currentThread(), new Handler());
        initializeView(GeckoAppShell.getEventDispatcher());

        GeckoProfile profile = GeckoProfile.get(context);
        BrowserDB.initialize(profile.getName());

        if (GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.Launching, GeckoThread.LaunchState.Launched)) {
            GeckoAppShell.setLayerView(this);
            GeckoThread.createAndStart();
        }
    }

    public void loadUrl(String uri) {
        Tabs.getInstance().loadUrl(uri);
    }

    public void loadUrlInNewTab(String uri) {
        Tabs.getInstance().loadUrl(uri, Tabs.LOADURL_NEW_TAB);
     }

    public void handleMessage(String event, JSONObject message) {
        if (event.equals("Gecko:Ready")) {
            GeckoThread.setLaunchState(GeckoThread.LaunchState.GeckoRunning);
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab != null)
                Tabs.getInstance().notifyListeners(selectedTab, Tabs.TabEvents.SELECTED);
            geckoConnected();
            GeckoAppShell.setLayerClient(getLayerClient());
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Viewport:Flush", null));
            show();
            requestRender();
        }
    }

    public static void setGeckoInterface(final BaseGeckoInterface geckoInterface) {
        GeckoAppShell.setGeckoInterface(geckoInterface);
    }

    public static GeckoAppShell.GeckoInterface getGeckoInterface() {
        return GeckoAppShell.getGeckoInterface();
    }
}
