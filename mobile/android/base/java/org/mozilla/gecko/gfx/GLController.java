/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.util.Log;

/**
 * This class is a singleton that tracks EGL and compositor things over
 * the lifetime of Fennec running.
 * We only ever create one C++ compositor over Fennec's lifetime, but
 * most of the Java-side objects (e.g. LayerView, GeckoLayerClient,
 * LayerRenderer) can all get destroyed and re-created if the GeckoApp
 * activity is destroyed. This GLController is never destroyed, so that
 * the mCompositorCreated field and other state variables are always
 * accurate.
 */
public class GLController extends JNIObject {
    private static final String LOGTAG = "GeckoGLController";

    /* package */ LayerView mView;
    private boolean mServerSurfaceValid;
    private int mWidth, mHeight;

    /* This is written by the compositor thread (while the UI thread
     * is blocked on it) and read by the UI thread. */
    private volatile boolean mCompositorCreated;

    @WrapForJNI @Override // JNIObject
    protected native void disposeNative();

    // Gecko thread sets its Java instances; does not block UI thread.
    @WrapForJNI
    /* package */ native void attachToJava(GeckoLayerClient layerClient,
                                           NativePanZoomController npzc);

    @WrapForJNI
    /* package */ native void onSizeChanged(int windowWidth, int windowHeight,
                                            int screenWidth, int screenHeight);

    // Gecko thread creates compositor; blocks UI thread.
    @WrapForJNI
    private native void createCompositor(int width, int height);

    // Gecko thread pauses compositor; blocks UI thread.
    @WrapForJNI
    private native void pauseCompositor();

    // UI thread resumes compositor and notifies Gecko thread; does not block UI thread.
    @WrapForJNI
    private native void syncResumeResizeCompositor(int width, int height);

    @WrapForJNI
    private native void syncInvalidateAndScheduleComposite();

    public GLController() {
    }

    synchronized void serverSurfaceDestroyed() {
        ThreadUtils.assertOnUiThread();

        mServerSurfaceValid = false;

        // We need to coordinate with Gecko when pausing composition, to ensure
        // that Gecko never executes a draw event while the compositor is paused.
        // This is sent synchronously to make sure that we don't attempt to use
        // any outstanding Surfaces after we call this (such as from a
        // serverSurfaceDestroyed notification), and to make sure that any in-flight
        // Gecko draw events have been processed.  When this returns, composition is
        // definitely paused -- it'll synchronize with the Gecko event loop, which
        // in turn will synchronize with the compositor thread.
        if (mCompositorCreated) {
            pauseCompositor();
        }
    }

    synchronized void serverSurfaceChanged(int newWidth, int newHeight) {
        ThreadUtils.assertOnUiThread();

        mWidth = newWidth;
        mHeight = newHeight;
        mServerSurfaceValid = true;

        // we defer to a runnable the task of updating the compositor, because this is going to
        // call back into createEGLSurface, which will try to create an EGLSurface
        // against mView, which we suspect might fail if called too early. By posting this to
        // mView, we hope to ensure that it is deferred until mView is actually "ready" for some
        // sense of "ready".
        mView.post(new Runnable() {
            @Override
            public void run() {
                updateCompositor();
            }
        });
    }

    void updateCompositor() {
        ThreadUtils.assertOnUiThread();

        if (mView == null) {
            return;
        }

        if (mCompositorCreated) {
            // If the compositor has already been created, just resume it instead. We don't need
            // to block here because if the surface is destroyed before the compositor grabs it,
            // we can handle that gracefully (i.e. the compositor will remain paused).
            resumeCompositor(mWidth, mHeight);
            return;
        }

        // Only try to create the compositor if we have a valid surface and gecko is up. When these
        // two conditions are satisfied, we can be relatively sure that the compositor creation will
        // happen without needing to block anywhere. Do it with a synchronous Gecko event so that the
        // Android doesn't have a chance to destroy our surface in between.
        if (mView.getLayerClient().isGeckoReady()) {
            createCompositor(mWidth, mHeight);
        }
    }

    void compositorCreated() {
        // This is invoked on the compositor thread, while the java UI thread
        // is blocked on the gecko sync event in updateCompositor() above
        mCompositorCreated = true;
    }

    public boolean isServerSurfaceValid() {
        return mServerSurfaceValid;
    }

    @WrapForJNI(allowMultithread = true)
    private synchronized Object getSurface() {
        compositorCreated();
        if (mView != null) {
            return mView.getSurface();
        } else {
            return null;
        }
    }

    void resumeCompositor(int width, int height) {
        // Asking Gecko to resume the compositor takes too long (see
        // https://bugzilla.mozilla.org/show_bug.cgi?id=735230#c23), so we
        // resume the compositor directly. We still need to inform Gecko about
        // the compositor resuming, so that Gecko knows that it can now draw.
        // It is important to not notify Gecko until after the compositor has
        // been resumed, otherwise Gecko may send updates that get dropped.
        if (mCompositorCreated) {
            syncResumeResizeCompositor(width, height);
            mView.requestRender();
        }
    }

    /* package */ void invalidateAndScheduleComposite() {
        if (mCompositorCreated) {
            syncInvalidateAndScheduleComposite();
        }
    }

    @WrapForJNI
    private void destroy() {
        // The nsWindow has been closed. First mark our compositor as destroyed.
        mCompositorCreated = false;

        // Then clear out any pending calls on the UI thread by disposing on the UI thread.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                GLController.this.disposeNative();
            }
        });
    }

    public static class GLControllerException extends RuntimeException {
        public static final long serialVersionUID = 1L;

        GLControllerException(String e) {
            super(e);
        }
    }
}
