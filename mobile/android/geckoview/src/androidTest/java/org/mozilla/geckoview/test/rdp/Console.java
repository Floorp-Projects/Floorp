/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.json.JSONObject;

/**
 * Provide access to the webconsole API.
 */
public final class Console extends Actor {
    private final ReplyParser<Object> JS_RESULT_PARSER = new ReplyParser<Object>() {
        @Override
        public boolean canParse(@NonNull JSONObject packet) {
            return packet.has("result") || packet.has("exception");
        }

        @Override
        public @Nullable Object parse(@NonNull JSONObject packet) {
            if (packet.has("exception") && !packet.isNull("exception")) {
                throw new RuntimeException(
                        "JS exception: " + packet.optString("exceptionMessage", null));
            }
            return Grip.unpack(connection, packet.opt("result"));
        }
    };

    /* package */ Console(final RDPConnection connection, final String name) {
        super(connection, name);
    }

    /**
     * Evaluate a JavaScript expression within the scope of this actor, and return its
     * result as a {@link Reply} object. Use {@link Reply#get} to wait and get the result,
     * or use {@link Reply#hasResult} to check the status of the result.
     *
     * Null and undefined are converted to null. Boolean and string results are
     * converted to their Java counterparts. Number results are converted to Double.
     * Array-like object results, including Array, arguments, and NodeList, are converted
     * to {@code List<Object>}. Other object results, including DOM nodes, are converted
     * to {@code Map<String, Object>}.
     *
     * @param js JavaScript expression.
     * @return Result of the evaluation as a {@link Reply} object
     */
    public Reply<Object> evaluateJS(final String js) {
        return sendPacket("{\"type\":\"evaluateJS\",\"text\":" + JSONObject.quote(js) + '}',
                          JS_RESULT_PARSER);
    }
}
