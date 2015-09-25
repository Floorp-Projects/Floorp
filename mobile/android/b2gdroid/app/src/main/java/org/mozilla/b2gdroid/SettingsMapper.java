/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.b2gdroid;

import java.util.Hashtable;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.WallpaperManager;
import android.content.Context;
import android.database.ContentObserver;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Handler;
import android.provider.Settings.System;
import android.util.Base64;
import android.util.Log;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.GeckoEventListener;

// This class communicates back and forth with MessagesBridge.jsm to
// map Android configuration settings and gaia settings.
// Each setting extends the base BaseMapping class to normalize values
// when needed.

class SettingsMapper extends ContentObserver implements GeckoEventListener {
    private static final String LOGTAG = "SettingsMapper";

    private Context mContext;
    private Hashtable<String, BaseMapping> mGeckoSettings;
    private Hashtable<String, BaseMapping> mAndroidSettings;

    abstract class BaseMapping {
        // Returns the list of gaia settings that are managed this class.
        abstract String[] getGeckoSettings();

        // Returns the list of android settings that are managed this class.
        abstract String[] getAndroidSettings();

        // Called when we a registered gecko setting changes.
        abstract void onGeckoChange(String setting, JSONObject message);

        // Called when we a registered android setting changes.
        abstract void onAndroidChange(Uri uri);

        void sendGeckoSetting(String name, String value) {
             JSONObject obj = new JSONObject();
             try {
                 obj.put(name, value);
                 sendGeckoSetting(obj);
             } catch(JSONException e) {
                Log.d(LOGTAG, e.toString());
             }
        }

        void sendGeckoSetting(String name, long value) {
             JSONObject obj = new JSONObject();
             try {
                 obj.put(name, value);
                 sendGeckoSetting(obj);
             } catch(JSONException e) {
                Log.d(LOGTAG, e.toString());
             }
        }

        void sendGeckoSetting(JSONObject obj) {
            GeckoEvent e = GeckoEvent.createBroadcastEvent("Android:Setting", obj.toString());
            GeckoAppShell.sendEventToGecko(e);
        }
    }

    class ScreenTimeoutMapping extends BaseMapping {
        ScreenTimeoutMapping() {}

        String[] getGeckoSettings() {
            String props[] = {"screen.timeout"};
            return props;
        }

        String[] getAndroidSettings() {
            String props[] = {"content://settings/system/screen_off_timeout"};
            return props;
        }

        void onGeckoChange(String setting, JSONObject message) {
            try {
                int timeout = message.getInt("value");
                // b2g uses seconds for the timeout while Android expects ms.
                // "never" is 0 in b2g, -1 in Android.
                if (timeout == 0) {
                    timeout = -1;
                } else {
                    timeout *= 1000;
                }
                System.putInt(mContext.getContentResolver(),
                              System.SCREEN_OFF_TIMEOUT,
                              timeout);
            } catch(Exception ex) {
                Log.d(LOGTAG, "Error setting screen.timeout value", ex);
            }
        }

        void onAndroidChange(Uri uri) {
            try {
                int timeout = System.getInt(mContext.getContentResolver(),
                                            System.SCREEN_OFF_TIMEOUT);
                Log.d(LOGTAG, "Android set timeout to " + timeout);

                // Convert to a gaia timeout.
                timeout /= 1000;
                sendGeckoSetting("screen.timeout", timeout);
            } catch(Exception e) {}
        }
    }

    class WallpaperMapping extends BaseMapping {
        private Context mContext;

        WallpaperMapping(Context context) {
            mContext = context;
        }

        String[] getGeckoSettings() {
            String props[] = {"wallpaper.image"};
            return props;
        }

        String[] getAndroidSettings() {
            String props[] = {};
            return props;
        }

        void onGeckoChange(String setting, JSONObject message) {
            try {
                final String url = message.getString("value");
                Log.d(LOGTAG, "wallpaper.image is now " + url);
                WallpaperManager manager = WallpaperManager.getInstance(mContext);
                // Remove the data:image/png;base64, prefix from the url.
                byte[] raw = Base64.decode(url.substring(22), Base64.NO_WRAP);
                Bitmap bitmap = BitmapFactory.decodeByteArray(raw, 0, raw.length);
                if (bitmap == null) {
                    Log.d(LOGTAG, "Unable to create a bitmap!");
                }
                manager.setBitmap(bitmap);
            } catch(Exception ex) {
                Log.d(LOGTAG, "Error setting wallpaper", ex);
            }
        }

        // Android doesn't notify on wallpaper changes.
        void onAndroidChange(Uri uri) { }
    }

    SettingsMapper(Context context, Handler handler) {
        super(handler);
        mContext = context;
        EventDispatcher.getInstance()
                       .registerGeckoThreadListener(this,
                                                    "Settings:Change");

        mContext.getContentResolver()
                .registerContentObserver(System.CONTENT_URI,
                                         true,
                                         this);

        mGeckoSettings = new Hashtable<String, BaseMapping>();
        mAndroidSettings = new Hashtable<String, BaseMapping>();

        // Add all the mappings.
        addMapping(new ScreenTimeoutMapping());
        addMapping(new WallpaperMapping(mContext));
    }

    void addMapping(BaseMapping mapping) {
        String[] props = mapping.getGeckoSettings();
        for (int i = 0; i < props.length; i++) {
            mGeckoSettings.put(props[i], mapping);
        }

        props = mapping.getAndroidSettings();
        for (int i = 0; i < props.length; i++) {
            mAndroidSettings.put(props[i], mapping);
        }
    }

    void destroy() {
        EventDispatcher.getInstance()
                       .unregisterGeckoThreadListener(this,
                                                      "Settings:Change");
        mGeckoSettings.clear();
        mGeckoSettings = null;
        mAndroidSettings.clear();
        mAndroidSettings = null;
        mContext.getContentResolver().unregisterContentObserver(this);
    }

    public void handleMessage(String event, JSONObject message) {
        Log.w(LOGTAG, "Received " + event);

        try {
            String setting = message.getString("setting");
            BaseMapping mapping = mGeckoSettings.get(setting);
            if (mapping != null) {
                Log.d(LOGTAG, "Changing gecko setting " + setting);
                mapping.onGeckoChange(setting, message);
            } else {
                Log.d(LOGTAG, "No gecko mapping registered for " + setting);
            }
        } catch(Exception ex) {
            Log.d(LOGTAG, "Error getting setting name", ex);
        }
    }

    // ContentObserver, see
    // http://developer.android.com/reference/android/database/ContentObserver.html
    @Override
    public boolean deliverSelfNotifications() {
        return false;
    }

    @Override
    public void onChange(boolean selfChange) {
        onChange(selfChange, null);
    }

    @Override
    public void onChange(boolean selfChange, Uri uri) {
        super.onChange(selfChange);
        Log.d(LOGTAG, "Settings change detected uri=" + uri);
        BaseMapping mapping = mAndroidSettings.get(uri.toString());
        if (mapping != null) {
            Log.d(LOGTAG, "Changing android setting " + uri);
            mapping.onAndroidChange(uri);
        } else {
            Log.d(LOGTAG, "No android mapping registered for " + uri);
        }
    }

}
