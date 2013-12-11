/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;
import org.mozilla.gecko.util.ThreadUtils;

import android.util.Log;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

/**
 * EGLPreloadingThread is purely a preloading optimization, not something
 * we rely on for anything else than performance. We will be initializing
 * EGL in GLController::initEGL() when we need it, but having EGL initialization
 * already previously done by EGLPreloadingThread::run() will make it much
 * faster for GLController to do again.
 *
 * For example, here are some timings recorded on two devices:
 *
 * Device                 | EGLPreloadingThread::run() | GLController::initEGL()
 * -----------------------+----------------------------+------------------------
 * Nexus S (Android 2.3)  | ~ 80 ms                    | < 0.1 ms
 * Nexus 10 (Android 4.3) | ~ 35 ms                    | < 0.1 ms
 */
class EGLPreloadingThread extends Thread
{
    private static final String LOGTAG = "EGLPreloadingThread";
    private EGL10 mEGL;
    private EGLDisplay mEGLDisplay;

    public EGLPreloadingThread()
    {
    }

    @Override
    public void run()
    {
        mEGL = (EGL10)EGLContext.getEGL();
        mEGLDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        if (mEGLDisplay == EGL10.EGL_NO_DISPLAY) {
            Log.w(LOGTAG, "Can't get EGL display!");
            return;
        }

        int[] returnedVersion = new int[2];
        if (!mEGL.eglInitialize(mEGLDisplay, returnedVersion)) {
            Log.w(LOGTAG, "eglInitialize failed");
            return;
        }
    }
}

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
public class GLController {
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
    private static final String LOGTAG = "GeckoGLController";

    private static GLController sInstance;

    private LayerView mView;
    private boolean mServerSurfaceValid;
    private int mWidth, mHeight;

    /* This is written by the compositor thread (while the UI thread
     * is blocked on it) and read by the UI thread. */
    private volatile boolean mCompositorCreated;

    private EGL10 mEGL;
    private EGLDisplay mEGLDisplay;
    private EGLConfig mEGLConfig;
    private EGLPreloadingThread mEGLPreloadingThread;
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

    private GLController() {
        mEGLPreloadingThread = new EGLPreloadingThread();
        mEGLPreloadingThread.start();
    }

    static GLController getInstance(LayerView view) {
        if (sInstance == null) {
            sInstance = new GLController();
        }
        sInstance.mView = view;
        return sInstance;
    }

    synchronized void serverSurfaceDestroyed() {
        ThreadUtils.assertOnUiThread();
        Log.w(LOGTAG, "GLController::serverSurfaceDestroyed() with mCompositorCreated=" + mCompositorCreated);

        mServerSurfaceValid = false;

        if (mEGLSurfaceForCompositor != null) {
          mEGL.eglDestroySurface(mEGLDisplay, mEGLSurfaceForCompositor);
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
            GeckoAppShell.sendEventToGeckoSync(GeckoEvent.createCompositorPauseEvent());
        }
        Log.w(LOGTAG, "done GLController::serverSurfaceDestroyed()");
    }

