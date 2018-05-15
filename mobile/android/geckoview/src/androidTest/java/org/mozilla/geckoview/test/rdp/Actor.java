/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Base class for actors in the remote debugging protocol. Provides basic methods such as
 * {@link #sendPacket}. The actor is automatically registered with the connection on
 * creation, and its {@link #onPacket} method is called whenever a packet is received with
 * the actor as the target.
 */
public class Actor {
    /* package */ interface ReplyParser<T> {
        boolean canParse(@NonNull JSONObject packet);
        @Nullable T parse(@NonNull JSONObject packet);
    }

    /* package */ static final ReplyParser<JSONObject> JSON_PARSER = new ReplyParser<JSONObject>() {
        @Override
        public boolean canParse(@NonNull JSONObject packet) {
            return true;
        }

        @Override
        public @NonNull JSONObject parse(@NonNull JSONObject packet) {
            return packet;
        }
    };

    /**
     * Represent an asynchronous reply, and provide methods for getting the result of the reply.
     *
     * @param <T> Type of the result from the reply.
     */
    public class Reply<T> {
        /* package */ final ReplyParser<T> parser;
        /* package */ JSONObject packet;

        /* package */ Reply(final @NonNull ReplyParser<T> parser) {
            this.parser = parser;
        }

        /**
         * Return whether the result has been received. After receiving the result,
         * calling {@link #get} will not block.
         *
         * @return True if the result has been received
         */
        public boolean hasResult() {
            return packet != null;
        }

        /**
         * Get the result and wait for it if necessary.
         *
         * @return Result object.
         */
        public T get() {
            while (packet == null) {
                Actor.this.connection.processInputPacket();
            }
            return parser.parse(packet);
        }
    }

    public final RDPConnection connection;
    public final String name;

    private List<Reply<?>> mPendingReplies;

    protected Actor(final RDPConnection connection, final JSONObject packet) {
        this(connection, packet.optString("actor", null));
    }

    protected Actor(final RDPConnection connection, final String name) {
        if (name == null) {
            throw new IllegalArgumentException();
        }
        this.connection = connection;
        this.name = name;
        connection.addActor(name, this);
    }

    @Override
    public boolean equals(Object o) {
        return (o instanceof Actor) && name.equals(((Actor) o).name);
    }

    @Override
    public int hashCode() {
        return name.hashCode();
    }

    protected void release() {
        connection.removeActor(name);
    }

    /* package */ <T> Reply<T> addReply(final @NonNull ReplyParser<T> parser) {
        final Reply<T> reply = new Reply<>(parser);
        if (mPendingReplies == null) {
            mPendingReplies = new ArrayList<>(1);
        }
        mPendingReplies.add(reply);
        return reply;
    }

    protected <T> Reply<T> sendPacket(final @NonNull String packet,
                                      final @NonNull ReplyParser<T> parser) {
        if (packet.charAt(0) != '{') {
            throw new IllegalArgumentException();
        }
        connection.sendRawPacket("{\"to\":" + JSONObject.quote(name) + ',' + packet.substring(1));
        return addReply(parser);
    }

    protected <T> Reply<T> sendPacket(final @NonNull JSONObject packet,
                                      final @NonNull ReplyParser<T> parser) {
        try {
            packet.put("to", name);
            connection.sendRawPacket(packet);
            return addReply(parser);
        } catch (final JSONException e) {
            throw new RuntimeException(e);
        }
    }

    protected void onPacket(final @NonNull JSONObject packet) {
        if (mPendingReplies == null) {
            return;
        }
        for (final Iterator<Reply<?>> it = mPendingReplies.iterator(); it.hasNext();) {
            final Reply<?> reply = it.next();
            if (reply.parser.canParse(packet)) {
                it.remove();
                reply.packet = packet;
            }
        }
    }
}
