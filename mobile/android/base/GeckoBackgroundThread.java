/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Handler;
import android.os.Looper;

import java.util.concurrent.SynchronousQueue;

public final class GeckoBackgroundThread extends Thread {
    private static final String LOOPER_NAME = "GeckoBackgroundThread";

    // Guarded by 'this'.
    private static Handler sHandler = null;
    private SynchronousQueue<Handler> mHandlerQueue = new SynchronousQueue<Handler>();

    // Singleton, so private constructor.
    private GeckoBackgroundThread() {
        super();
    }

    public void run() {
        setName(LOOPER_NAME);
        Looper.prepare();
        try {
            mHandlerQueue.put(new Handler());
        } catch (InterruptedException ie) {}

        Looper.loop();
    }

    // Get a Handler for a looper thread, or create one if it doesn't yet exist.
    public static synchronized Handler getHandler() {
        if (sHandler == null) {
          GeckoBackgroundThread lt = new GeckoBackgroundThread();
          lt.start();
          try {
              sHandler = lt.mHandlerQueue.take();
          } catch (InterruptedException ie) {}
        }
        return sHandler;
    }

    public static void post(Runnable runnable) {
        Handler handler = getHandler();
        if (handler == null) {
            throw new IllegalStateException("No handler! Must have been interrupted. Not posting.");
        }
        handler.post(runnable);
    }
}
