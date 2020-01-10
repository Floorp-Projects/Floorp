/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

/**
 * Wrapper for nsIEventTarget, enabling seamless dispatch of java runnables to
 * Gecko event queues.
 */
@WrapForJNI
public final class XPCOMEventTarget extends JNIObject {
    public void dispatch(final Runnable runnable) {
        dispatchNative(new JNIRunnable(runnable));
    }

    public static synchronized XPCOMEventTarget mainThread() {
        if (mMainThread == null) {
            mMainThread = createWrapper("main");
        }
        return mMainThread;
    }
    private static XPCOMEventTarget mMainThread = null;

    public static synchronized XPCOMEventTarget launcherThread() {
        if (mLauncherThread == null) {
            mLauncherThread = createWrapper("launcher");
        }
        return mLauncherThread;
    }
    private static XPCOMEventTarget mLauncherThread = null;

    public native boolean isOnCurrentThread();
    private native void dispatchNative(JNIRunnable runnable);
    private static native XPCOMEventTarget createWrapper(String name);

    @Override
    protected native void disposeNative();

    @WrapForJNI
    final class JNIRunnable {
        JNIRunnable(final Runnable inner) {
            mInner = inner;
        }

        @WrapForJNI
        void run() {
            mInner.run();
        }

        private Runnable mInner;
    }
}

