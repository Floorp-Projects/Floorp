/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.opengl.GLES20;
import android.util.Log;

import java.util.concurrent.ArrayBlockingQueue;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLContext;

public class TextureGenerator {
    private static final String LOGTAG = "TextureGenerator";
    private static final int POOL_SIZE = 5;

    private static TextureGenerator sSharedInstance;

    private final ArrayBlockingQueue<Integer> mTextureIds;
    private EGLContext mContext;

    private TextureGenerator() { mTextureIds = new ArrayBlockingQueue<Integer>(POOL_SIZE); }

    public static TextureGenerator get() {
        if (sSharedInstance == null)
            sSharedInstance = new TextureGenerator();
        return sSharedInstance;
    }

    public synchronized int take() {
        try {
            // Will block until one becomes available
            return (int)mTextureIds.take();
        } catch (InterruptedException e) {
            return 0;
        }
    }

    public synchronized void fill() {
        EGL10 egl = (EGL10)EGLContext.getEGL();
        EGLContext context = egl.eglGetCurrentContext();

        if (mContext != null && mContext != context) {
            mTextureIds.clear();
        }

        mContext = context;

        int numNeeded = mTextureIds.remainingCapacity();
        if (numNeeded == 0)
            return;

        // Clear existing GL errors
        int error;
        while ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR) {
            Log.w(LOGTAG, String.format("Clearing GL error: %#x", error));
        }

        int[] textures = new int[numNeeded];
        GLES20.glGenTextures(numNeeded, textures, 0);

        error = GLES20.glGetError();
        if (error != GLES20.GL_NO_ERROR) {
            Log.e(LOGTAG, String.format("Failed to generate textures: %#x", error), new Exception());
            return;
        }

        for (int i = 0; i < numNeeded; i++) {
            mTextureIds.offer(textures[i]);
        }
    }
}


