/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.ReflectionTarget;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
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
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;

@RobocopTarget
public final class EventDispatcher extends JNIObject {
    private static final String LOGTAG = "GeckoEventDispatcher";
    /* package */ static final String GUID = "__guid__";
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
    private static final int DEFAULT_GECKO_EVENTS_COUNT = 0; // Default for HashMap
    private static final int DEFAULT_UI_EVENTS_COUNT = 0; // Default for HashMap
    private static final int DEFAULT_BACKGROUND_EVENTS_COUNT = 0; // Default for HashMap

    // Legacy events.
    private final Map<String, List<NativeEventListener>> mGeckoThreadNativeListeners =
        new HashMap<String, List<NativeEventListener>>(DEFAULT_GECKO_NATIVE_EVENTS_COUNT);
    private final Map<String, List<GeckoEventListener>> mGeckoThreadJSONListeners =
        new HashMap<String, List<GeckoEventListener>>(DEFAULT_GECKO_JSON_EVENTS_COUNT);

    // GeckoBundle-based events.
    private final Map<String, List<BundleEventListener>> mGeckoThreadListeners =
        new HashMap<String, List<BundleEventListener>>(DEFAULT_GECKO_EVENTS_COUNT);
    private final Map<String, List<BundleEventListener>> mUiThreadListeners =
        new HashMap<String, List<BundleEventListener>>(DEFAULT_UI_EVENTS_COUNT);
    private final Map<String, List<BundleEventListener>> mBackgroundThreadListeners =
        new HashMap<String, List<BundleEventListener>>(DEFAULT_BACKGROUND_EVENTS_COUNT);

    private boolean mAttachedToGecko;

    @ReflectionTarget
    @WrapForJNI(calledFrom = "gecko")
    public static EventDispatcher getInstance() {
        return INSTANCE;
    }

    /* package */ EventDispatcher() {
    }

    @WrapForJNI(dispatchTo = "gecko") @Override // JNIObject
    protected native void disposeNative();

    @WrapForJNI private static final int DETACHED = 0;
    @WrapForJNI private static final int ATTACHED = 1;
    @WrapForJNI private static final int REATTACHING = 2;

