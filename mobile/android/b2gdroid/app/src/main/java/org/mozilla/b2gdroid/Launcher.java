/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.b2gdroid;

import java.util.Date;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.PixelFormat;
import android.location.Location;
import android.location.LocationListener;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.util.Log;

import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;

import org.json.JSONObject;
import org.json.JSONException;

import org.mozilla.gecko.BaseGeckoInterface;
import org.mozilla.gecko.ContactService;
import org.mozilla.gecko.ContextGetter;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoBatteryManager;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.IntentHelper;
import org.mozilla.gecko.AppNotificationClient;
import org.mozilla.gecko.updater.UpdateServiceHelper;
import org.mozilla.gecko.util.GeckoEventListener;

import org.mozilla.b2gdroid.ScreenStateObserver;
import org.mozilla.b2gdroid.Apps;
import org.mozilla.b2gdroid.SettingsMapper;

public class Launcher extends FragmentActivity
                      implements GeckoEventListener, ContextGetter {
    private static final String LOGTAG = "B2G";

    private ContactService        mContactService;
    private ScreenStateObserver   mScreenStateObserver;
    private Apps                  mApps;
    private SettingsMapper        mSettings;
    private GeckoEventReceiver    mGeckoEventReceiver;
    private RemoteGeckoEventProxy mGeckoEventProxy;

    private View mDisableStatusBarView = null;

    private static final long   kHomeRepeat = 2;
    private static final long   kHomeDelay  = 500; // delay in ms to tap kHomeRepeat times.
    private long                mFirstHome;
    private long                mLastHome;
    private long                mHomeCount;

    final class GeckoInterface extends BaseGeckoInterface
                               implements LocationListener {
        public GeckoInterface(Context context) {
            super(context);
        }

        public LocationListener getLocationListener() {
            return this;
        }

        @Override
        public void onLocationChanged(Location location) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createLocationEvent(location));
        }

        @Override
        public void onProviderDisabled(String provider) {
        }

        @Override
        public void onProviderEnabled(String provider) {
        }

        @Override
        public void onStatusChanged(String provider, int status, Bundle extras) {
        }
    }

    /** ContextGetter */
    public Context getContext() {
        return this;
    }

    public SharedPreferences getSharedPreferences() {
        return null;
    }

    /** Initializes Gecko APIs */
    private void initGecko() {
        GeckoAppShell.setContextGetter(this);
        GeckoAppShell.setNotificationClient(new AppNotificationClient(this));

        GeckoBatteryManager.getInstance().start(this);
        mContactService = new ContactService(EventDispatcher.getInstance(), this);
        mApps = new Apps(this);
        mSettings = new SettingsMapper(this, null);
    }

    private void hideSplashScreen() {
        final View splash = findViewById(R.id.splashscreen);
        runOnUiThread(new Runnable() {
            @Override public void run() {
                splash.setVisibility(View.GONE);
            }
        });
    }

    private View getStatusBarOverlay() {
        if (mDisableStatusBarView != null) {
            return mDisableStatusBarView;
        }

        mDisableStatusBarView = new View(this);

        mDisableStatusBarView.setOnTouchListener(new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent ev) {
                // Pass the touch event down to the GeckoView
                Launcher.this.findViewById(R.id.gecko_view).dispatchTouchEvent(ev);
                return true;
            }
        });

        WindowManager.LayoutParams handleParams = new WindowManager.LayoutParams(
                // Fill the width of the screen
                WindowManager.LayoutParams.FILL_PARENT,
                // Arbitrary value to cover the edge of the top of the screen to interrupt the gesture.
                // This value was found through trial and error on a large screen L and a Z3C on KK
                25,
                // display over everything
                WindowManager.LayoutParams.TYPE_SYSTEM_ERROR,
                // Prevent events from capturing in other views beneath this
                // one
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
                // Allows the View to receive touch events so we can pass them
                // to Gecko
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL |
                // Draw over status bar area
                WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                // Draw transparent
                PixelFormat.TRANSPARENT);

        handleParams.gravity = Gravity.TOP;
        WindowManager manager = ((WindowManager) getApplicationContext().getSystemService(Context.WINDOW_SERVICE));
        manager.addView(mDisableStatusBarView, handleParams);
        return mDisableStatusBarView;
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.w(LOGTAG, "onCreate");
        super.onCreate(savedInstanceState);

        IntentHelper.init(this);
        mScreenStateObserver = new ScreenStateObserver(this);

        initGecko();

        mGeckoEventProxy = new RemoteGeckoEventProxy(this);

        mGeckoEventReceiver = new GeckoEventReceiver();
        mGeckoEventReceiver.registerWithContext(this);

        GeckoAppShell.setGeckoInterface(new GeckoInterface(this));

        UpdateServiceHelper.registerForUpdates(this);

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Launcher:Ready");

        // Register the RemoteGeckoEventProxy with the Notification Opened
        // event, Notifications are handled in a different process as a
        // service, so we need to forward them to the remote service
        EventDispatcher.getInstance().registerGeckoThreadListener(mGeckoEventProxy,
            "Android:NotificationOpened");

        // Initialize status bar overlay
        getStatusBarOverlay();

        setContentView(R.layout.launcher);

        mHomeCount = 0;
        mFirstHome = 0;
        mLastHome = 0;
    }

    @Override
    public void onResume() {
        super.onResume();
        if (GeckoThread.isRunning()) {
            hideSplashScreen();
            NotificationObserver.registerForNativeNotifications(this);
        }
    }

    @Override
    public void onDestroy() {
        Log.w(LOGTAG, "onDestroy");
        super.onDestroy();
        IntentHelper.destroy();

        mGeckoEventReceiver.destroy(this);
        mGeckoEventReceiver = null;

        mScreenStateObserver.destroy(this);
        mScreenStateObserver = null;

        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "Launcher:Ready");

        mContactService.destroy();
        mApps.destroy();
        mSettings.destroy();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        final String action = intent.getAction();
        Log.w(LOGTAG, "onNewIntent " + action);
        if (Intent.ACTION_VIEW.equals(action)) {
            Log.w(LOGTAG, "Asking gecko to view " + intent.getDataString());
            JSONObject obj = new JSONObject();
            try {
                obj.put("action", "view");
                obj.put("url", intent.getDataString());
            } catch(JSONException ex) {
                Log.wtf(LOGTAG, "Error building Android:Launcher view message", ex);
            }
            GeckoEvent e = GeckoEvent.createBroadcastEvent("Android:Launcher", obj.toString());
            GeckoAppShell.sendEventToGecko(e);
        } else if (Intent.ACTION_MAIN.equals(action)) {
            String message = "home-key";

            // Check if we did a multiple home tap to trigger the task switcher.
            long now = (new Date()).getTime();
            if (now - mLastHome > kHomeDelay) {
                mHomeCount = 0;
            }
            if (mHomeCount == 0) {
                mFirstHome = now;
            }
            mHomeCount++;
            if (mHomeCount == kHomeRepeat) {
                mHomeCount = 0;
                if (now - mFirstHome < kHomeDelay) {
                    message = "task-switcher";
                }
            }
            mLastHome = now;

            Log.d(LOGTAG, "Let's dispatch a '" + message + "' key event");
            JSONObject obj = new JSONObject();
            try {
                obj.put("action", message);
            } catch(JSONException ex) {
                Log.wtf(LOGTAG, "Error building Android:Launcher message", ex);
            }
            GeckoEvent e = GeckoEvent.createBroadcastEvent("Android:Launcher", obj.toString());
            GeckoAppShell.sendEventToGecko(e);
        } else if (Intent.ACTION_SENDTO.equals(action) ||
                   Intent.ACTION_SEND.equals(action)) {
            Log.d(LOGTAG, "Sending new SMS intent to Gecko");
            JSONObject obj = new JSONObject();
            try {
                obj.put("action", "send_sms");
                if (Intent.ACTION_SENDTO.equals(action)) {
                    obj.put("number", intent.getData().getPath());
                }
                obj.put("body", intent.getStringExtra("sms_body"));
            } catch (JSONException ex) {
                Log.wtf(LOGTAG, "Error building Android:Launcher message", ex);
            }
            GeckoEvent e = GeckoEvent.createBroadcastEvent("Android:Launcher", obj.toString());
            GeckoAppShell.sendEventToGecko(e);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        Log.d(LOGTAG, "onWindowFocusChanged hasFocus=" + hasFocus);

        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            getStatusBarOverlay().setVisibility(View.VISIBLE);
            findViewById(R.id.main_layout).setSystemUiVisibility(
                     View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    );
        } else {
            getStatusBarOverlay().setVisibility(View.INVISIBLE);
        }
    }

    @Override
    public void onBackPressed() {
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Android:Launcher", "{\"action\":\"back-key\"}");
        GeckoAppShell.sendEventToGecko(e);
    }

    public void handleMessage(String event, JSONObject message) {
        Log.w(LOGTAG, "Launcher received " + event);

        if ("Launcher:Ready".equals(event)) {
            hideSplashScreen();
        }
    }
}
