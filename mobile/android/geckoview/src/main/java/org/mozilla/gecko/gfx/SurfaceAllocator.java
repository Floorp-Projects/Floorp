/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;

import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;

/* package */ final class SurfaceAllocator {
    private static final String LOGTAG = "SurfaceAllocator";

    private static SurfaceAllocatorConnection sConnection;

    private static synchronized void ensureConnection() throws Exception {
        if (sConnection != null) {
            return;
        }

        sConnection = new SurfaceAllocatorConnection();
        final Intent intent = new Intent();
        intent.setClassName(GeckoAppShell.getApplicationContext(),
                            "org.mozilla.gecko.gfx.SurfaceAllocatorService");

        // FIXME: may not want to auto create
        if (!GeckoAppShell.getApplicationContext().bindService(intent, sConnection, Context.BIND_AUTO_CREATE)) {
            throw new Exception("Failed to connect to surface allocator service!");
        }
    }

    @WrapForJNI
    public static GeckoSurface acquireSurface(final int width, final int height,
                                              final boolean singleBufferMode) {
        try {
            ensureConnection();

            if (singleBufferMode && !GeckoSurfaceTexture.isSingleBufferSupported()) {
                return null;
            }
            final ISurfaceAllocator allocator = sConnection.getAllocator();
            final GeckoSurface surface = allocator.acquireSurface(width, height, singleBufferMode);
            if (surface != null && !surface.inProcess()) {
                allocator.configureSync(surface.initSyncSurface(width, height));
            }
            return surface;
        } catch (final Exception e) {
            Log.w(LOGTAG, "Failed to acquire GeckoSurface", e);
            return null;
        }
    }

    @WrapForJNI
    public static void disposeSurface(final GeckoSurface surface) {
        try {
            ensureConnection();
        } catch (final Exception e) {
            Log.w(LOGTAG, "Failed to dispose surface, no connection");
            return;
        }

        // Release the SurfaceTexture on the other side
        try {
            sConnection.getAllocator().releaseSurface(surface.getHandle());
        } catch (final RemoteException e) {
            Log.w(LOGTAG, "Failed to release surface texture", e);
        }

        // And now our Surface
        try {
            surface.release();
        } catch (final Exception e) {
            Log.w(LOGTAG, "Failed to release surface", e);
        }
    }

    public static void sync(final int upstream) {
        try {
            ensureConnection();
        } catch (final Exception e) {
            Log.w(LOGTAG, "Failed to sync texture, no connection");
            return;
        }

        // Release the SurfaceTexture on the other side
        try {
            sConnection.getAllocator().sync(upstream);
        } catch (final RemoteException e) {
            Log.w(LOGTAG, "Failed to sync texture", e);
        }
    }

    private static final class SurfaceAllocatorConnection implements ServiceConnection {
        private ISurfaceAllocator mAllocator;

        public synchronized ISurfaceAllocator getAllocator() {
            while (mAllocator == null) {
                try {
                    this.wait();
                } catch (final InterruptedException e) { }
            }

            return mAllocator;
        }

        @Override
        public synchronized void onServiceConnected(final ComponentName name,
                                                    final IBinder service) {
            mAllocator = ISurfaceAllocator.Stub.asInterface(service);
            this.notifyAll();
        }

        @Override
        public synchronized void onServiceDisconnected(final ComponentName name) {
            mAllocator = null;
        }
    }
}
