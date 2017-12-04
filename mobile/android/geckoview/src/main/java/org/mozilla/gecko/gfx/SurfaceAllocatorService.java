/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

public class SurfaceAllocatorService extends Service {

    private static final String LOGTAG = "SurfaceAllocatorService";

    public int onStartCommand(final Intent intent, final int flags, final int startId) {
        return Service.START_STICKY;
    }

    private Binder mBinder = new ISurfaceAllocator.Stub() {
        public GeckoSurface acquireSurface(int width, int height, boolean singleBufferMode) {
            GeckoSurfaceTexture gst = GeckoSurfaceTexture.acquire(singleBufferMode);

            if (gst == null) {
                return null;
            }

            if (width > 0 && height > 0) {
                gst.setDefaultBufferSize(width, height);
            }

            return new GeckoSurface(gst);
        }

        public void releaseSurface(int handle) {
            final GeckoSurfaceTexture gst = GeckoSurfaceTexture.lookup(handle);
            if (gst != null) {
                gst.decrementUse();
            }
        }
    };

    public IBinder onBind(final Intent intent) {
        return mBinder;
    }

    public boolean onUnbind(Intent intent) {
        return false;
    }
}
