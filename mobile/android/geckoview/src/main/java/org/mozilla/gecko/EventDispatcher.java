/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSContainer;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;

@RobocopTarget
public final class EventDispatcher {
    private static final String LOGTAG = "GeckoEventDispatcher";
    private static final String GUID = "__guid__";
    private static final String STATUS_ERROR = "error";
    private static final String STATUS_SUCCESS = "success";

    private static final EventDispatcher INSTANCE = new EventDispatcher();

    /**
     * The capacity of a HashMap is rounded up to the next power-of-2. Every time the size
     * of the map goes beyond 75% of the capacity, the map is rehashed. Therefore, to
     * empirically determine the initial capacity that avoids rehashing, we need to
     * determine the initial size, divide it by 75%, and round up to the next power-of-2.
     */
    private static final int DEFAULT_GECKO_NATIVE_EVENTS_COUNT = 0; // Default for HashMap
    private static final int DEFAULT_GECKO_JSON_EVENTS_COUNT = 256; // Empirically measured
    private static final int DEFAULT_UI_EVENTS_COUNT = 0; // Default for HashMap
    private static final int DEFAULT_BACKGROUND_EVENTS_COUNT = 0; // Default for HashMap

    private final Map<String, List<NativeEventListener>> mGeckoThreadNativeListeners =
        new HashMap<String, List<NativeEventListener>>(DEFAULT_GECKO_NATIVE_EVENTS_COUNT);
    private final Map<String, List<GeckoEventListener>> mGeckoThreadJSONListeners =
        new HashMap<String, List<GeckoEventListener>>(DEFAULT_GECKO_JSON_EVENTS_COUNT);
    private final Map<String, List<BundleEventListener>> mUiThreadListeners =
        new HashMap<String, List<BundleEventListener>>(DEFAULT_UI_EVENTS_COUNT);
    private final Map<String, List<BundleEventListener>> mBackgroundThreadListeners =
        new HashMap<String, List<BundleEventListener>>(DEFAULT_BACKGROUND_EVENTS_COUNT);

    public static EventDispatcher getInstance() {
        return INSTANCE;
    }

    private EventDispatcher() {
    }

    private <T> void registerListener(final Class<?> listType,
                                      final Map<String, List<T>> listenersMap,
                                      final T listener,
                                      final String[] events) {
        try {
            synchronized (listenersMap) {
                for (final String event : events) {
                    List<T> listeners = listenersMap.get(event);
                    if (listeners == null) {
                        // Java doesn't let us put Class<? extends List<T>> as the type for listType.
                        @SuppressWarnings("unchecked")
                        final Class<? extends List<T>> type = (Class) listType;
                        listeners = type.newInstance();
                        listenersMap.put(event, listeners);
                    }
                    if (!AppConstants.RELEASE_BUILD && listeners.contains(listener)) {
                        throw new IllegalStateException("Already registered " + event);
                    }
                    listeners.add(listener);
                }
            }
        } catch (final IllegalAccessException | InstantiationException e) {
            throw new IllegalArgumentException("Invalid new list type", e);
        }
    }

    private void checkNotRegisteredElsewhere(final Map<String, ?> allowedMap,
                                             final String[] events) {
        if (AppConstants.RELEASE_BUILD) {
            // for performance reasons, we only check for
            // already-registered listeners in non-release builds.
            return;
        }
        for (final Map<String, ?> listenersMap : Arrays.asList(mGeckoThreadNativeListeners,
                                                               mGeckoThreadJSONListeners,
                                                               mUiThreadListeners,
                                                               mBackgroundThreadListeners)) {
            if (listenersMap == allowedMap) {
                continue;
            }
            synchronized (listenersMap) {
                for (final String event : events) {
                    if (listenersMap.get(event) != null) {
                        throw new IllegalStateException(
                            "Already registered " + event + " under a different type");
                    }
                }
            }
        }
    }

    private <T> void unregisterListener(final Map<String, List<T>> listenersMap,
                                        final T listener,
                                        final String[] events) {
        synchronized (listenersMap) {
            for (final String event : events) {
                List<T> listeners = listenersMap.get(event);
                if ((listeners == null ||
                     !listeners.remove(listener)) && !AppConstants.RELEASE_BUILD) {
                    throw new IllegalArgumentException(event + " was not registered");
                }
            }
        }
    }

