/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.util;

import java.util.concurrent.atomic.AtomicInteger;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.annotation.RobocopTarget;

import android.util.Log;

public abstract class GeckoRequest {
    private static final String LOGTAG = "GeckoRequest";
    private static final AtomicInteger currentId = new AtomicInteger(0);

    private final int id = currentId.getAndIncrement();
    private final String name;
    private final String data;

    /**
     * Creates a request that can be dispatched using
     * {@link GeckoAppShell#sendRequestToGecko(GeckoRequest)}.
     *
     * @param name The name of the event associated with this request, which must have a
     *             Gecko-side listener registered to respond to this request.
     * @param data Data to send with this request, which can be any object serializable by
     *             {@link JSONObject#put(String, Object)}.
     */
    @RobocopTarget
    public GeckoRequest(String name, Object data) {
        this.name = name;
        final JSONObject message = new JSONObject();
        try {
            message.put("id", id);
            message.put("data", data);
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON error", e);
        }
        this.data = message.toString();
    }

    /**
     * Gets the ID for this request.
     *
     * @return The request ID
     */
    public int getId() {
        return id;
    }

    /**
     * Gets the event name associated with this request.
     *
     * @return The name of the event sent to Gecko
     */
    public String getName() {
        return name;
    }

    /**
     * Gets the stringified data associated with this request.
     *
     * @return The data being sent with the request
     */
    public String getData() {
        return data;
    }

    /**
     * Callback executed when the request succeeds.
     *
     * @param nativeJSObject The response data from Gecko
     */
    @RobocopTarget
    public abstract void onResponse(NativeJSObject nativeJSObject);

    /**
     * Callback executed when the request fails.
     *
     * By default, an exception is thrown. This should be overridden if the
     * GeckoRequest is able to recover from the error.
     *
     * @throws RuntimeException
     */
    @RobocopTarget
    public void onError(NativeJSObject error) {
        final String message = error.optString("message", "<no message>");
        final String stack = error.optString("stack", "<no stack>");
        throw new RuntimeException("Unhandled error for GeckoRequest " + name + ": " + message + "\nJS stack:\n" + stack);
    }
}
