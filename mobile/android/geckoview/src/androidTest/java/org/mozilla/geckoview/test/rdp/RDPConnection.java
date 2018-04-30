/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;

/**
 * Class for connecting to a remote debugging protocol server, and retrieving various
 * actors after connection. After establishing a connection, use {@link #getMostRecentTab}
 * to get the actor for the most recent tab, which allows further interactions with the
 * tab.
 */
public final class RDPConnection implements Closeable {
    private static final String LOGTAG = "GeckoRDPConnection";

    private final LocalSocket mSocket = new LocalSocket();
    private final InputStream mInput;
    private final OutputStream mOutput;
    private final HashMap<String, Actor> mActors = new HashMap<>();
    private final Actor mRoot = new Actor(this, "root");
    private final JSONObject mRuntimeInfo;

    {
        mActors.put(mRoot.name, mRoot);
    }

    /**
     * Create a connection to a server.
     *
     * @param address Address to the remote debugging protocol socket; can be an address
     * in either the filesystem or the abstract namespace.
     */
    public RDPConnection(final LocalSocketAddress address) {
        try {
            mSocket.connect(address);
            mInput = new BufferedInputStream(mSocket.getInputStream());
            mOutput = mSocket.getOutputStream();
            mRuntimeInfo = mRoot.getReply(null);
        } catch (final IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Get the socket timeout.
     *
     * @return Socket timeout in milliseconds.
     */
    public int getTimeout() {
        try {
            return mSocket.getSoTimeout();
        } catch (final IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Set the socket timeout. IOException is thrown if the timeout expires while waiting
     * for a socket operation.
     */
    public void setTimeout(final int timeout) {
        try {
            mSocket.setSoTimeout(timeout);
        } catch (final IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Close the server connection.
     */
    @Override
    public void close() {
        try {
            mOutput.close();
            mSocket.shutdownOutput();
            mInput.close();
            mSocket.shutdownInput();
            mSocket.close();
        } catch (final IOException e) {
            throw new RuntimeException(e);
        }
    }

    /* package */ void addActor(final String name, final Actor actor) {
        mActors.put(name, actor);
    }

    /* package */ void removeActor(final String name) {
        mActors.remove(name);
    }

    /* package */ Actor getActor(final JSONObject packet) {
        return mActors.get(packet.optString("actor", null));
    }

    /* package */ Actor getActor(final String name) {
        return mActors.get(name);
    }

    /* package */ void sendRawPacket(final JSONObject packet) {
        sendRawPacket(packet.toString());
    }

    /* package */ void sendRawPacket(final String packet) {
        try {
            final byte[] buffer = packet.getBytes("utf-8");
            final byte[] header = (String.valueOf(buffer.length) + ':').getBytes("utf-8");
            mOutput.write(header);
            mOutput.write(buffer);
        } catch (final IOException e) {
            throw new RuntimeException(e);
        }
    }

    /* package */ void dispatchInputPacket() {
        try {
            byte[] buffer = new byte[128];
            int len = 0;
            for (int c = mInput.read(); c != ':'; c = mInput.read()) {
                if (c == -1) {
                    throw new IllegalStateException("EOF reached");
                }
                buffer[len++] = (byte) c;
            }

            final String header = new String(buffer, 0, len, "utf-8");
            final int length;
            try {
                length = Integer.valueOf(header.substring(header.lastIndexOf(' ') + 1));
            } catch (final NumberFormatException e) {
                throw new RuntimeException("Invalid packet header: " + header);
            }

            if (header.startsWith("bulk ")) {
                // Bulk packet not supported; skip the data.
                mInput.skip(length);
                return;
            }

            // JSON packet.
            if (length > buffer.length) {
                buffer = new byte[length];
            }
            int cursor = 0;
            do {
                final int read = mInput.read(buffer, cursor, length - cursor);
                if (read <= 0) {
                    throw new IllegalStateException("EOF reached");
                }
                cursor += read;
            } while (cursor < length);

            final String str = new String(buffer, 0, length, "utf-8");
            final JSONObject json;
            try {
                json = new JSONObject(str);
            } catch (final JSONException e) {
                throw new RuntimeException(e);
            }

            final String error = json.optString("error", null);
            if (error != null) {
                throw new UnsupportedOperationException("Request failed: " + error);
            }

            final String from = json.optString("from", "none");
            final Actor actor = mActors.get(from);
            if (actor != null) {
                actor.onPacket(json);
            } else {
                Log.w(LOGTAG, "Packet from unknown actor " + from);
            }
        } catch (final IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Get the actor for the most recent tab. For GeckoView, this tab represents the most
     * recent GeckoSession.
     *
     * @return Tab actor.
     */
    public Tab getMostRecentTab() {
        final JSONObject reply = mRoot.sendPacket("{\"type\":\"getTab\"}", "tab")
                                      .optJSONObject("tab");
        final Actor actor = getActor(reply);
        return (actor != null) ? (Tab) actor : new Tab(this, reply);
    }
}
