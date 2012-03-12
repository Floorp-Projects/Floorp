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
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *   Arkady Blyakher <rkadyb@mit.edu>
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

import org.mozilla.gecko.GeckoInputConnection;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.InputConnectionHandler;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.ui.SimpleScaleGestureDetector;
import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.View;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.ScaleGestureDetector;
import android.widget.RelativeLayout;
import android.util.Log;
import java.nio.IntBuffer;
import java.util.LinkedList;

/**
 * A view rendered by the layer compositor.
 *
 * This view delegates to LayerRenderer to actually do the drawing. Its role is largely that of a
 * mediator between the LayerRenderer and the LayerController.
 */
public class LayerView extends FlexibleGLSurfaceView {
    private Context mContext;
    private LayerController mController;
    private InputConnectionHandler mInputConnectionHandler;
    private LayerRenderer mRenderer;
    private GestureDetector mGestureDetector;
    private SimpleScaleGestureDetector mScaleGestureDetector;
    private long mRenderTime;
    private boolean mRenderTimeReset;
    private static String LOGTAG = "GeckoLayerView";
    /* List of events to be processed if the page does not prevent them. Should only be touched on the main thread */
    private LinkedList<MotionEvent> mEventQueue = new LinkedList<MotionEvent>();


    public LayerView(Context context, LayerController controller) {
        super(context);

        mContext = context;
        mController = controller;
        mRenderer = new LayerRenderer(this);
        setRenderer(mRenderer);
        mGestureDetector = new GestureDetector(context, controller.getGestureListener());
        mScaleGestureDetector =
            new SimpleScaleGestureDetector(controller.getScaleGestureListener());
        mGestureDetector.setOnDoubleTapListener(controller.getDoubleTapListener());
        mInputConnectionHandler = null;

        setFocusable(true);
        setFocusableInTouchMode(true);

        createGLThread();
    }

    private void addToEventQueue(MotionEvent event) {
        MotionEvent copy = MotionEvent.obtain(event);
        mEventQueue.add(copy);
    }

    public void processEventQueue() {
        MotionEvent event = mEventQueue.poll();
        while(event != null) {
            processEvent(event);
            event = mEventQueue.poll();
        }
    }

    public void clearEventQueue() {
        mEventQueue.clear();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mController.onTouchEvent(event)) {
            addToEventQueue(event);
            return true;
        }
        return processEvent(event);
    }

    private boolean processEvent(MotionEvent event) {
        if (mGestureDetector.onTouchEvent(event))
            return true;
        mScaleGestureDetector.onTouchEvent(event);
        if (mScaleGestureDetector.isInProgress())
            return true;
        mController.getPanZoomController().onTouchEvent(event);
        return true;
    }

    public LayerController getController() { return mController; }

    /** The LayerRenderer calls this to indicate that the window has changed size. */
    public void setViewportSize(IntSize size) {
        mController.setViewportSize(new FloatSize(size));
    }

    public GeckoInputConnection setInputConnectionHandler() {
        GeckoInputConnection geckoInputConnection = GeckoInputConnection.create(this);
        mInputConnectionHandler = geckoInputConnection;
        return geckoInputConnection;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onCreateInputConnection(outAttrs);
        return null;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyPreIme(keyCode, event);
        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyDown(keyCode, event);
        return false;
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyLongPress(keyCode, event);
        return false;
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyMultiple(keyCode, repeatCount, event);
        return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyUp(keyCode, event);
        return false;
    }

    @Override
    public void requestRender() {
        super.requestRender();

        synchronized(this) {
            if (!mRenderTimeReset) {
                mRenderTimeReset = true;
                mRenderTime = System.nanoTime();
            }
        }
    }

    public void addLayer(Layer layer) {
        mRenderer.addLayer(layer);
    }

    public void removeLayer(Layer layer) {
        mRenderer.removeLayer(layer);
    }

    /**
     * Returns the time elapsed between the first call of requestRender() after
     * the last call of getRenderTime(), in nanoseconds.
     */
    public long getRenderTime() {
        synchronized(this) {
            mRenderTimeReset = false;
            return System.nanoTime() - mRenderTime;
        }
    }

    public int getMaxTextureSize() {
        return mRenderer.getMaxTextureSize();
    }

    /** Used by robocop for testing purposes. Not for production use! This is called via reflection by robocop. */
    public IntBuffer getPixels() {
        return mRenderer.getPixels();
    }
}

