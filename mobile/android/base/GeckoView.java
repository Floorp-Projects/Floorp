/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.LayerView;
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
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.os.Handler;

public class GeckoView extends LayerView
    implements GeckoEventListener, ContextGetter {
    static GeckoThread sGeckoThread;

    public GeckoView(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.GeckoView);
        String url = a.getString(R.styleable.GeckoView_url);
        a.recycle();

        Intent intent;
        if (url == null) {
            intent = new Intent(Intent.ACTION_MAIN);
        } else {
            intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(url));
        }
        GeckoAppShell.setContextGetter(this);
        if (context instanceof Activity) {
            Tabs tabs = Tabs.getInstance();
            tabs.attachToActivity((Activity) context);
        }
        GeckoProfile profile = GeckoProfile.get(context);
        BrowserDB.initialize(profile.getName());
        GeckoAppShell.registerEventListener("Gecko:Ready", this);

        sGeckoThread = new GeckoThread(intent, url);
        ThreadUtils.setUiThread(Thread.currentThread(), new Handler());
        initializeView(GeckoAppShell.getEventDispatcher());
        if (GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.Launching, GeckoThread.LaunchState.Launched)) {
            GeckoAppShell.setLayerView(this);
            sGeckoThread.start();
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus) {
            setBackgroundDrawable(null);
        }
    }

    public void loadUrl(String uri) {
        Tabs.getInstance().loadUrl(uri);
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

    public static void setGeckoInterface(GeckoAppShell.GeckoInterface aGeckoInterface) {
        GeckoAppShell.setGeckoInterface(aGeckoInterface);
    }
}
