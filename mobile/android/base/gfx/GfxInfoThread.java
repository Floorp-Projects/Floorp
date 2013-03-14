/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.opengl.GLES20;
import android.util.Log;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

public class GfxInfoThread extends Thread {
    private static final String LOGTAG = "GfxInfoThread";

    private static GfxInfoThread sInstance;
    private String mData;

    private GfxInfoThread() {
    }

    public static void startThread() {
        if (sInstance == null) {
            sInstance = new GfxInfoThread();
            sInstance.start();
        }
    }

    public static boolean hasData() {
        // This should never be called before startThread(), so if
        // sInstance is null here, then we know the thread was created,
        // ran to completion, and getData() was called. Therefore hasData()
        // should return true. If sInstance is not null, then we need to
        // check if the mData field on it is null or not and return accordingly.
        // Note that we keep a local copy of sInstance to avoid race conditions
        // as getData() may be called concurrently.
        GfxInfoThread instance = sInstance;
        if (instance == null) {
            return true;
        }
        synchronized (instance) {
            return instance.mData != null;
        }
    }

    public static String getData() {
        // This should be called exactly once after startThread(), so we
        // know sInstance will be non-null here
        String data = sInstance.getDataImpl();
        sInstance = null;
        return data;
    }

    private synchronized void error(String msg) {
        Log.e(LOGTAG, msg);
        mData = "ERROR\n" + msg + "\n";
        notifyAll();
    }

    private void eglError(EGL10 egl, String msg) {
        error(msg + " (EGL error " + Integer.toHexString(egl.eglGetError()) + ")");
    }

    private synchronized String getDataImpl() {
        if (mData != null) {
            return mData;
        }

        Log.w(LOGTAG, "We need the GfxInfo data, but it is not yet available. " +
                      "We have to wait for it, so expect abnormally long startup times. " +
                      "Please report a Mozilla bug.");

        try {
            while (mData == null) {
                wait();
            }
        } catch (InterruptedException e) {
            Log.w(LOGTAG, "Thread interrupted", e);
            Thread.currentThread().interrupt();
        }
        Log.i(LOGTAG, "GfxInfo data is finally available.");
        return mData;
    }

    @Override
    public void run() {
        // initialize EGL
        EGL10 egl = (EGL10) EGLContext.getEGL();
        EGLDisplay eglDisplay = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);

        if (eglDisplay == EGL10.EGL_NO_DISPLAY) {
            eglError(egl, "eglGetDisplay failed");
            return;
        }

        int[] returnedVersion = new int[2];
        if (!egl.eglInitialize(eglDisplay, returnedVersion)) {
            eglError(egl, "eglInitialize failed");
            return;
        }

        // query number of configs
        int[] returnedNumberOfConfigs = new int[1];
        int EGL_OPENGL_ES2_BIT = 4;
        int[] configAttribs = new int[] {
            EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL10.EGL_NONE
        };

        String noES2SupportMsg = "Maybe this device does not support OpenGL ES2?";

        if (!egl.eglChooseConfig(eglDisplay,
                                 configAttribs,
                                 null,
                                 0,
                                 returnedNumberOfConfigs))
        {
            eglError(egl, "eglChooseConfig failed to query OpenGL ES2 configs. " +
                          noES2SupportMsg);
            return;
        }

        // get the first config
        int numConfigs = returnedNumberOfConfigs[0];
        if (numConfigs == 0) {
            error("eglChooseConfig returned zero OpenGL ES2 configs. " +
                  noES2SupportMsg);
            return;
        }

        EGLConfig[] returnedConfigs = new EGLConfig[numConfigs];
        if (!egl.eglChooseConfig(eglDisplay,
                                 configAttribs,
                                 returnedConfigs,
                                 numConfigs,
                                 returnedNumberOfConfigs))
        {
            eglError(egl, "eglChooseConfig failed (listing OpenGL ES2 configs). " +
                          noES2SupportMsg);
            return;
        }

        EGLConfig eglConfig = returnedConfigs[0];

        // create a ES 2.0 context
        int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
        int[] contextAttribs = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };
        EGLContext eglContext = egl.eglCreateContext(eglDisplay,
                                                     eglConfig,
                                                     EGL10.EGL_NO_CONTEXT,
                                                     contextAttribs);
        if (eglContext == EGL10.EGL_NO_CONTEXT) {
            eglError(egl, "eglCreateContext failed to create a OpenGL ES2 context" +
                          noES2SupportMsg);
            return;
        }

        // create a surface, necessary to make the context current. Hopefully PBuffers
        // are well supported enough. Are there other kinds of off-screen surfaces in
        // Android EGL anyway?
        int[] surfaceAttribs = new int[] {
            EGL10.EGL_WIDTH, 16,
            EGL10.EGL_HEIGHT, 16,
            EGL10.EGL_NONE
        };
        EGLSurface eglSurface = egl.eglCreatePbufferSurface(eglDisplay,
                                                            eglConfig,
                                                            surfaceAttribs);
        if (eglSurface == EGL10.EGL_NO_SURFACE) {
            eglError(egl, "eglCreatePbufferSurface failed");
            return;
        }

        // obtain GL strings, store them in mDataQueue
        if (!egl.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
            eglError(egl, "eglMakeCurrent failed");
            return;
        }

        {
            int error = egl.eglGetError();
            if (error != EGL10.EGL_SUCCESS) {
                error("EGL error " + Integer.toHexString(error));
                return;
            }
        }

        String data =
            "VENDOR\n"   + GLES20.glGetString(GLES20.GL_VENDOR)   + "\n" +
            "RENDERER\n" + GLES20.glGetString(GLES20.GL_RENDERER) + "\n" +
            "VERSION\n"  + GLES20.glGetString(GLES20.GL_VERSION)  + "\n";

        {
            int error = GLES20.glGetError();
            if (error != GLES20.GL_NO_ERROR) {
                error("OpenGL error " + Integer.toHexString(error));
                return;
            }
        }

        // clean up after ourselves. This is especially important as some Android devices
        // have a very low limit on the global number of GL contexts.
        egl.eglMakeCurrent(eglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);
        egl.eglDestroySurface(eglDisplay, eglSurface);
        egl.eglDestroyContext(eglDisplay, eglContext);
        // intentionally do not eglTerminate: maybe this will make the next eglInitialize faster?

        // finally send the data. Notice that we've already freed the EGL resources, so that they don't
        // remain there until the data is read.
        synchronized (this) {
            mData = data;
            notifyAll();
        }
    }
}