    @WrapForJNI(calledFrom = "gecko")
    private synchronized void setAttachedToGecko(final int state) {
        if (mAttachedToGecko && state == DETACHED) {
            if (GeckoThread.isStateAtLeast(GeckoThread.State.JNI_READY)) {
                disposeNative();
            } else {
                GeckoThread.queueNativeCallUntil(GeckoThread.State.JNI_READY,
                        this, "disposeNative");
            }
        }
        mAttachedToGecko = (state == ATTACHED);
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
                    if (!AppConstants.RELEASE_OR_BETA && listeners.contains(listener)) {
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
        if (AppConstants.RELEASE_OR_BETA) {
            // for performance reasons, we only check for
            // already-registered listeners in non-release builds.
            return;
        }
        for (final Map<String, ?> listenersMap : Arrays.asList(mGeckoThreadNativeListeners,
                                                               mGeckoThreadJSONListeners,
                                                               mGeckoThreadListeners,
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
                     !listeners.remove(listener)) && !AppConstants.RELEASE_OR_BETA) {
                    throw new IllegalArgumentException(event + " was not registered");
                }
            }
        }
    }

    public void registerGeckoThreadListener(final BundleEventListener listener,
                                            final String... events) {
        checkNotRegisteredElsewhere(mGeckoThreadListeners, events);

        // For listeners running on the Gecko thread, we want to notify the listeners
        // outside of our synchronized block, because the listeners may take an
        // indeterminate amount of time to run. Therefore, to ensure concurrency when
        // iterating the list outside of the synchronized block, we use a
        // CopyOnWriteArrayList.
        registerListener(CopyOnWriteArrayList.class,
                         mGeckoThreadListeners, listener, events);
    }

    @Deprecated // Use BundleEventListener instead
    public void registerGeckoThreadListener(final NativeEventListener listener,
                                            final String... events) {
        checkNotRegisteredElsewhere(mGeckoThreadNativeListeners, events);

        registerListener(CopyOnWriteArrayList.class,
                         mGeckoThreadNativeListeners, listener, events);
    }

    @Deprecated // Use BundleEventListener instead
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

    @ReflectionTarget
    public void registerBackgroundThreadListener(final BundleEventListener listener,
                                                 final String... events) {
        checkNotRegisteredElsewhere(mBackgroundThreadListeners, events);

        registerListener(ArrayList.class,
                         mBackgroundThreadListeners, listener, events);
    }

    public void unregisterGeckoThreadListener(final BundleEventListener listener,
                                              final String... events) {
        unregisterListener(mGeckoThreadListeners, listener, events);
    }

    @Deprecated // Use BundleEventListener instead
    public void unregisterGeckoThreadListener(final NativeEventListener listener,
                                              final String... events) {
        unregisterListener(mGeckoThreadNativeListeners, listener, events);
    }

    @Deprecated // Use BundleEventListener instead
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

    private List<NativeEventListener> getNativeListeners(final String type) {
        final List<NativeEventListener> listeners;
        synchronized (mGeckoThreadNativeListeners) {
            listeners = mGeckoThreadNativeListeners.get(type);
        }
        return listeners;
    }

    private List<GeckoEventListener> getGeckoListeners(final String type) {
        final List<GeckoEventListener> listeners;
        synchronized (mGeckoThreadJSONListeners) {
            listeners = mGeckoThreadJSONListeners.get(type);
        }
        return listeners;
    }

    public boolean dispatchEvent(final NativeJSContainer message) {
        // First try native listeners.
        final String type = message.optString("type", null);
        if (type == null) {
            Log.e(LOGTAG, "JSON message must have a type property");
            return true; // It may seem odd to return true here, but it's necessary to preserve the correct behavior.
        }

        final List<NativeEventListener> listeners = getNativeListeners(type);

        final String guid = message.optString(GUID, null);
        EventCallback callback = null;
        if (guid != null) {
            callback = new GeckoEventCallback(guid, type);
        }

        if (listeners != null) {
            if (listeners.isEmpty()) {
                Log.w(LOGTAG, "No listeners for " + type);

                // There were native listeners, and they're gone.  Return a failure rather than
                // looking for JSON listeners. This is an optimization, as we can safely assume
                // that an event which previously had native listeners will never have JSON
                // listeners.
                return false;
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
            return true;
        }

        // Check for thread event listeners before checking for JSON event listeners,
        // because checking for thread listeners is very fast and doesn't require us to
        // serialize into JSON and construct a JSONObject.
        if (dispatchToThreads(type, message, /* bundle */ null, callback)) {
            // If we found thread listeners, we assume we don't have any other types of listeners
            // and return early. This assumption is checked when registering listeners.
            return true;
        }

        try {
            // If we didn't find native listeners, try JSON listeners.
            return dispatchEvent(new JSONObject(message.toString()), callback);
        } catch (final JSONException e) {
            Log.e(LOGTAG, "Cannot parse JSON", e);
        } catch (final UnsupportedOperationException e) {
            Log.e(LOGTAG, "Cannot convert message to JSON", e);
        }

        return true;
    }

    @WrapForJNI
    private native boolean hasGeckoListener(final String event);

    @WrapForJNI(dispatchTo = "gecko")
    private native void dispatchToGecko(final String event, final GeckoBundle data,
                                        final EventCallback callback);

    /**
     * Dispatch event to any registered Bundle listeners (non-Gecko thread listeners).
     *
     * @param type Event type
     * @param message Bundle message
     */
    public void dispatch(final String type, final GeckoBundle message) {
        dispatch(type, message, /* callback */ null);
    }

    /**
     * Dispatch event to any registered Bundle listeners (non-Gecko thread listeners).
     *
     * @param type Event type
     * @param message Bundle message
     * @param callback Optional object for callbacks from events.
     */
    public void dispatch(final String type, final GeckoBundle message,
                         final EventCallback callback) {
        synchronized (this) {
            if (mAttachedToGecko && hasGeckoListener(type)) {
                dispatchToGecko(type, message, JavaCallbackDelegate.wrap(callback));
                return;
            }
        }

        dispatchToThreads(type, /* js */ null, message, /* callback */ callback);
    }

    @WrapForJNI(calledFrom = "gecko")
    private boolean dispatchToThreads(final String type,
                                      final NativeJSObject jsMessage,
                                      final GeckoBundle bundleMessage,
                                      final EventCallback callback) {
        final List<BundleEventListener> geckoListeners;
        synchronized (mGeckoThreadListeners) {
            geckoListeners = mGeckoThreadListeners.get(type);
        }
        if (geckoListeners != null && !geckoListeners.isEmpty()) {
            final boolean onGeckoThread = ThreadUtils.isOnGeckoThread();
            final EventCallback wrappedCallback = JavaCallbackDelegate.wrap(callback);
            final GeckoBundle messageAsBundle;
            try {
                messageAsBundle = jsMessage != null ?
                        convertBundle(jsMessage.toBundle()) : bundleMessage;
            } catch (final NativeJSObject.InvalidPropertyException e) {
                Log.e(LOGTAG, "Exception occurred while handling " + type, e);
                return true;
            }

            for (final BundleEventListener listener : geckoListeners) {
                // For other threads, we always dispatch asynchronously. However, for
                // Gecko listeners only, we dispatch synchronously if we're already on
                // Gecko thread.
                if (onGeckoThread) {
                    listener.handleMessage(type, messageAsBundle, wrappedCallback);
                    continue;
                }
                ThreadUtils.sGeckoHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        listener.handleMessage(type, messageAsBundle, wrappedCallback);
                    }
                });
            }
            return true;
        }

        if (dispatchToThread(type, jsMessage, bundleMessage, callback,
                             mUiThreadListeners, ThreadUtils.getUiHandler())) {
            return true;
        }

        if (dispatchToThread(type, jsMessage, bundleMessage, callback,
                             mBackgroundThreadListeners, ThreadUtils.getBackgroundHandler())) {
            return true;
        }

        if (jsMessage == null) {
            Log.w(LOGTAG, "No listeners for " + type + " in dispatchToThreads");
        }

        if (!AppConstants.RELEASE_OR_BETA && jsMessage == null) {
            // We're dispatching a Bundle message. Because Gecko thread listeners are not
            // supported for Bundle messages, do a sanity check to make sure we don't have
            // matching Gecko thread listeners.
            boolean hasGeckoListener = false;
            synchronized (mGeckoThreadNativeListeners) {
                hasGeckoListener |= mGeckoThreadNativeListeners.containsKey(type);
            }
            synchronized (mGeckoThreadJSONListeners) {
                hasGeckoListener |= mGeckoThreadJSONListeners.containsKey(type);
            }
            if (hasGeckoListener) {
                throw new IllegalStateException(
                        "Dispatching Bundle message to Gecko listener " + type);
            }
        }

        return false;
    }

    // XXX: temporary helper for converting Bundle to GeckoBundle.
    private GeckoBundle convertBundle(final Bundle bundle) {
        if (bundle == null) {
            return null;
        }

        final Set<String> keys = bundle.keySet();
        final GeckoBundle out = new GeckoBundle(keys.size());

        for (final String key : keys) {
            final Object value = bundle.get(key);

            if (value instanceof Bundle) {
                out.putBundle(key, convertBundle((Bundle) value));

            } else if (value instanceof Bundle[]) {
                final Bundle[] inArray = (Bundle[]) value;
                final GeckoBundle[] outArray = new GeckoBundle[inArray.length];
                for (int i = 0; i < inArray.length; i++) {
                    outArray[i] = convertBundle(inArray[i]);
                }
                out.putBundleArray(key, outArray);

            } else {
                out.put(key, value);
            }
        }
        return out;
    }

    private boolean dispatchToThread(final String type,
                                     final NativeJSObject jsMessage,
                                     final GeckoBundle bundleMessage,
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
                Log.w(LOGTAG, "No listeners for " + type + " in dispatchToThread");

                // There were native listeners, and they're gone.
                return false;
            }

            final GeckoBundle messageAsBundle;
            try {
                messageAsBundle = jsMessage != null ?
                        convertBundle(jsMessage.toBundle()) : bundleMessage;
            } catch (final NativeJSObject.InvalidPropertyException e) {
                Log.e(LOGTAG, "Exception occurred while handling " + type, e);
                return true;
            }

            // Use a delegate to make sure callbacks happen on a specific thread.
            final EventCallback wrappedCallback = JavaCallbackDelegate.wrap(callback);

            // Event listeners will call | callback.sendError | if applicable.
            for (final BundleEventListener listener : listeners) {
                thread.post(new Runnable() {
                    @Override
                    public void run() {
                        listener.handleMessage(type, messageAsBundle, wrappedCallback);
                    }
                });
            }
            return true;
        }
    }

    public boolean dispatchEvent(final JSONObject message, final EventCallback callback) {
        // {
        //   "type": "value",
        //   "event_specific": "value",
        //   ...
        try {
            final String type = message.getString("type");

            final List<GeckoEventListener> listeners = getGeckoListeners(type);

            if (listeners == null || listeners.isEmpty()) {
                Log.w(LOGTAG, "No listeners for " + type + " in dispatchEvent");

                return false;
            }

            for (final GeckoEventListener listener : listeners) {
                listener.handleMessage(type, message);
            }
        } catch (final JSONException e) {
            Log.e(LOGTAG, "handleGeckoMessage throws " + e, e);
        }

        return true;
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

    private static class NativeCallbackDelegate extends JNIObject implements EventCallback {
        @WrapForJNI(calledFrom = "gecko")
        private NativeCallbackDelegate() {
        }

        @Override // JNIObject
        protected void disposeNative() {
            // We dispose in finalize().
            throw new UnsupportedOperationException();
        }

        @WrapForJNI(dispatchTo = "proxy") @Override // EventCallback
        public native void sendSuccess(Object response);

        @WrapForJNI(dispatchTo = "proxy") @Override // EventCallback
        public native void sendError(Object response);

        @WrapForJNI(dispatchTo = "gecko") @Override // Object
        protected native void finalize();
    }

    private static class JavaCallbackDelegate implements EventCallback {
        private final Thread originalThread = Thread.currentThread();
        private final EventCallback callback;

        public static EventCallback wrap(final EventCallback callback) {
            if (callback == null) {
                return null;
            }
            if (callback instanceof NativeCallbackDelegate) {
                // NativeCallbackDelegate always posts to Gecko thread if needed.
                return callback;
            }
            return new JavaCallbackDelegate(callback);
        }

        JavaCallbackDelegate(final EventCallback callback) {
            this.callback = callback;
        }

        private void makeCallback(final boolean callSuccess, final Object response) {
            // Call back synchronously if we happen to be on the same thread as the thread
            // making the original request.
            if (ThreadUtils.isOnThread(originalThread)) {
                if (callSuccess) {
                    callback.sendSuccess(response);
                } else {
                    callback.sendError(response);
                }
                return;
            }

            // Make callback on the thread of the original request, if the original thread
            // is the UI or Gecko thread. Otherwise default to the background thread.
            final Handler handler =
                    originalThread == ThreadUtils.getUiThread() ? ThreadUtils.getUiHandler() :
                    originalThread == ThreadUtils.sGeckoThread ? ThreadUtils.sGeckoHandler :
                                                                 ThreadUtils.getBackgroundHandler();
            final EventCallback callback = this.callback;

            handler.post(new Runnable() {
                @Override
                public void run() {
                    if (callSuccess) {
                        callback.sendSuccess(response);
                    } else {
                        callback.sendError(response);
                    }
                }
            });
        }

        @Override // EventCallback
        public void sendSuccess(Object response) {
            makeCallback(/* success */ true, response);
        }

        @Override // EventCallback
        public void sendError(Object response) {
            makeCallback(/* success */ false, response);
        }
    }

    /* package */ static class GeckoEventCallback implements EventCallback {
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
