/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.javaaddons;

import android.content.Context;
import android.util.Log;
import android.util.Pair;
import dalvik.system.DexClassLoader;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.javaaddons.JavaAddonInterfaceV1;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.Map;

public class JavaAddonManagerV1 implements BundleEventListener {
    private static final String LOGTAG = "GeckoJavaAddonMgrV1";
    public static final String MESSAGE_LOAD = "JavaAddonManagerV1:Load";
    public static final String MESSAGE_UNLOAD = "JavaAddonManagerV1:Unload";

    private static JavaAddonManagerV1 sInstance;

    // Protected by static synchronized.
    private Context mApplicationContext;

    private final org.mozilla.gecko.EventDispatcher mDispatcher;

    // Protected by synchronized (this).
    private final Map<String, EventDispatcherImpl> mGUIDToDispatcherMap = new HashMap<>();

    public static synchronized JavaAddonManagerV1 getInstance() {
        if (sInstance == null) {
            sInstance = new JavaAddonManagerV1();
        }
        return sInstance;
    }

    private JavaAddonManagerV1() {
        mDispatcher = org.mozilla.gecko.EventDispatcher.getInstance();
    }

    public synchronized void init(Context applicationContext) {
        if (mApplicationContext != null) {
            // We've already registered; don't register again.
            return;
        }
        mApplicationContext = applicationContext;
        mDispatcher.registerGeckoThreadListener(this,
                MESSAGE_LOAD,
                MESSAGE_UNLOAD);
    }

    protected String getExtension(String filename) {
        if (filename == null) {
            return "";
        }
        final int last = filename.lastIndexOf(".");
        if (last < 0) {
            return "";
        }
        return filename.substring(last);
    }

    protected synchronized EventDispatcherImpl registerNewInstance(String classname, String filename)
            throws ClassNotFoundException, InstantiationException, IllegalAccessException, InvocationTargetException, NoSuchMethodException, IOException {
        Log.d(LOGTAG, "Attempting to instantiate " + classname + " from filename " + filename);

        // It's important to maintain the extension, either .dex, .apk, .jar.
        final String extension = getExtension(filename);
        final File dexFile = GeckoJarReader.extractStream(mApplicationContext, filename, mApplicationContext.getCacheDir(), "." + extension);
        try {
            if (dexFile == null) {
                throw new IOException("Could not find file " + filename);
            }
            final File tmpDir = mApplicationContext.getDir("dex", 0); // We'd prefer getCodeCacheDir but it's API 21+.
            final DexClassLoader loader = new DexClassLoader(dexFile.getAbsolutePath(), tmpDir.getAbsolutePath(), null, mApplicationContext.getClassLoader());
            final Class<?> c = loader.loadClass(classname);
            final Constructor<?> constructor = c.getDeclaredConstructor(Context.class, JavaAddonInterfaceV1.EventDispatcher.class);
            final String guid = Utils.generateGuid();
            final EventDispatcherImpl dispatcher = new EventDispatcherImpl(guid, filename);
            final Object instance = constructor.newInstance(mApplicationContext, dispatcher);
            mGUIDToDispatcherMap.put(guid, dispatcher);
            return dispatcher;
        } finally {
            // DexClassLoader writes an optimized version, so we can get rid of our temporary extracted version.
            if (dexFile != null) {
                dexFile.delete();
            }
        }
    }

    @Override // BundleEventListener
    public synchronized void handleMessage(final String event, final GeckoBundle message,
                                           final EventCallback callback) {
        switch (event) {
            case MESSAGE_LOAD: {
                if (callback == null) {
                    throw new IllegalArgumentException("callback must not be null");
                }
                final String classname = message.getString("classname");
                final String filename = message.getString("filename");
                final EventDispatcherImpl dispatcher;
                try {
                    dispatcher = registerNewInstance(classname, filename);
                    callback.sendSuccess(dispatcher.guid);
                } catch (final Exception e) {
                    Log.e(LOGTAG, "Unable to load dex successfully", e);
                    callback.sendError(e.toString());
                }
            }
            break;

            case MESSAGE_UNLOAD: {
                if (callback == null) {
                    throw new IllegalArgumentException("callback must not be null");
                }
                final String guid = message.getString("guid");
                final EventDispatcherImpl dispatcher = mGUIDToDispatcherMap.remove(guid);
                if (dispatcher == null) {
                    Log.w(LOGTAG, "Attempting to unload addon with unknown " +
                                  "associated dispatcher; ignoring.");
                    callback.sendSuccess(false);
                } else {
                    dispatcher.unregisterAllEventListeners();
                    callback.sendSuccess(true);
                }
            }
            break;
        }
    }

    /**
     * An event dispatcher is tied to a single Java Addon instance.  It serves to prefix all
     * messages with its unique GUID.
     * <p/>
     * Curiously, the dispatcher does not hold a direct reference to its add-on instance.  It will
     * likely hold indirect instances through its wrapping map, since the instance will probably
     * register event listeners that hold a reference to itself.  When these listeners are
     * unregistered, any link will be broken, allowing the instances to be garbage collected.
     */
    private class EventDispatcherImpl implements JavaAddonInterfaceV1.EventDispatcher {
        private final String guid;
        private final String dexFileName;

