/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Class for connecting to a remote debugging protocol server, and retrieving various
 * actors after connection. After establishing a connection, use {@link #getMostRecentTab}
 * to get the actor for the most recent tab, which allows further interactions with the
 * tab.
 */
public final class RDPConnection implements Closeable {
    private static final String LOGTAG = "GeckoRDPConnection";

    public static class TimeoutException extends RuntimeException {
        public TimeoutException(final String detailMessage) {
            super(detailMessage);
        }
    }

    private final class InputThread extends Thread {
        private final BlockingQueue<Object> mQueue = new SynchronousQueue<>();
        private final Handler mHandler = new Handler();
        private final InputStream mStream;
        /* package */ final AtomicInteger mPendingPackets = new AtomicInteger();

        private final Runnable mProcessPacketRunnable = new Runnable() {
            @Override
            public void run() {
                final int pending = mPendingPackets.get();
                if (pending > 0) {
                    InputThread.this.processPacket();
                }
            }
        };

        public long timeout;

        public InputThread(final InputStream stream) {
            super("RDPInput");
            setDaemon(true);
            mStream = stream;
        }

        @Override
        public void run() {
            boolean quit = false;
            do {
                Object result = null;
                try {
                    result = readInputPacket();
                } catch (final IOException e) {
                    if (RDPConnection.this.mSocket.isClosed()) {
                        quit = true;
                    } else {
                        result = e;
                    }
                }
                if (result == null) {
                    continue;
                }
                mPendingPackets.incrementAndGet();
                mHandler.post(mProcessPacketRunnable);
                do {
                    try {
                        mQueue.put(result);
                        break;
                    } catch (final InterruptedException e) {
                        // Ignore
                    }
                    quit = RDPConnection.this.mSocket.isClosed();
                } while (!quit);
            } while (!quit);
        }

        public void processPacket() {
            if (!mHandler.getLooper().equals(Looper.myLooper())) {
                throw new AssertionError("Must be called from single thread");
            }

            Object result = null;
            do {
                try {
                    if (timeout <= 0) {
                        result = mQueue.take();
                    } else {
                        result = mQueue.poll(timeout, TimeUnit.MILLISECONDS);
                    }
                    if (result == null) {
                        throw new TimeoutException("Socket timeout");
                    }
                } catch (final InterruptedException e) {
                    // Ignore
                }
            } while (result == null);

            mPendingPackets.decrementAndGet();

            if (result instanceof Throwable) {
                final Throwable throwable = (Throwable) result;
                throw new RuntimeException(throwable.getMessage(), throwable);
            }

            final JSONObject json = (JSONObject) result;
            final String from = json.optString("from", "none");
            final Actor actor = RDPConnection.this.mActors.get(from);
            if (actor != null) {
                actor.onPacket(json);
            } else {
                Log.w(LOGTAG, "Packet from unknown actor " + from);
            }
        }

        private Object readInputPacket() throws IOException {
            byte[] buffer = new byte[128];
            int len = 0;
            for (int c = mStream.read(); c != ':'; c = mStream.read()) {
                if (c == -1) {
                    return new IllegalStateException("EOF reached");
                }
                buffer[len++] = (byte) c;
            }

            final String header = new String(buffer, 0, len, "utf-8");
            final int length;
            try {
                length = Integer.valueOf(header.substring(header.lastIndexOf(' ') + 1));
            } catch (final NumberFormatException e) {
                return new IllegalStateException("Invalid packet header: " + header);
            }

            if (header.startsWith("bulk ")) {
                // Bulk packet not supported; skip the data.
                int remaining = length;
                while (remaining > 0) {
                    remaining -= mStream.skip(remaining);
                }
                return null;
            }

            // JSON packet.
            if (length > buffer.length) {
                buffer = new byte[length];
            }
            int cursor = 0;
            do {
                final int read = mStream.read(buffer, cursor, length - cursor);
                if (read <= 0) {
                    return new IllegalStateException("EOF reached");
                }
                cursor += read;
            } while (cursor < length);

            final String str = new String(buffer, 0, length, "utf-8");
            final JSONObject json;
            try {
                json = new JSONObject(str);
            } catch (final JSONException e) {
                return e;
            }

            final String error = json.optString("error", null);
            if (error != null) {
                return new Exception(error + ": " + json.optString("message", null));
            }
            return json;
        }
    }

    private final Actor.ReplyParser<Tab> TAB_PARSER = new Actor.ReplyParser<Tab>() {
        @Override
        public boolean canParse(@NonNull JSONObject packet) {
            return packet.has("tab");
        }

        @Override
        public @NonNull Tab parse(@NonNull JSONObject packet) {
            final JSONObject tab = packet.optJSONObject("tab");
            final Actor actor = getActor(tab);
            return (actor != null) ? (Tab) actor : new Tab(RDPConnection.this, tab);
        }
    };

    private final Actor.ReplyParser<Tab> PROCESS_PARSER = new Actor.ReplyParser<Tab>() {
        @Override
        public boolean canParse(@NonNull JSONObject packet) {
            return packet.has("form");
        }

        @Override
        public @NonNull Tab parse(@NonNull JSONObject packet) {
            final JSONObject tab = packet.optJSONObject("form");
            final Actor actor = getActor(tab);
            return (actor != null) ? (Tab) actor : new Tab(RDPConnection.this, tab);
        }
    };

    private final LocalSocket mSocket = new LocalSocket();
    private final InputThread mInputThread;
    private final OutputStream mOutputStream;
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
            mOutputStream = mSocket.getOutputStream();
            mInputThread = new InputThread(new BufferedInputStream(mSocket.getInputStream()));
            mInputThread.start();

            mRuntimeInfo = mRoot.addReply(Actor.JSON_PARSER).get();
        } catch (final IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Get the socket timeout.
     *
     * @return Socket timeout in milliseconds, or 0 if there is no timeout.
     */
    public long getTimeout() {
        return mInputThread.timeout;
    }

    /**
     * Set the socket timeout.
     *
     * @param timeout Timeout in milliseconds, or 0 to disable timeout.
     */
    public void setTimeout(final long timeout) {
        mInputThread.timeout = timeout;
    }

    /**
     * Close the server connection.
     */
    @Override
    public void close() {
        try {
            mSocket.close();
            mInputThread.interrupt();
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
            mOutputStream.write(header);
            mOutputStream.write(buffer);
        } catch (final IOException e) {
            throw new RuntimeException(e);
        }
    }

    /* package */ void processInputPacket() {
        mInputThread.processPacket();
    }

    /**
     * Get the actor for the most recent tab. For GeckoView, this tab represents the most
     * recent GeckoSession.
     *
     * @return Tab actor.
     */
    public Tab getMostRecentTab() {
        return mRoot.sendPacket("{\"type\":\"getTab\"}", TAB_PARSER).get();
    }

    /**
     * Get the actor for the chrome process. The returned Tab object acts like a tab but has
     * chrome privileges.
     *
     * @return Tab actor representing the process.
     */
    public Tab getChromeProcess() {
        return mRoot.sendPacket("{\"type\":\"getProcess\"}", PROCESS_PARSER).get();
    }
}
