/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.javaaddons;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import dalvik.system.DexClassLoader;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

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
 * The Handler.Callback instances provided (as described above) are invoked with
 * Message objects when the corresponding events are dispatched. The Bundle
 * object attached to the Message will contain the "primitive" values from the
 * JSON of the event. ("primitive" includes bool/int/long/double/String). If
 * the addon callback wishes to synchronously return a value back to the event
 * dispatcher, they can do so by inserting the response string into the bundle
 * under the key "response".
 */
public class JavaAddonManager implements BundleEventListener {
    private static final String LOGTAG = "GeckoJavaAddonManager";

    private static JavaAddonManager sInstance;

    private final EventDispatcher mDispatcher;
    private final Map<String, Map<String, BundleEventListener>> mAddonCallbacks;

    private Context mApplicationContext;

    public static JavaAddonManager getInstance() {
        if (sInstance == null) {
            sInstance = new JavaAddonManager();
        }
        return sInstance;
    }

    private JavaAddonManager() {
        mDispatcher = EventDispatcher.getInstance();
        mAddonCallbacks = new HashMap<>();
    }

    public void init(Context applicationContext) {
        if (mApplicationContext != null) {
            // we've already done this registration. don't do it again
            return;
        }
        mApplicationContext = applicationContext;
        mDispatcher.registerGeckoThreadListener(this,
            "Dex:Load",
            "Dex:Unload");
        JavaAddonManagerV1.getInstance().init(applicationContext);
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("Dex:Load".equals(event)) {
            final String zipFile = message.getString("zipfile");
            final String implClass = message.getString("impl");
            Log.d(LOGTAG, "Attempting to load classes.dex file from " + zipFile +
                          " and instantiate " + implClass);
            try {
                final File tmpDir = mApplicationContext.getDir("dex", 0);
                final DexClassLoader loader = new DexClassLoader(
                        zipFile, tmpDir.getAbsolutePath(),
                        null, mApplicationContext.getClassLoader());
                final Class<?> c = loader.loadClass(implClass);
                try {
                    final Constructor<?> constructor = c.getDeclaredConstructor(Map.class);
                    final Map<String, Handler.Callback> callbacks =
                            new HashMap<String, Handler.Callback>();
                    constructor.newInstance(callbacks);
                    registerCallbacks(zipFile, callbacks);
                } catch (final NoSuchMethodException nsme) {
                    Log.d(LOGTAG, "Did not find constructor with parameters " +
                                  "Map<String, Handler.Callback>. Falling back " +
                                  "to default constructor...");
                    // fallback for instances with no constructor that takes a Map<String,
                    // Handler.Callback>
                    c.newInstance();
                }
            } catch (final Exception e) {
                Log.e(LOGTAG, "Unable to load dex successfully", e);
            }

        } else if ("Dex:Unload".equals(event)) {
            final String zipFile = message.getString("zipfile");
            unregisterCallbacks(zipFile);
        }
    }

    private void registerCallbacks(String zipFile, Map<String, Handler.Callback> callbacks) {
        Map<String, BundleEventListener> addonCallbacks = mAddonCallbacks.get(zipFile);
        if (addonCallbacks != null) {
            Log.w(LOGTAG, "Found pre-existing callbacks for zipfile [" + zipFile + "]; aborting re-registration!");
            return;
        }
        addonCallbacks = new HashMap<>();
        for (String event : callbacks.keySet()) {
            CallbackWrapper wrapper = new CallbackWrapper(callbacks.get(event));
            mDispatcher.registerGeckoThreadListener(wrapper, event);
            addonCallbacks.put(event, wrapper);
        }
        mAddonCallbacks.put(zipFile, addonCallbacks);
    }

    private void unregisterCallbacks(String zipFile) {
        Map<String, BundleEventListener> callbacks = mAddonCallbacks.remove(zipFile);
        if (callbacks == null) {
            Log.w(LOGTAG, "Attempting to unregister callbacks from zipfile [" + zipFile +
                          "] which has no callbacks registered.");
            return;
        }
        for (String event : callbacks.keySet()) {
            mDispatcher.unregisterGeckoThreadListener(callbacks.get(event), event);
        }
    }

    private static class CallbackWrapper implements BundleEventListener {
        private final Handler.Callback mDelegate;

        CallbackWrapper(Handler.Callback delegate) {
            mDelegate = delegate;
        }

        @Override // BundleEventListener
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            final Message msg = new Message();
            final Bundle data = message.toBundle();
            data.putString("type", event);
            msg.setData(data);
            mDelegate.handleMessage(msg);

            final GeckoBundle response = new GeckoBundle(1);
            response.putString("response", data.getString("response"));
            callback.sendSuccess(response);
        }
    }
}
