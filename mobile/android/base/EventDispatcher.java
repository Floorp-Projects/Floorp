/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONObject;

import android.util.Log;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;

public final class EventDispatcher {
    private static final String LOGTAG = "GeckoEventDispatcher";
    private static final String GUID = "__guid__";

    private final Map<String, CopyOnWriteArrayList<GeckoEventListener>> mEventListeners
                  = new HashMap<String, CopyOnWriteArrayList<GeckoEventListener>>();

    public void registerEventListener(String event, GeckoEventListener listener) {
        synchronized (mEventListeners) {
            CopyOnWriteArrayList<GeckoEventListener> listeners = mEventListeners.get(event);
            if (listeners == null) {
                // create a CopyOnWriteArrayList so that we can modify it
                // concurrently with iterating through it in handleGeckoMessage.
                // Otherwise we could end up throwing a ConcurrentModificationException.
                listeners = new CopyOnWriteArrayList<GeckoEventListener>();
            } else if (listeners.contains(listener)) {
                Log.w(LOGTAG, "EventListener already registered for event '" + event + "'",
                      new IllegalArgumentException());
            }
            listeners.add(listener);
            mEventListeners.put(event, listeners);
        }
    }

    public void unregisterEventListener(String event, GeckoEventListener listener) {
        synchronized (mEventListeners) {
            CopyOnWriteArrayList<GeckoEventListener> listeners = mEventListeners.get(event);
            if (listeners == null) {
                Log.w(LOGTAG, "unregisterEventListener: event '" + event + "' has no listeners");
                return;
            }
            if (!listeners.remove(listener)) {
                Log.w(LOGTAG, "unregisterEventListener: tried to remove an unregistered listener " +
                              "for event '" + event + "'");
            }
            if (listeners.size() == 0) {
                mEventListeners.remove(event);
            }
        }
    }

    public void dispatchEvent(String message) {
        try {
            JSONObject json = new JSONObject(message);
            dispatchEvent(json);
        } catch (Exception e) {
            Log.e(LOGTAG, "dispatchEvent: malformed JSON.", e);
        }
    }

    public void dispatchEvent(JSONObject json) {
        // {
        //   "type": "value",
        //   "event_specific": "value",
        //   ...
        try {
            JSONObject gecko = json.has("gecko") ? json.getJSONObject("gecko") : null;
            if (gecko != null) {
                json = gecko;
            }

            String type = json.getString("type");

            if (gecko != null) {
                Log.w(LOGTAG, "Message '" + type + "' has deprecated 'gecko' property!");
            }

            CopyOnWriteArrayList<GeckoEventListener> listeners;
            synchronized (mEventListeners) {
                listeners = mEventListeners.get(type);
            }

            if (listeners == null || listeners.size() == 0) {
                Log.d(LOGTAG, "dispatchEvent: no listeners registered for event '" + type + "'");
                return;
            }

            for (GeckoEventListener listener : listeners) {
                listener.handleMessage(type, json);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "handleGeckoMessage throws " + e, e);
        }

    }

    public static void sendResponse(JSONObject message, JSONObject response) {
        try {
            response.put(GUID, message.getString(GUID));
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(message.getString("type") + ":Return", response.toString()));
        } catch (Exception ex) {
            Log.e(LOGTAG, "Unable to send response", ex);
        }
    }

    public static void sendError(JSONObject message, JSONObject error) {
        try {
            error.put(GUID, message.getString(GUID));
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(message.getString("type") + ":Error", error.toString()));
        } catch (Exception ex) {
            Log.e(LOGTAG, "Unable to send error", ex);
        }
    }
}