    public void registerGeckoThreadListener(final NativeEventListener listener,
                                            final String... events) {
        checkNotRegisteredElsewhere(mGeckoThreadNativeListeners, events);

        // For listeners running on the Gecko thread, we want to notify the listeners
        // outside of our synchronized block, because the listeners may take an
        // indeterminate amount of time to run. Therefore, to ensure concurrency when
        // iterating the list outside of the synchronized block, we use a
        // CopyOnWriteArrayList.
        registerListener(CopyOnWriteArrayList.class,
                         mGeckoThreadNativeListeners, listener, events);
    }

    @Deprecated // Use NativeEventListener instead
    public void registerGeckoThreadListener(final GeckoEventListener listener,
                                            final String... events) {
        checkNotRegisteredElsewhere(mGeckoThreadJSONListeners, events);

        registerListener(CopyOnWriteArrayList.class,
                         mGeckoThreadJSONListeners, listener, events);
    }

    public void registerUiThreadListener(final BundleEventListener listener,
                                         final String... events) {
        checkNotRegisteredElsewhere(mUiThreadListeners, events);

        registerListener(ArrayList.class,
                         mUiThreadListeners, listener, events);
    }

    public void registerBackgroundThreadListener(final BundleEventListener listener,
                                                 final String... events) {
        checkNotRegisteredElsewhere(mBackgroundThreadListeners, events);

        registerListener(ArrayList.class,
                         mBackgroundThreadListeners, listener, events);
    }

    public void unregisterGeckoThreadListener(final NativeEventListener listener,
                                              final String... events) {
        unregisterListener(mGeckoThreadNativeListeners, listener, events);
    }

    @Deprecated // Use NativeEventListener instead
    public void unregisterGeckoThreadListener(final GeckoEventListener listener,
                                              final String... events) {
        unregisterListener(mGeckoThreadJSONListeners, listener, events);
    }

    public void unregisterUiThreadListener(final BundleEventListener listener,
                                           final String... events) {
        unregisterListener(mUiThreadListeners, listener, events);
    }

    public void unregisterBackgroundThreadListener(final BundleEventListener listener,
                                                   final String... events) {
        unregisterListener(mBackgroundThreadListeners, listener, events);
    }

    public void dispatchEvent(final NativeJSContainer message) {
        // First try native listeners.
        final String type = message.optString("type", null);
        if (type == null) {
            Log.e(LOGTAG, "JSON message must have a type property");
            return;
        }

        final List<NativeEventListener> listeners;
        synchronized (mGeckoThreadNativeListeners) {
            listeners = mGeckoThreadNativeListeners.get(type);
        }

        final String guid = message.optString(GUID, null);
        EventCallback callback = null;
        if (guid != null) {
            callback = new GeckoEventCallback(guid, type);
        }

        if (listeners != null) {
            if (listeners.isEmpty()) {
                Log.w(LOGTAG, "No listeners for " + type);

                // There were native listeners, and they're gone.  Dispatch an error rather than
                // looking for JSON listeners.
                if (callback != null) {
                    callback.sendError("No listeners for request");
                }
            }
            try {
                for (final NativeEventListener listener : listeners) {
                    listener.handleMessage(type, message, callback);
                }
            } catch (final NativeJSObject.InvalidPropertyException e) {
                Log.e(LOGTAG, "Exception occurred while handling " + type, e);
            }
            // If we found native listeners, we assume we don't have any other types of listeners
            // and return early. This assumption is checked when registering listeners.
            return;
        }

        // Check for thread event listeners before checking for JSON event listeners,
        // because checking for thread listeners is very fast and doesn't require us to
        // serialize into JSON and construct a JSONObject.
        if (dispatchToThread(type, message, callback,
                             mUiThreadListeners, ThreadUtils.getUiHandler()) ||
            dispatchToThread(type, message, callback,
                             mBackgroundThreadListeners, ThreadUtils.getBackgroundHandler())) {

            // If we found thread listeners, we assume we don't have any other types of listeners
            // and return early. This assumption is checked when registering listeners.
            return;
        }

        try {
            // If we didn't find native listeners, try JSON listeners.
            dispatchEvent(new JSONObject(message.toString()), callback);
        } catch (final JSONException e) {
            Log.e(LOGTAG, "Cannot parse JSON", e);
        } catch (final UnsupportedOperationException e) {
            Log.e(LOGTAG, "Cannot convert message to JSON", e);
        }
    }

