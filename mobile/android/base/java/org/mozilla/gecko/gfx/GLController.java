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

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

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
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
    private static final String LOGTAG = "GeckoGLController";

    /* package */ LayerView mView;
    private boolean mServerSurfaceValid;
    private int mWidth, mHeight;

    /* This is written by the compositor thread (while the UI thread
     * is blocked on it) and read by the UI thread. */
    private volatile boolean mCompositorCreated;

    private static EGL10 sEGL;
    private static EGLDisplay sEGLDisplay;
    private static EGLConfig sEGLConfig;
    private EGLSurface mEGLSurfaceForCompositor;

    private static final int LOCAL_EGL_OPENGL_ES2_BIT = 4;

    private static final int[] CONFIG_SPEC_16BPP = {
        EGL10.EGL_RED_SIZE, 5,
        EGL10.EGL_GREEN_SIZE, 6,
        EGL10.EGL_BLUE_SIZE, 5,
        EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,
        EGL10.EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
        EGL10.EGL_NONE
    };

    private static final int[] CONFIG_SPEC_24BPP = {
        EGL10.EGL_RED_SIZE, 8,
        EGL10.EGL_GREEN_SIZE, 8,
        EGL10.EGL_BLUE_SIZE, 8,
        EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,
        EGL10.EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
        EGL10.EGL_NONE
    };

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

        if (mEGLSurfaceForCompositor != null) {
          sEGL.eglDestroySurface(sEGLDisplay, mEGLSurfaceForCompositor);
          mEGLSurfaceForCompositor = null;
        }

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

        if (!AttemptPreallocateEGLSurfaceForCompositor()) {
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

    private static void initEGL() {
        if (sEGL != null) {
            return;
        }

        sEGL = (EGL10)EGLContext.getEGL();

        sEGLDisplay = sEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        if (sEGLDisplay == EGL10.EGL_NO_DISPLAY) {
            Log.w(LOGTAG, "can't get EGL display!");
            return;
        }

        // while calling eglInitialize here should not be necessary as it was already called
        // by the EGLPreloadingThread, it really doesn't cost much to call it again here,
        // and makes this code easier to think about: EGLPreloadingThread is only a
        // preloading optimization, not something we rely on for anything else.
        //
        // Also note that while calling eglInitialize isn't necessary on Android 4.x
        // (at least Android's HardwareRenderer does it for us already), it is necessary
        // on Android 2.x.
        int[] returnedVersion = new int[2];
        if (!sEGL.eglInitialize(sEGLDisplay, returnedVersion)) {
            Log.w(LOGTAG, "eglInitialize failed");
            return;
        }

        sEGLConfig = chooseConfig();
    }

    private static EGLConfig chooseConfig() {
        int[] desiredConfig;
        int rSize, gSize, bSize;
        int[] numConfigs = new int[1];

        switch (GeckoAppShell.getScreenDepth()) {
        case 24:
            desiredConfig = CONFIG_SPEC_24BPP;
            rSize = gSize = bSize = 8;
            break;
        case 16:
        default:
            desiredConfig = CONFIG_SPEC_16BPP;
            rSize = 5; gSize = 6; bSize = 5;
            break;
        }

        if (!sEGL.eglChooseConfig(sEGLDisplay, desiredConfig, null, 0, numConfigs) ||
                numConfigs[0] <= 0) {
            throw new GLControllerException("No available EGL configurations " +
                                            getEGLError());
        }

        EGLConfig[] configs = new EGLConfig[numConfigs[0]];
        if (!sEGL.eglChooseConfig(sEGLDisplay, desiredConfig, configs, numConfigs[0], numConfigs)) {
            throw new GLControllerException("No EGL configuration for that specification " +
                                            getEGLError());
        }

        // Select the first configuration that matches the screen depth.
        int[] red = new int[1], green = new int[1], blue = new int[1];
        for (EGLConfig config : configs) {
            sEGL.eglGetConfigAttrib(sEGLDisplay, config, EGL10.EGL_RED_SIZE, red);
            sEGL.eglGetConfigAttrib(sEGLDisplay, config, EGL10.EGL_GREEN_SIZE, green);
            sEGL.eglGetConfigAttrib(sEGLDisplay, config, EGL10.EGL_BLUE_SIZE, blue);
            if (red[0] == rSize && green[0] == gSize && blue[0] == bSize) {
                return config;
            }
        }

        throw new GLControllerException("No suitable EGL configuration found");
    }

    private synchronized boolean AttemptPreallocateEGLSurfaceForCompositor() {
        if (mEGLSurfaceForCompositor == null) {
            initEGL();
            try {
                mEGLSurfaceForCompositor = sEGL.eglCreateWindowSurface(sEGLDisplay, sEGLConfig, mView.getNativeWindow(), null);
                // In failure cases, eglCreateWindowSurface should return EGL_NO_SURFACE.
                // We currently normalize this to null, and compare to null in all our checks.
                if (mEGLSurfaceForCompositor == EGL10.EGL_NO_SURFACE) {
                    mEGLSurfaceForCompositor = null;
                }
            } catch (Exception e) {
                Log.e(LOGTAG, "eglCreateWindowSurface threw", e);
            }
        }
        if (mEGLSurfaceForCompositor == null) {
            Log.w(LOGTAG, "eglCreateWindowSurface returned no surface!");
        }
        return mEGLSurfaceForCompositor != null;
    }

    @WrapForJNI(allowMultithread = true)
    private synchronized EGLSurface createEGLSurface() {
        compositorCreated();
        AttemptPreallocateEGLSurfaceForCompositor();
        EGLSurface result = mEGLSurfaceForCompositor;
        mEGLSurfaceForCompositor = null;
        return result;
    }

    private static String getEGLError() {
        return "Error " + (sEGL == null ? "(no sEGL)" : sEGL.eglGetError());
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
