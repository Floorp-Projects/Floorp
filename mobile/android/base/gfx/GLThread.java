/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011-2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.gfx;

import android.opengl.GLSurfaceView;
import android.view.SurfaceHolder;
import javax.microedition.khronos.opengles.GL10;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;

// A GL thread managed by Java. It is not necessary to use this class to use the
// FlexibleGLSurfaceView, but it can be helpful, especially if the GL rendering is to be done
// entirely in Java.
class GLThread extends Thread {
    private LinkedBlockingQueue<Runnable> mQueue;
    private GLController mController;
    private boolean mRenderQueued;

    public GLThread(GLController controller) {
        mQueue = new LinkedBlockingQueue<Runnable>();
        mController = controller;
    }

    @Override
    public void run() {
        while (true) {
            Runnable runnable;
            try {
                runnable = mQueue.take();
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }

            runnable.run();
            if (runnable instanceof ShutdownMessage) {
                break;
            }
        }
    }

    public void recreateSurface() {
        mQueue.add(new RecreateSurfaceMessage());
    }

    public void renderFrame() {
        // Make sure there's only one render event in the queue at a time.
        synchronized (this) {
            if (!mRenderQueued) {
                mQueue.add(new RenderFrameMessage());
                mRenderQueued = true;
            }
        }
    }

    public void shutdown() {
        mQueue.add(new ShutdownMessage());
    }

    public void surfaceChanged(int width, int height) {
        mQueue.add(new SizeChangedMessage(width, height));
    }

    public void surfaceCreated() {
        mQueue.add(new SurfaceCreatedMessage());
    }

    public void surfaceDestroyed() {
        mQueue.add(new SurfaceDestroyedMessage());
    }

    private void doRecreateSurface() {
        mController.disposeGLContext();
        mController.initGLContext();
    }

    private GLSurfaceView.Renderer getRenderer() {
        return mController.getView().getRenderer();
    }

    private class RecreateSurfaceMessage implements Runnable {
        public void run() {
            doRecreateSurface();
        }
    }

    private class RenderFrameMessage implements Runnable {
        public void run() {
            synchronized (GLThread.this) {
                mRenderQueued = false;
            }

            // Bail out if the surface was lost.
            if (mController.getEGLSurface() == null) {
                return;
            }

            GLSurfaceView.Renderer renderer = getRenderer();
            if (renderer != null) {
                renderer.onDrawFrame((GL10)mController.getGL());
            }

            mController.swapBuffers();
        }
    }

    private class ShutdownMessage implements Runnable {
        public void run() {
            mController.disposeGLContext();
            mController = null;
        }
    }

    private class SizeChangedMessage implements Runnable {
        private int mWidth, mHeight;

        public SizeChangedMessage(int width, int height) {
            mWidth = width;
            mHeight = height;
        }

        public void run() {
            GLSurfaceView.Renderer renderer = getRenderer();
            if (renderer != null) {
                renderer.onSurfaceChanged((GL10)mController.getGL(), mWidth, mHeight);
            }
        }
    }

    private class SurfaceCreatedMessage implements Runnable {
        public void run() {
            if (!mController.hasSurface()) {
                mController.initGLContext();
            }
        }
    }

    private class SurfaceDestroyedMessage implements Runnable {
        public void run() {
            mController.disposeGLContext();
        }
    }
}

