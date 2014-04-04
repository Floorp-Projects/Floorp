/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSContainer;

import org.json.JSONException;
import org.json.JSONObject;

import android.util.Log;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;

public final class EventDispatcher {
    private static final String LOGTAG = "GeckoEventDispatcher";
    private static final String GUID = "__guid__";
    private static final String SUFFIX_RETURN = "Return";
    private static final String SUFFIX_ERROR = "Error";

    /**
     * The capacity of a HashMap is rounded up to the next power-of-2. Every time the size
     * of the map goes beyond 75% of the capacity, the map is rehashed. Therefore, to
     * empirically determine the initial capacity that avoids rehashing, we need to
     * determine the initial size, divide it by 75%, and round up to the next power-of-2.
     */
    private static final int GECKO_NATIVE_EVENTS_COUNT = 0; // Default for HashMap
    private static final int GECKO_JSON_EVENTS_COUNT = 256; // Empirically measured

    private final Map<String, List<NativeEventListener>> mGeckoThreadNativeListeners =
        new HashMap<String, List<NativeEventListener>>(GECKO_NATIVE_EVENTS_COUNT);
    private final Map<String, List<GeckoEventListener>> mGeckoThreadJSONListeners =
        new HashMap<String, List<GeckoEventListener>>(GECKO_JSON_EVENTS_COUNT);

    private <T> void registerListener(final Class<? extends List<T>> listType,
                                      final Map<String, List<T>> listenersMap,
                                      final T listener,
                                      final String[] events) {
        try {
            synchronized (listenersMap) {
                for (final String event : events) {
                    List<T> listeners = listenersMap.get(event);
                    if (listeners == null) {
                        listeners = listType.newInstance();
                        listenersMap.put(event, listeners);
                    }
                    listeners.add(listener);
                }
            }
        } catch (final IllegalAccessException e) {
            throw new IllegalArgumentException("Invalid new list type", e);
        } catch (final InstantiationException e) {
            throw new IllegalArgumentException("Invalid new list type", e);
        }
    }

    private <T> void checkNotRegistered(final Map<String, List<T>> listenersMap,
                                        final String[] events) {
        synchronized (listenersMap) {
            for (final String event: events) {
                if (listenersMap.get(event) != null) {
                    throw new IllegalStateException(
                        "Already registered " + event + " under a different type");
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
                if (listeners == null ||
                    !listeners.remove(listener)) {
                    throw new IllegalArgumentException(event + " was not registered");
                }
            }
        }
    }

    @SuppressWarnings("unchecked")
    public void registerGeckoThreadListener(final NativeEventListener listener,
                                            final String... events) {
        checkNotRegistered(mGeckoThreadJSONListeners, events);

        // For listeners running on the Gecko thread, we want to notify the listeners
        // outside of our synchronized block, because the listeners may take an
        // indeterminate amount of time to run. Therefore, to ensure concurrency when
        // iterating the list outside of the synchronized block, we use a
        // CopyOnWriteArrayList.
        registerListener((Class)CopyOnWriteArrayList.class,
                         mGeckoThreadNativeListeners, listener, events);
    }

    @Deprecated // Use NativeEventListener instead
    @SuppressWarnings("unchecked")
    private void registerGeckoThreadListener(final GeckoEventListener listener,
                                             final String... events) {
        checkNotRegistered(mGeckoThreadNativeListeners, events);

        registerListener((Class)CopyOnWriteArrayList.class,
                         mGeckoThreadJSONListeners, listener, events);
    }

    public void unregisterGeckoThreadListener(final NativeEventListener listener,
                                              final String... events) {
        unregisterListener(mGeckoThreadNativeListeners, listener, events);
    }

    @Deprecated // Use NativeEventListener instead
    private void unregisterGeckoThreadListener(final GeckoEventListener listener,
                                               final String... events) {
        unregisterListener(mGeckoThreadJSONListeners, listener, events);
    }

    @Deprecated // Use one of the variants above.
    public void registerEventListener(final String event, final GeckoEventListener listener) {
        registerGeckoThreadListener(listener, event);
    }

    @Deprecated // Use one of the variants above
    public void unregisterEventListener(final String event, final GeckoEventListener listener) {
        unregisterGeckoThreadListener(listener, event);
    }

    public void dispatchEvent(final NativeJSContainer message) {
        try {
            // First try native listeners.
            final String type = message.getString("type");

            final List<NativeEventListener> listeners;
            synchronized (mGeckoThreadNativeListeners) {
                listeners = mGeckoThreadNativeListeners.get(type);
            }
            if (listeners != null) {
                if (listeners.size() == 0) {
                    Log.w(LOGTAG, "No listeners for " + type);
                }
                for (final NativeEventListener listener : listeners) {
                    listener.handleMessage(type, message);
                }
                // If we found native listeners, we assume we don't have any JSON listeners
                // and return early. This assumption is checked when registering listeners.
                return;
            }
        } catch (final IllegalArgumentException e) {
            // Message doesn't have a "type" property, fallback to JSON
        }
        try {
            // If we didn't find native listeners, try JSON listeners.
            dispatchEvent(new JSONObject(message.toString()));
        } catch (final JSONException e) {
            Log.e(LOGTAG, "Cannot parse JSON");
        } catch (final UnsupportedOperationException e) {
            Log.e(LOGTAG, "Cannot convert message to JSON");
        }
    }

    public void dispatchEvent(final JSONObject message) {
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
            if (listeners == null || listeners.size() == 0) {
                Log.w(LOGTAG, "No listeners for " + type);
                return;
            }
            for (final GeckoEventListener listener : listeners) {
                listener.handleMessage(type, message);
            }
        } catch (final JSONException e) {
            Log.e(LOGTAG, "handleGeckoMessage throws " + e, e);
        }
    }

    public static void sendResponse(JSONObject message, Object response) {
        sendResponseHelper(SUFFIX_RETURN, message, response);
    }

    public static void sendError(JSONObject message, Object response) {
        sendResponseHelper(SUFFIX_ERROR, message, response);
    }

    private static void sendResponseHelper(String suffix, JSONObject message, Object response) {
        try {
            final JSONObject wrapper = new JSONObject();
            wrapper.put(GUID, message.getString(GUID));
            wrapper.put("response", response);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(message.getString("type") + ":" + suffix, wrapper.toString()));
        } catch (Exception e) {
            Log.e(LOGTAG, "Unable to send " + suffix, e);
        }
    }
}