    synchronized void serverSurfaceChanged(int newWidth, int newHeight) {
        ThreadUtils.assertOnUiThread();
        Log.w(LOGTAG, "GLController::serverSurfaceChanged(" + newWidth + ", " + newHeight + ")");

        mWidth = newWidth;
        mHeight = newHeight;
        mServerSurfaceValid = true;

        // we defer to a runnable the task of updating the compositor, because this is going to
        // call back into createEGLSurfaceForCompositor, which will try to create an EGLSurface
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
        Log.w(LOGTAG, "GLController::updateCompositor with mCompositorCreated=" + mCompositorCreated);

        if (mCompositorCreated) {
            // If the compositor has already been created, just resume it instead. We don't need
            // to block here because if the surface is destroyed before the compositor grabs it,
            // we can handle that gracefully (i.e. the compositor will remain paused).
            resumeCompositor(mWidth, mHeight);
            Log.w(LOGTAG, "done GLController::updateCompositor with compositor resume");
            return;
        }

        if (!AttemptPreallocateEGLSurfaceForCompositor()) {
            return;
        }

        // Only try to create the compositor if we have a valid surface and gecko is up. When these
        // two conditions are satisfied, we can be relatively sure that the compositor creation will
        // happen without needing to block anyhwere. Do it with a sync gecko event so that the
        // android doesn't have a chance to destroy our surface in between.
        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            GeckoAppShell.sendEventToGeckoSync(GeckoEvent.createCompositorCreateEvent(mWidth, mHeight));
        }
        Log.w(LOGTAG, "done GLController::updateCompositor");
    }

    void compositorCreated() {
        Log.w(LOGTAG, "GLController::compositorCreated");
        // This is invoked on the compositor thread, while the java UI thread
        // is blocked on the gecko sync event in updateCompositor() above
        mCompositorCreated = true;
    }

    public boolean isServerSurfaceValid() {
        return mServerSurfaceValid;
    }

    public boolean isCompositorCreated() {
        return mCompositorCreated;
    }

    private void initEGL() {
        if (mEGL != null) {
            return;
        }

        // This join() should not be necessary, but makes this code a bit easier to think about.
        // The EGLPreloadingThread should long be done by now, and even if it's not,
        // it shouldn't be a problem to be initalizing EGL from two different threads.
        // Still, having this join() here means that we don't have to wonder about what
        // kind of caveats might exist with EGL initialization reentrancy on various drivers.
        try {
            mEGLPreloadingThread.join();
        } catch (InterruptedException e) {
            Log.w(LOGTAG, "EGLPreloadingThread interrupted", e);
        }

        mEGL = (EGL10)EGLContext.getEGL();

        mEGLDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        if (mEGLDisplay == EGL10.EGL_NO_DISPLAY) {
            Log.w(LOGTAG, "Can't get EGL display!");
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
        if (!mEGL.eglInitialize(mEGLDisplay, returnedVersion)) {
            Log.w(LOGTAG, "eglInitialize failed");
            return;
        }

        mEGLConfig = chooseConfig();
    }

    private EGLConfig chooseConfig() {
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

        if (!mEGL.eglChooseConfig(mEGLDisplay, desiredConfig, null, 0, numConfigs) ||
                numConfigs[0] <= 0) {
            throw new GLControllerException("No available EGL configurations " +
                                            getEGLError());
        }

        EGLConfig[] configs = new EGLConfig[numConfigs[0]];
        if (!mEGL.eglChooseConfig(mEGLDisplay, desiredConfig, configs, numConfigs[0], numConfigs)) {
            throw new GLControllerException("No EGL configuration for that specification " +
                                            getEGLError());
        }

        // Select the first configuration that matches the screen depth.
        int[] red = new int[1], green = new int[1], blue = new int[1];
        for (EGLConfig config : configs) {
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_RED_SIZE, red);
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_GREEN_SIZE, green);
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_BLUE_SIZE, blue);
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
                mEGLSurfaceForCompositor = mEGL.eglCreateWindowSurface(mEGLDisplay, mEGLConfig, mView.getNativeWindow(), null);
            } catch (Exception e) {
                Log.e(LOGTAG, "eglCreateWindowSurface threw", e);
            }
        }
        return mEGLSurfaceForCompositor != null;
    }

    @WrapElementForJNI(allowMultithread = true, stubName = "CreateEGLSurfaceForCompositorWrapper")
    private synchronized EGLSurface createEGLSurfaceForCompositor() {
        AttemptPreallocateEGLSurfaceForCompositor();
        EGLSurface result = mEGLSurfaceForCompositor;
        mEGLSurfaceForCompositor = null;
        return result;
    }

    private String getEGLError() {
        return "Error " + (mEGL == null ? "(no mEGL)" : mEGL.eglGetError());
    }

    void resumeCompositor(int width, int height) {
        Log.w(LOGTAG, "GLController::resumeCompositor(" + width + ", " + height + ") and mCompositorCreated=" + mCompositorCreated);
        // Asking Gecko to resume the compositor takes too long (see
        // https://bugzilla.mozilla.org/show_bug.cgi?id=735230#c23), so we
        // resume the compositor directly. We still need to inform Gecko about
        // the compositor resuming, so that Gecko knows that it can now draw.
        // It is important to not notify Gecko until after the compositor has
        // been resumed, otherwise Gecko may send updates that get dropped.
        if (mCompositorCreated) {
            GeckoAppShell.scheduleResumeComposition(width, height);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createCompositorResumeEvent());
        }
        Log.w(LOGTAG, "done GLController::resumeCompositor");
    }

    public static class GLControllerException extends RuntimeException {
        public static final long serialVersionUID = 1L;

        GLControllerException(String e) {
            super(e);
        }
    }
}
