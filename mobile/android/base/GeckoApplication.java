/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AdjustConstants;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.home.HomePanelsManager;
import org.mozilla.gecko.lwt.LightweightTheme;
import org.mozilla.gecko.mdns.MulticastDNSManager;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.util.Log;

import java.io.File;

public class GeckoApplication extends Application 
    implements ContextGetter {
    private static final String LOG_TAG = "GeckoApplication";

    private static volatile GeckoApplication instance;

    private boolean mInBackground;
    private boolean mPausedGecko;

    private LightweightTheme mLightweightTheme;

    public GeckoApplication() {
        super();
        instance = this;
    }

    public static GeckoApplication get() {
        return instance;
    }

    @Override
    public Context getContext() {
        return this;
    }

    @Override
    public SharedPreferences getSharedPreferences() {
        return GeckoSharedPrefs.forApp(this);
    }

    /**
     * We need to do locale work here, because we need to intercept
     * each hit to onConfigurationChanged.
     */
    @Override
    public void onConfigurationChanged(Configuration config) {
        Log.d(LOG_TAG, "onConfigurationChanged: " + config.locale +
                       ", background: " + mInBackground);

        // Do nothing if we're in the background. It'll simply cause a loop
        // (Bug 936756 Comment 11), and it's not necessary.
        if (mInBackground) {
            super.onConfigurationChanged(config);
            return;
        }

        // Otherwise, correct the locale. This catches some cases that GeckoApp
        // doesn't get a chance to.
        try {
            BrowserLocaleManager.getInstance().correctLocale(this, getResources(), config);
        } catch (IllegalStateException ex) {
            // GeckoApp hasn't started, so we have no ContextGetter in BrowserLocaleManager.
            Log.w(LOG_TAG, "Couldn't correct locale.", ex);
        }

        super.onConfigurationChanged(config);
    }

    public void onActivityPause(GeckoActivityStatus activity) {
        mInBackground = true;

        if ((activity.isFinishing() == false) &&
            (activity.isGeckoActivityOpened() == false)) {
            // Notify Gecko that we are pausing; the cache service will be
            // shutdown, closing the disk cache cleanly. If the android
            // low memory killer subsequently kills us, the disk cache will
            // be left in a consistent state, avoiding costly cleanup and
            // re-creation. 
            GeckoAppShell.sendEventToGecko(GeckoEvent.createAppBackgroundingEvent());
            mPausedGecko = true;

            final BrowserDB db = GeckoProfile.get(this).getDB();
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    db.expireHistory(getContentResolver(), BrowserContract.ExpirePriority.NORMAL);
                }
            });
        }
        GeckoConnectivityReceiver.getInstance().stop();
        GeckoNetworkManager.getInstance().stop();
    }

    public void onActivityResume(GeckoActivityStatus activity) {
        if (mPausedGecko) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createAppForegroundingEvent());
            mPausedGecko = false;
        }

        final Context applicationContext = getApplicationContext();
        GeckoBatteryManager.getInstance().start(applicationContext);
        GeckoConnectivityReceiver.getInstance().start(applicationContext);
        GeckoNetworkManager.getInstance().start(applicationContext);

        mInBackground = false;
    }

    @Override
    public void onCreate() {
        final Context context = getApplicationContext();
        HardwareUtils.init(context);
        Clipboard.init(context);
        FilePicker.init(context);
        DownloadsIntegration.init();
        HomePanelsManager.getInstance().init(context);

        // This getInstance call will force initialization of the NotificationHelper, but does nothing with the result
        NotificationHelper.getInstance(context).init();

        MulticastDNSManager.getInstance(context).init();

        // Make sure that all browser-ish applications default to the real LocalBrowserDB.
        // GeckoView consumers use their own Application class, so this doesn't affect them.
        // WebappImpl overrides this on creation.
        //
        // We need to do this before any access to the profile; it controls
        // which database class is used.
        //
        // As such, this needs to occur before the GeckoView in GeckoApp is inflated -- i.e., in the
        // GeckoApp constructor or earlier -- because GeckoView implicitly accesses the profile. This is earlier!
        GeckoProfile.setBrowserDBFactory(new BrowserDB.Factory() {
            @Override
            public BrowserDB get(String profileName, File profileDir) {
                // Note that we don't use the profile directory -- we
                // send operations to the ContentProvider, which does
                // its own thing.
                return new LocalBrowserDB(profileName);
            }
        });

        super.onCreate();

        if (AppConstants.MOZ_INSTALL_TRACKING) {
            AdjustConstants.getAdjustHelper().onCreate(this, AdjustConstants.MOZ_INSTALL_TRACKING_ADJUST_SDK_APP_TOKEN);
        }
    }

    public boolean isApplicationInBackground() {
        return mInBackground;
    }

    public LightweightTheme getLightweightTheme() {
        return mLightweightTheme;
    }

    public void prepareLightweightTheme() {
        mLightweightTheme = new LightweightTheme(this);
    }
}