    private boolean dispatchToThread(final String type,
                                     final NativeJSObject message,
                                     final EventCallback callback,
                                     final Map<String, List<BundleEventListener>> listenersMap,
                                     final Handler thread) {
        // We need to hold the lock throughout dispatching, to ensure the listeners list
        // is consistent, while we iterate over it. We don't have to worry about listeners
        // running for a long time while we have the lock, because the listeners will run
        // on a separate thread.
        synchronized (listenersMap) {
            final List<BundleEventListener> listeners = listenersMap.get(type);
            if (listeners == null) {
                return false;
            }

            if (listeners.isEmpty()) {
                Log.w(LOGTAG, "No listeners for " + type);

                // There were native listeners, and they're gone.
                // Dispatch an error rather than looking for more listeners.
                if (callback != null) {
                    callback.sendError("No listeners for request");
                }
                return true;
            }

            final Bundle messageAsBundle;
            try {
                messageAsBundle = message.toBundle();
            } catch (final NativeJSObject.InvalidPropertyException e) {
                Log.e(LOGTAG, "Exception occurred while handling " + type, e);
                return true;
            }

            // Event listeners will call | callback.sendError | if applicable.
            for (final BundleEventListener listener : listeners) {
                thread.post(new Runnable() {
                    @Override
                    public void run() {
                        listener.handleMessage(type, messageAsBundle, callback);
                    }
                });
            }
            return true;
        }
    }

    public void dispatchEvent(final JSONObject message, final EventCallback callback) {
        // {
        //   "type": "value",
        //   "event_specific": "value",
        //   ...
        try {
            final String type = message.getString("type");

            List<GeckoEventListener> listeners;
            synchronized (mGeckoThreadJSONListeners) {
                listeners = mGeckoThreadJSONListeners.get(type);
            }
            if (listeners == null || listeners.isEmpty()) {
                Log.w(LOGTAG, "No listeners for " + type);

                // If there are no listeners, dispatch an error.
                if (callback != null) {
                    callback.sendError("No listeners for request");
                }
                return;
            }
            for (final GeckoEventListener listener : listeners) {
                listener.handleMessage(type, message);
            }
        } catch (final JSONException e) {
            Log.e(LOGTAG, "handleGeckoMessage throws " + e, e);
        }
    }

    @RobocopTarget
    @Deprecated
    public static void sendResponse(JSONObject message, Object response) {
        sendResponseHelper(STATUS_SUCCESS, message, response);
    }

    @Deprecated
    public static void sendError(JSONObject message, Object response) {
        sendResponseHelper(STATUS_ERROR, message, response);
    }

    @Deprecated
    private static void sendResponseHelper(String status, JSONObject message, Object response) {
        try {
            final String topic = message.getString("type") + ":Response";
            final JSONObject wrapper = new JSONObject();
            wrapper.put(GUID, message.getString(GUID));
            wrapper.put("status", status);
            wrapper.put("response", response);

            if (ThreadUtils.isOnGeckoThread()) {
                GeckoAppShell.syncNotifyObservers(topic, wrapper.toString());
            } else {
                GeckoAppShell.notifyObservers(topic, wrapper.toString(),
                                              GeckoThread.State.PROFILE_READY);
            }
        } catch (final JSONException e) {
            Log.e(LOGTAG, "Unable to send response", e);
        }
    }

    private static class GeckoEventCallback implements EventCallback {
        private final String guid;
        private final String type;
        private boolean sent;

        public GeckoEventCallback(final String guid, final String type) {
            this.guid = guid;
            this.type = type;
        }

        @Override
        public void sendSuccess(final Object response) {
            sendResponse(STATUS_SUCCESS, response);
        }

        @Override
        public void sendError(final Object response) {
            sendResponse(STATUS_ERROR, response);
        }

        private void sendResponse(final String status, final Object response) {
            if (sent) {
                throw new IllegalStateException("Callback has already been executed for type=" +
                        type + ", guid=" + guid);
            }

            sent = true;

            try {
                final String topic = type + ":Response";
                final JSONObject wrapper = new JSONObject();
                wrapper.put(GUID, guid);
                wrapper.put("status", status);
                wrapper.put("response", response);

                if (ThreadUtils.isOnGeckoThread()) {
                    GeckoAppShell.syncNotifyObservers(topic, wrapper.toString());
                } else {
                    GeckoAppShell.notifyObservers(topic, wrapper.toString(),
                                                  GeckoThread.State.PROFILE_READY);
                }
            } catch (final JSONException e) {
                Log.e(LOGTAG, "Unable to send response for: " + type, e);
            }
        }
    }
}
