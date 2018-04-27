/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Base class for actors in the remote debugging protocol. Provides basic methods such as
 * {@link #sendPacket}. The actor is automatically registered with the connection on
 * creation, and its {@link onPacket} method is called whenever a packet is received with
 * the actor as the target.
 */
public class Actor {
    public final RDPConnection connection;
    public final String name;
    protected JSONObject mReply;

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

    protected JSONObject sendPacket(final String packet, final String replyProp) {
        if (packet.charAt(0) != '{') {
            throw new IllegalArgumentException();
        }
        connection.sendRawPacket("{\"to\":" + JSONObject.quote(name) + ',' + packet.substring(1));
        return getReply(replyProp);
    }

    protected JSONObject sendPacket(final JSONObject packet, final String replyProp) {
        try {
            packet.put("to", name);
            connection.sendRawPacket(packet);
            return getReply(replyProp);
        } catch (final JSONException e) {
            throw new RuntimeException(e);
        }
    }

    protected void onPacket(final JSONObject packet) {
        mReply = packet;
    }

    protected JSONObject getReply(final String replyProp) {
        mReply = null;
        do {
            connection.dispatchInputPacket();

            if (mReply != null && replyProp != null && !mReply.has(replyProp)) {
                // Out-of-band notifications not supported currently.
                mReply = null;
            }
        } while (mReply == null);
        return mReply;
    }
}