        // Protected by synchronized (this).
        private final Map<JavaAddonInterfaceV1.EventListener, Pair<BundleEventListener, String[]>>
                mListenerToWrapperMap = new IdentityHashMap<>();

        public EventDispatcherImpl(String guid, String dexFileName) {
            this.guid = guid;
            this.dexFileName = dexFileName;
        }

        protected class ListenerWrapper implements BundleEventListener {
            private final JavaAddonInterfaceV1.EventListener listener;

            public ListenerWrapper(JavaAddonInterfaceV1.EventListener listener) {
                this.listener = listener;
            }

            @Override // BundleEventListener
            public void handleMessage(final String prefixedEvent, final GeckoBundle message,
                                      final EventCallback callback) {
                if (!prefixedEvent.startsWith(guid + ":")) {
                    return;
                }
                final String event = prefixedEvent.substring(guid.length() + 1); // Skip "guid:".
                final JavaAddonInterfaceV1.EventCallback callbackAdapter;
                if (callback == null) {
                    callbackAdapter = null;
                } else {
                    callbackAdapter = new JavaAddonInterfaceV1.EventCallback() {
                        private void invokeCallback(final boolean success, final Object response) {
                            final GeckoBundle bundle;
                            try {
                                final JSONObject wrapper = new JSONObject();
                                wrapper.put("response", response);
                                bundle = GeckoBundle.fromJSONObject(wrapper);
                            } catch (final JSONException e) {
                                Log.e(LOGTAG, "Unsupported response for message [" + event + "]", e);
                                return;
                            }
                            if (success) {
                                callback.sendSuccess(bundle);
                            } else {
                                callback.sendError(bundle);
                            }
                        }

                        @Override
                        public void sendSuccess(Object response) {
                            invokeCallback(/* success */ true, response);
                        }

                        @Override
                        public void sendError(Object response) {
                            invokeCallback(/* success */ false, response);
                        }
                    };
                }
                final JSONObject json;
                try {
                    json = message.toJSONObject();
                } catch (final JSONException e) {
                    Log.e(LOGTAG, "Exception handling message [" + prefixedEvent + "]", e);
                    return;
                }
                listener.handleMessage(mApplicationContext, event, json, callbackAdapter);
            }
        }

        @Override
        public synchronized void registerEventListener(
                final JavaAddonInterfaceV1.EventListener listener, String... events) {
            if (mListenerToWrapperMap.containsKey(listener)) {
                Log.e(LOGTAG, "Attempting to register listener which is already registered; ignoring.");
                return;
            }

            final BundleEventListener listenerWrapper = new ListenerWrapper(listener);

            final String[] prefixedEvents = new String[events.length];
            for (int i = 0; i < events.length; i++) {
                prefixedEvents[i] = this.guid + ":" + events[i];
            }
            mDispatcher.registerGeckoThreadListener(listenerWrapper, prefixedEvents);
            mListenerToWrapperMap.put(listener, new Pair<>(listenerWrapper, prefixedEvents));
        }

        @Override
        public synchronized void unregisterEventListener(
                final JavaAddonInterfaceV1.EventListener listener) {
            final Pair<BundleEventListener, String[]> pair = mListenerToWrapperMap.remove(listener);
            if (pair == null) {
                Log.e(LOGTAG, "Attempting to unregister listener which is not registered; ignoring.");
                return;
            }
            mDispatcher.unregisterGeckoThreadListener(pair.first, pair.second);
        }

        protected synchronized void unregisterAllEventListeners() {
            // Unregister everything, then forget everything.
            for (Pair<BundleEventListener, String[]> pair : mListenerToWrapperMap.values()) {
                 mDispatcher.unregisterGeckoThreadListener(pair.first, pair.second);
            }
            mListenerToWrapperMap.clear();
        }

        @Override
        public void sendRequestToGecko(final String event, final JSONObject message,
                                       final JavaAddonInterfaceV1.RequestCallback callback) {
            final String prefixedEvent = guid + ":" + event;
            final GeckoBundle data;
            try {
                data = GeckoBundle.fromJSONObject(message);
            } catch (final JSONException e) {
                Log.e(LOGTAG, "Cannot convert message", e);
                return;
            }

            final EventCallback cb;
            if (callback == null) {
                cb = null;
            } else {
                cb = new EventCallback() {
                    @Override
                    public void sendSuccess(final Object response) {
                        if (!(response instanceof GeckoBundle)) {
                            Log.e(LOGTAG, "Response to request [" + event + "] must be an object");
                            return;
                        }
                        try {
                            final JSONObject json = ((GeckoBundle) response).toJSONObject();
                            callback.onResponse(GeckoAppShell.getApplicationContext(), json);
                        } catch (final Exception e) {
                            // No way to report failure.
                            Log.e(LOGTAG, "Exception handling response to request [" +
                                          event + "]", e);
                        }
                    }

                    @Override
                    public void sendError(final Object response) {
                        Log.e(LOGTAG, "Exception handling response to request [" +
                                      event + "]: " + response);
                    }
                };
            }

            mDispatcher.dispatch(prefixedEvent, data, cb);
        }
    }
}
