/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

public class GLController {
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
    private static final String LOGTAG = "GeckoGLController";

    private LayerView mView;
    private boolean mSurfaceValid;
    private int mWidth, mHeight;

    private EGL10 mEGL;
    private EGLDisplay mEGLDisplay;
    private EGLConfig mEGLConfig;

    private static final int LOCAL_EGL_OPENGL_ES2_BIT = 4;

    private static final int[] CONFIG_SPEC = {
        EGL10.EGL_RED_SIZE, 5,
        EGL10.EGL_GREEN_SIZE, 6,
        EGL10.EGL_BLUE_SIZE, 5,
        EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,
        EGL10.EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
        EGL10.EGL_NONE
    };

    GLController(LayerView view) {
        mView = view;
        mSurfaceValid = false;
    }

    /* This function is invoked by JNI */
    void resumeCompositorIfValid() {
        synchronized (this) {
            if (!mSurfaceValid) {
                return;
            }
        }
        mView.getListener().compositionResumeRequested(mWidth, mHeight);
    }

    /* Wait until we are allowed to use EGL functions on the Surface backing
     * this window.
     * This function is invoked by JNI */
    synchronized void waitForValidSurface() {
        while (!mSurfaceValid) {
            try {
                wait();
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    synchronized void surfaceDestroyed() {
        mSurfaceValid = false;
        notifyAll();
    }

    synchronized void surfaceChanged(int newWidth, int newHeight) {
        mWidth = newWidth;
        mHeight = newHeight;
        mSurfaceValid = true;
        notifyAll();
    }

    private void initEGL() {
        mEGL = (EGL10)EGLContext.getEGL();

        mEGLDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        if (mEGLDisplay == EGL10.EGL_NO_DISPLAY) {
            throw new GLControllerException("eglGetDisplay() failed");
        }

        mEGLConfig = chooseConfig();

        // updating the state in the view/controller/client should be
        // done on the main UI thread, not the GL renderer thread
        mView.post(new Runnable() {
            public void run() {
                mView.setViewportSize(mWidth, mHeight);
            }
        });
    }

    private EGLConfig chooseConfig() {
        int[] numConfigs = new int[1];
        if (!mEGL.eglChooseConfig(mEGLDisplay, CONFIG_SPEC, null, 0, numConfigs) ||
                numConfigs[0] <= 0) {
            throw new GLControllerException("No available EGL configurations " +
                                            getEGLError());
        }

        EGLConfig[] configs = new EGLConfig[numConfigs[0]];
        if (!mEGL.eglChooseConfig(mEGLDisplay, CONFIG_SPEC, configs, numConfigs[0], numConfigs)) {
            throw new GLControllerException("No EGL configuration for that specification " +
                                            getEGLError());
        }

        // Select the first 565 RGB configuration.
        int[] red = new int[1], green = new int[1], blue = new int[1];
        for (EGLConfig config : configs) {
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_RED_SIZE, red);
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_GREEN_SIZE, green);
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_BLUE_SIZE, blue);
            if (red[0] == 5 && green[0] == 6 && blue[0] == 5) {
                return config;
            }
        }

        throw new GLControllerException("No suitable EGL configuration found");
    }

    /**
     * Provides an EGLSurface without assuming ownership of this surface.
     * This class does not keep a reference to the provided EGL surface; the
     * caller assumes ownership of the surface once it is returned.
     * This function is invoked by JNI */
    private EGLSurface provideEGLSurface() {
        if (mEGL == null) {
            initEGL();
        }

        Object window = mView.getNativeWindow();
        EGLSurface surface = mEGL.eglCreateWindowSurface(mEGLDisplay, mEGLConfig, window, null);
        if (surface == null || surface == EGL10.EGL_NO_SURFACE) {
            throw new GLControllerException("EGL window surface could not be created! " +
                                            getEGLError());
        }

        return surface;
    }

    private String getEGLError() {
        return "Error " + mEGL.eglGetError();
    }

    public static class GLControllerException extends RuntimeException {
        public static final long serialVersionUID = 1L;

        GLControllerException(String e) {
            super(e);
        }
    }
}

