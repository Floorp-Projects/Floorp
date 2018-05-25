/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import android.support.annotation.NonNull;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Provide access to the promises API.
 */
public final class Promises extends Actor {
    private final ReplyParser<Promise[]> PROMISE_LIST_PARSER = new ReplyParser<Promise[]>() {
        @Override
        public boolean canParse(@NonNull JSONObject packet) {
            return packet.has("promises");
        }

        @Override
        public @NonNull Promise[] parse(@NonNull JSONObject packet) {
            return getPromisesFromArray(packet.optJSONArray("promises"),
                                        /* canCreate */ true);
        }
    };

    private final Set<Promise> mPromises = new HashSet<>();

    /* package */ Promises(final RDPConnection connection, final String name) {
        super(connection, name);
        attach();
    }

    /**
     * Attach to the promises API.
     */
    private void attach() {
        sendPacket("{\"type\":\"attach\"}", JSON_PARSER).get();
    }

    /**
     * Detach from the promises API.
     */
    public void detach() {
        for (final Promise promise : mPromises) {
            promise.release();
        }
        sendPacket("{\"type\":\"detach\"}", JSON_PARSER).get();
        release();
    }

    /* package */ Promise[] getPromisesFromArray(final @NonNull JSONArray array,
                                                 final boolean canCreate) {
        final Promise[] promises = new Promise[array.length()];
        for (int i = 0; i < promises.length; i++) {
            final JSONObject grip = array.optJSONObject(i);
            final Promise promise = (Promise) connection.getActor(grip);
            if (promise != null) {
                promise.setPromiseState(grip);
                promises[i] = promise;
            } else if (canCreate) {
                promises[i] = new Promise(connection, grip);
            }
        }
        return promises;
    }

    /**
     * Return a list of live promises.
     *
     * @returns List of promises.
     */
    public @NonNull Promise[] listPromises() {
        final Promise[] promises = sendPacket("{\"type\":\"listPromises\"}",
                                              PROMISE_LIST_PARSER).get();
        mPromises.addAll(Arrays.asList(promises));
        return promises;
    }

    @Override
    protected void onPacket(final @NonNull JSONObject packet) {
        final String type = packet.optString("type", null);
        if ("new-promises".equals(type)) {
            // We always call listPromises() to get updated Promises,
            // so that means we shouldn't handle "new-promises" here.
        } else if ("promises-settled".equals(type)) {
            // getPromisesFromArray will update states for us.
            getPromisesFromArray(packet.optJSONArray("data"),
                                 /* canCreate */ false);
        } else {
            super.onPacket(packet);
        }
    }
}
