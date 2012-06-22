/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import java.util.concurrent.locks.ReentrantLock;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.opengl.GLSurfaceView;
import android.util.Log;

public class ViEAndroidGLES20 extends GLSurfaceView
        implements GLSurfaceView.Renderer {
    // True if onSurfaceCreated has been called.
    private boolean surfaceCreated = false;
    private boolean openGLCreated = false;
    // True if NativeFunctionsRegistered has been called.
    private boolean nativeFunctionsRegisted = false;
    private ReentrantLock nativeFunctionLock = new ReentrantLock();
    // Address of Native object that will do the drawing.
    private long nativeObject = 0;
    private int viewWidth = 0;
    private int viewHeight = 0;

    public static boolean UseOpenGL2(Object renderWindow) {
        return ViEAndroidGLES20.class.isInstance(renderWindow);
    }

    public ViEAndroidGLES20(Context context) {
        super(context);

        // Setup the context factory for 2.0 rendering.
        // See ContextFactory class definition below
        setEGLContextFactory(new ContextFactory());

        // We need to choose an EGLConfig that matches the format of
        // our surface exactly. This is going to be done in our
        // custom config chooser. See ConfigChooser class definition below
        // Use RGB 565 without an alpha channel.
        setEGLConfigChooser( new ConfigChooser(5, 6, 5, 0, 0, 0) );

        this.setRenderer(this);
        this.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
    }

    // IsSupported
    // Return true if this device support Open GL ES 2.0 rendering.
    public static boolean IsSupported(Context context) {
        ActivityManager am =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        ConfigurationInfo info = am.getDeviceConfigurationInfo();
        if(info.reqGlEsVersion >= 0x20000) {
            // Open GL ES 2.0 is supported.
            return true;
        }
        return false;
    }

    public void onDrawFrame(GL10 gl) {
        nativeFunctionLock.lock();
        if(!nativeFunctionsRegisted || !surfaceCreated) {
            nativeFunctionLock.unlock();
            return;
        }

        if(!openGLCreated) {
            if(0 != CreateOpenGLNative(nativeObject, viewWidth, viewHeight)) {
                return; // Failed to create OpenGL
            }
            openGLCreated = true; // Created OpenGL successfully
        }
        DrawNative(nativeObject); // Draw the new frame
        nativeFunctionLock.unlock();
    }

    public void onSurfaceChanged(GL10 gl, int width, int height) {
        surfaceCreated = true;
        viewWidth = width;
        viewHeight = height;

        nativeFunctionLock.lock();
        if(nativeFunctionsRegisted) {
            if(CreateOpenGLNative(nativeObject,width,height) == 0)
                openGLCreated = true;
        }
        nativeFunctionLock.unlock();
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
    }

    public void RegisterNativeObject(long nativeObject) {
        nativeFunctionLock.lock();
        nativeObject = nativeObject;
        nativeFunctionsRegisted = true;
        nativeFunctionLock.unlock();
    }

    public void DeRegisterNativeObject() {
        nativeFunctionLock.lock();
        nativeFunctionsRegisted = false;
        openGLCreated = false;
        nativeObject = 0;
        nativeFunctionLock.unlock();
    }

    public void ReDraw() {
        if(surfaceCreated) {
            // Request the renderer to redraw using the render thread context.
            this.requestRender();
        }
    }

    // EGL Context factory used for creating EGL 2.0 context
    // on Android 2.1(and later,
    // though there are simpler ways in 2.2)
    // Code is from the NDK samples\hello-gl2\src\com\android\gl2jni.
    private static class ContextFactory
            implements GLSurfaceView.EGLContextFactory {
        private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
        public EGLContext createContext(EGL10 egl,
                EGLDisplay display,
                EGLConfig eglConfig) {
            //checkEglError("Before eglCreateContext", egl);
            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };
            // Create an Open GL ES 2.0 context
            EGLContext context = egl.eglCreateContext(display,
                    eglConfig,
                    EGL10.EGL_NO_CONTEXT,
                    attrib_list);
            checkEglError("ContextFactory eglCreateContext", egl);
            return context;
        }

        public void destroyContext(EGL10 egl, EGLDisplay display,
                EGLContext context) {
            egl.eglDestroyContext(display, context);
        }
    }

    private static void checkEglError(String prompt, EGL10 egl) {
        int error;
        while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
            Log.e("*WEBRTC*", String.format("%s: EGL error: 0x%x", prompt, error));
        }
    }

    // Code is from the NDK samples\hello-gl2\src\com\android\gl2jni
    private static class ConfigChooser
            implements GLSurfaceView.EGLConfigChooser {

        public ConfigChooser(int r, int g, int b, int a, int depth, int stencil) {
            mRedSize = r;
            mGreenSize = g;
            mBlueSize = b;
            mAlphaSize = a;
            mDepthSize = depth;
            mStencilSize = stencil;
        }

        // This EGL config specification is used to specify 2.0 rendering.
        // We use a minimum size of 4 bits for red/green/blue, but will
        // perform actual matching in chooseConfig() below.
        private static int EGL_OPENGL_ES2_BIT = 4;
        private static int[] s_configAttribs2 =
        {
            EGL10.EGL_RED_SIZE, 4,
            EGL10.EGL_GREEN_SIZE, 4,
            EGL10.EGL_BLUE_SIZE, 4,
            EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL10.EGL_NONE
        };

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {

            // Get the number of minimally matching EGL configurations
            int[] num_config = new int[1];
            egl.eglChooseConfig(display, s_configAttribs2, null, 0, num_config);

            int numConfigs = num_config[0];

            if (numConfigs <= 0) {
                throw new IllegalArgumentException("No configs match configSpec");
            }

            // Allocate then read the array of minimally matching EGL configs
            EGLConfig[] configs = new EGLConfig[numConfigs];
            egl.eglChooseConfig(display, s_configAttribs2, configs,
                    numConfigs, num_config);

            // Now return the "best" one
            return chooseConfig(egl, display, configs);
        }

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display,
                EGLConfig[] configs) {
            for(EGLConfig config : configs) {
                int d = findConfigAttrib(egl, display, config,
                        EGL10.EGL_DEPTH_SIZE, 0);
                int s = findConfigAttrib(egl, display, config,
                        EGL10.EGL_STENCIL_SIZE, 0);

                // We need at least mDepthSize and mStencilSize bits
                if (d < mDepthSize || s < mStencilSize)
                    continue;

                // We want an *exact* match for red/green/blue/alpha
                int r = findConfigAttrib(egl, display, config,
                        EGL10.EGL_RED_SIZE, 0);
                int g = findConfigAttrib(egl, display, config,
                        EGL10.EGL_GREEN_SIZE, 0);
                int b = findConfigAttrib(egl, display, config,
                        EGL10.EGL_BLUE_SIZE, 0);
                int a = findConfigAttrib(egl, display, config,
                        EGL10.EGL_ALPHA_SIZE, 0);

                if (r == mRedSize && g == mGreenSize &&
                        b == mBlueSize && a == mAlphaSize)
                    return config;
            }
            return null;
        }

        private int findConfigAttrib(EGL10 egl, EGLDisplay display,
                EGLConfig config, int attribute,
                int defaultValue) {

            if (egl.eglGetConfigAttrib(display, config, attribute, mValue)) {
                return mValue[0];
            }
            return defaultValue;
        }

        // Subclasses can adjust these values:
        protected int mRedSize;
        protected int mGreenSize;
        protected int mBlueSize;
        protected int mAlphaSize;
        protected int mDepthSize;
        protected int mStencilSize;
        private int[] mValue = new int[1];
    }

    private native int CreateOpenGLNative(long nativeObject,
            int width, int height);
    private native void DrawNative(long nativeObject);

}
