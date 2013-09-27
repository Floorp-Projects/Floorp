/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.mozglue.GeneratableAndroidBridgeTarget;
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
public class GLController {
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
    private static final String LOGTAG = "GeckoGLController";

    private static GLController sInstance;

    private LayerView mView;
    private boolean mSurfaceValid;
    private int mWidth, mHeight;

    /* This is written by the compositor thread (while the UI thread
     * is blocked on it) and read by the UI thread. */
    private volatile boolean mCompositorCreated;

    private EGL10 mEGL;
    private EGLDisplay mEGLDisplay;
    private EGLConfig mEGLConfig;
    private EGLSurface mEGLSurface;

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
        // Here we start the GfxInfo thread, which will query OpenGL
        // system information for Gecko. This must be done early enough that the data will be
        // ready by the time it's needed to initialize the compositor (it takes about 100 ms
        // to obtain).
        GfxInfoThread.startThread();
    }

    static GLController getInstance(LayerView view) {
        if (sInstance == null) {
            sInstance = new GLController();
        }
        sInstance.mView = view;
        return sInstance;
    }

    synchronized void surfaceDestroyed() {
        ThreadUtils.assertOnUiThread();
        Log.w(LOGTAG, "GLController::surfaceDestroyed() with mCompositorCreated=" + mCompositorCreated);

        mSurfaceValid = false;
        mEGLSurface = null;

        // We need to coordinate with Gecko when pausing composition, to ensure
        // that Gecko never executes a draw event while the compositor is paused.
        // This is sent synchronously to make sure that we don't attempt to use
        // any outstanding Surfaces after we call this (such as from a
        // surfaceDestroyed notification), and to make sure that any in-flight
        // Gecko draw events have been processed.  When this returns, composition is
        // definitely paused -- it'll synchronize with the Gecko event loop, which
        // in turn will synchronize with the compositor thread.
        if (mCompositorCreated) {
            GeckoAppShell.sendEventToGeckoSync(GeckoEvent.createCompositorPauseEvent());
        }
        Log.w(LOGTAG, "done GLController::surfaceDestroyed()");
    }

    synchronized void surfaceChanged(int newWidth, int newHeight) {
        ThreadUtils.assertOnUiThread();
        Log.w(LOGTAG, "GLController::surfaceChanged(" + newWidth + ", " + newHeight + ") with mSurfaceValid=" + mSurfaceValid);

        mWidth = newWidth;
        mHeight = newHeight;

        if (mSurfaceValid) {
            // We need to make this call even when the compositor isn't currently
            // paused (e.g. during an orientation change), to make the compositor
            // aware of the changed surface.
            resumeCompositor(mWidth, mHeight);
            Log.w(LOGTAG, "done GLController::surfaceChanged with compositor resume");
            return;
        }
        mSurfaceValid = true;

        // If we get here, we supposedly have a valid surface where previously we
        // did not. So we're going to create the window surface and hold on to it
        // until the compositor comes asking for it. However, we can't call
        // eglCreateWindowSurface right away because the UI thread isn't *actually*
        // done setting up - for some reason Android will send us a surfaceChanged
        // notification before the surface is actually ready. So, we need to do the
        // call to eglCreateWindowSurface in a runnable posted back to the UI thread
        // that will run once this call unwinds all the way out and Android finishes
        // doing its thing.

        mView.post(new Runnable() {
            @Override
            public void run() {
                Log.w(LOGTAG, "GLController::surfaceChanged, creating compositor; mCompositorCreated=" + mCompositorCreated + ", mSurfaceValid=" + mSurfaceValid);
                // If we haven't yet created the compositor, and the GfxInfoThread
                // isn't done it's data gathering activities, then postpone creating
                // the compositor a little bit more. Don't block though, since this is
                // the UI thread we're running on.
                if (!mCompositorCreated && !GfxInfoThread.hasData()) {
                    mView.postDelayed(this, 1);
                    return;
                }

                try {
                    // Re-check mSurfaceValid in case the surface was destroyed between
                    // where we set it to true above and this runnable getting run.
                    // If mSurfaceValid is still true, try to create mEGLSurface. If
                    // mSurfaceValid is false, leave mEGLSurface as null. So at the end
                    // of this block mEGLSurface will be null (or EGL_NO_SURFACE) if
                    // eglCreateWindowSurface failed or if mSurfaceValid changed to false.
                    if (mSurfaceValid) {
                        if (mEGL == null) {
                            initEGL();
                        }

                        mEGLSurface = mEGL.eglCreateWindowSurface(mEGLDisplay, mEGLConfig, mView.getNativeWindow(), null);
                    }
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unable to create window surface", e);
                }
                if (mEGLSurface == null || mEGLSurface == EGL10.EGL_NO_SURFACE) {
                    mSurfaceValid = false;
                    mEGLSurface = null; // normalize EGL_NO_SURFACE to null to simplify later checks
                    Log.e(LOGTAG, "EGL window surface could not be created: " + getEGLError());
                    return;
                }
                // At this point mSurfaceValid is true and mEGLSurface is a valid surface. Try
                // to create the compositor if it hasn't been created already.
                createCompositor();
            }
        });
    }

    void createCompositor() {
        ThreadUtils.assertOnUiThread();
        Log.w(LOGTAG, "GLController::createCompositor with mCompositorCreated=" + mCompositorCreated);

        if (mCompositorCreated) {
            // If the compositor has already been created, just resume it instead. We don't need
            // to block here because if the surface is destroyed before the compositor grabs it,
            // we can handle that gracefully (i.e. the compositor will remain paused).
            resumeCompositor(mWidth, mHeight);
            Log.w(LOGTAG, "done GLController::createCompositor with compositor resume");
            return;
        }

        // Only try to create the compositor if we have a valid surface and gecko is up. When these
        // two conditions are satisfied, we can be relatively sure that the compositor creation will
        // happen without needing to block anyhwere. Do it with a sync gecko event so that the
        // android doesn't have a chance to destroy our surface in between.
        if (mEGLSurface != null && GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            GeckoAppShell.sendEventToGeckoSync(GeckoEvent.createCompositorCreateEvent(mWidth, mHeight));
        }
        Log.w(LOGTAG, "done GLController::createCompositor");
    }

    void compositorCreated() {
        Log.w(LOGTAG, "GLController::compositorCreated");
        // This is invoked on the compositor thread, while the java UI thread
        // is blocked on the gecko sync event in createCompositor() above
        mCompositorCreated = true;
    }

    public boolean hasValidSurface() {
        return mSurfaceValid;
    }

    private void initEGL() {
        mEGL = (EGL10)EGLContext.getEGL();

        mEGLDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        if (mEGLDisplay == EGL10.EGL_NO_DISPLAY) {
            throw new GLControllerException("eglGetDisplay() failed");
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

    @GeneratableAndroidBridgeTarget(allowMultithread = true, stubName = "ProvideEGLSurfaceWrapper")
    private EGLSurface provideEGLSurface() {
        return mEGLSurface;
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
