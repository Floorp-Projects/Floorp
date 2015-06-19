/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.javaaddons;

import android.content.Context;
import org.json.JSONObject;

public interface JavaAddonInterfaceV1 {
    /**
     * Callback interface for Gecko requests.
     * <p/>
     * For each instance of EventCallback, exactly one of sendResponse, sendError, must be called to prevent observer leaks.
     * If more than one send* method is called, or if a single send method is called multiple times, an
     * {@link IllegalStateException} will be thrown.
     */
    interface EventCallback {
        /**
         * Sends a success response with the given data.
         *
         * @param response The response data to send to Gecko. Can be any of the types accepted by
         *                 JSONObject#put(String, Object).
         */
        public void sendSuccess(Object response);

        /**
         * Sends an error response with the given data.
         *
         * @param response The response data to send to Gecko. Can be any of the types accepted by
         *                 JSONObject#put(String, Object).
         */
        public void sendError(Object response);
    }

    interface EventDispatcher {
        void registerEventListener(EventListener listener, String... events);
        void unregisterEventListener(EventListener listener);

        void sendRequestToGecko(String event, JSONObject message, RequestCallback callback);
    }

    interface EventListener {
        public void handleMessage(final Context context, final String event, final JSONObject message, final EventCallback callback);
    }

    interface RequestCallback {
        void onResponse(final Context context, JSONObject jsonObject);
    }
}
