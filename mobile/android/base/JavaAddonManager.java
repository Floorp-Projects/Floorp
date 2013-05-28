/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.GeckoEventResponder;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import dalvik.system.DexClassLoader;

import java.io.File;
import java.lang.reflect.Constructor;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * The manager for addon-provided Java code.
 *
 * Java code in addons can be loaded using the Dex:Load message, and unloaded
 * via the Dex:Unload message. Addon classes loaded are checked for a constructor
 * that takes a Map&lt;String, Handler.Callback&gt;. If such a constructor
 * exists, it is called and the objects populated into the map by the constructor
 * are registered as event listeners. If no such constructor exists, the default
 * constructor is invoked instead.
 *
 * Note: The Map and Handler.Callback classes were used in this API definition
 * rather than defining a custom class. This was done explicitly so that the
 * addon code can be compiled against the android.jar provided in the Android
 * SDK, rather than having to be compiled against Fennec source code.
 *
 * The Handler.Callback instances provided (as described above) are inovked with
 * Message objects when the corresponding events are dispatched. The Bundle
 * object attached to the Message will contain the "primitive" values from the
 * JSON of the event. ("primitive" includes bool/int/long/double/String). If
 * the addon callback wishes to synchronously return a value back to the event
 * dispatcher, they can do so by inserting the response string into the bundle
 * under the key "response".
 */
class JavaAddonManager implements GeckoEventListener {
    private static final String LOGTAG = "GeckoJavaAddonManager";

    private static JavaAddonManager sInstance;

    private final EventDispatcher mDispatcher;
    private final Map<String, Map<String, GeckoEventListener>> mAddonCallbacks;

    private Context mApplicationContext;

    public static JavaAddonManager getInstance() {
        if (sInstance == null) {
            sInstance = new JavaAddonManager();
        }
        return sInstance;
    }

    private JavaAddonManager() {
        mDispatcher = GeckoAppShell.getEventDispatcher();
        mAddonCallbacks = new HashMap<String, Map<String, GeckoEventListener>>();
    }

    void init(Context applicationContext) {
        if (mApplicationContext != null) {
            // we've already done this registration. don't do it again
            return;
        }
        mApplicationContext = applicationContext;
        mDispatcher.registerEventListener("Dex:Load", this);
        mDispatcher.registerEventListener("Dex:Unload", this);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Dex:Load")) {
                String zipFile = message.getString("zipfile");
                String implClass = message.getString("impl");
                Log.d(LOGTAG, "Attempting to load classes.dex file from " + zipFile + " and instantiate " + implClass);
                try {
                    File tmpDir = mApplicationContext.getDir("dex", 0);
                    DexClassLoader loader = new DexClassLoader(zipFile, tmpDir.getAbsolutePath(), null, ClassLoader.getSystemClassLoader());
                    Class<?> c = loader.loadClass(implClass);
                    try {
                        Constructor<?> constructor = c.getDeclaredConstructor(Map.class);
                        Map<String, Handler.Callback> callbacks = new HashMap<String, Handler.Callback>();
                        constructor.newInstance(callbacks);
                        registerCallbacks(zipFile, callbacks);
                    } catch (NoSuchMethodException nsme) {
                        Log.d(LOGTAG, "Did not find constructor with parameters Map<String, Handler.Callback>. Falling back to default constructor...");
                        // fallback for instances with no constructor that takes a Map<String, Handler.Callback>
                        c.newInstance();
                    }
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unable to load dex successfully", e);
                }
            } else if (event.equals("Dex:Unload")) {
                String zipFile = message.getString("zipfile");
                unregisterCallbacks(zipFile);
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Exception handling message [" + event + "]:", e);
        }
    }

    private void registerCallbacks(String zipFile, Map<String, Handler.Callback> callbacks) {
        Map<String, GeckoEventListener> addonCallbacks = mAddonCallbacks.get(zipFile);
        if (addonCallbacks != null) {
            Log.w(LOGTAG, "Found pre-existing callbacks for zipfile [" + zipFile + "]; aborting re-registration!");
            return;
        }
        addonCallbacks = new HashMap<String, GeckoEventListener>();
        for (String event : callbacks.keySet()) {
            CallbackWrapper wrapper = new CallbackWrapper(callbacks.get(event));
            mDispatcher.registerEventListener(event, wrapper);
            addonCallbacks.put(event, wrapper);
        }
        mAddonCallbacks.put(zipFile, addonCallbacks);
    }

    private void unregisterCallbacks(String zipFile) {
        Map<String, GeckoEventListener> callbacks = mAddonCallbacks.remove(zipFile);
        if (callbacks == null) {
            Log.w(LOGTAG, "Attempting to unregister callbacks from zipfile [" + zipFile + "] which has no callbacks registered.");
            return;
        }
        for (String event : callbacks.keySet()) {
            mDispatcher.unregisterEventListener(event, callbacks.get(event));
        }
    }

    private static class CallbackWrapper implements GeckoEventResponder {
        private final Handler.Callback mDelegate;
        private Bundle mBundle;

        CallbackWrapper(Handler.Callback delegate) {
            mDelegate = delegate;
        }

        private Bundle jsonToBundle(JSONObject json) {
            // XXX right now we only support primitive types;
            // we don't recurse down into JSONArray or JSONObject instances
            Bundle b = new Bundle();
            for (Iterator<?> keys = json.keys(); keys.hasNext(); ) {
                try {
                    String key = (String)keys.next();
                    Object value = json.get(key);
                    if (value instanceof Integer) {
                        b.putInt(key, (Integer)value);
                    } else if (value instanceof String) {
                        b.putString(key, (String)value);
                    } else if (value instanceof Boolean) {
                        b.putBoolean(key, (Boolean)value);
                    } else if (value instanceof Long) {
                        b.putLong(key, (Long)value);
                    } else if (value instanceof Double) {
                        b.putDouble(key, (Double)value);
                    }
                } catch (JSONException e) {
                    Log.d(LOGTAG, "Error during JSON->bundle conversion", e);
                }
            }
            return b;
        }

        @Override
        public void handleMessage(String event, JSONObject json) {
            try {
                if (mBundle != null) {
                    Log.w(LOGTAG, "Event [" + event + "] handler is re-entrant; response messages may be lost");
                }
                mBundle = jsonToBundle(json);
                Message msg = new Message();
                msg.setData(mBundle);
                mDelegate.handleMessage(msg);
            } catch (Exception e) {
                Log.e(LOGTAG, "Caught exception thrown from wrapped addon message handler", e);
            }
        }

        @Override
        public String getResponse(JSONObject origMessage) {
            String response = mBundle.getString("response");
            mBundle = null;
            return response;
        }
    }
}
