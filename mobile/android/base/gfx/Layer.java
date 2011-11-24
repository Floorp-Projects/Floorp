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
 *   Chris Lord <chrislord.net@gmail.com>
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

import android.graphics.Point;
import android.util.Log;
import java.util.concurrent.locks.ReentrantLock;
import javax.microedition.khronos.opengles.GL10;

public abstract class Layer {
    private final ReentrantLock mTransactionLock;
    private boolean mInTransaction;
    private Point mOrigin;
    private Point mNewOrigin;
    private float mResolution;
    private float mNewResolution;

    public Layer() {
        mTransactionLock = new ReentrantLock();
        mOrigin = new Point(0, 0);
        mResolution = 1.0f;
    }

    /** Updates the layer. */
    public final void update(GL10 gl) {
        if (mTransactionLock.isHeldByCurrentThread()) {
            throw new RuntimeException("draw() called while transaction lock held by this " +
                                       "thread?!");
        }

        if (mTransactionLock.tryLock()) {
            try {
                performUpdates(gl);
            } finally {
                mTransactionLock.unlock();
            }
        }
    }

    /** Sets the transformation for the layer. */
    public final void transform(GL10 gl) {
        gl.glScalef(1.0f / mResolution, 1.0f / mResolution, 1.0f);
        gl.glTranslatef(mOrigin.x, mOrigin.y, 0.0f);
    }

    /** Draws the layer. Automatically applies the transformation. */
    public final void draw(GL10 gl) {
        gl.glPushMatrix();

        onDraw(gl);

        gl.glPopMatrix();
    }

    /**
     * Call this before modifying the layer. Note that, for TileLayers, "modifying the layer"
     * includes altering the underlying CairoImage in any way. Thus you must call this function
     * before modifying the byte buffer associated with this layer.
     *
     * This function may block, so you should never call this on the main UI thread.
     */
    public void beginTransaction() {
        if (mTransactionLock.isHeldByCurrentThread())
            throw new RuntimeException("Nested transactions are not supported");
        mTransactionLock.lock();
        mInTransaction = true;
        mNewResolution = mResolution;
    }

    /** Call this when you're done modifying the layer. */
    public void endTransaction() {
        if (!mInTransaction)
            throw new RuntimeException("endTransaction() called outside a transaction");
        mInTransaction = false;
        mTransactionLock.unlock();
    }

    /** Returns true if the layer is currently in a transaction and false otherwise. */
    protected boolean inTransaction() {
        return mInTransaction;
    }

    /** Returns the current layer origin. */
    public Point getOrigin() {
        return mOrigin;
    }

    /** Sets the origin. Only valid inside a transaction. */
    public void setOrigin(Point newOrigin) {
        if (!mInTransaction)
            throw new RuntimeException("setOrigin() is only valid inside a transaction");
        mNewOrigin = newOrigin;
    }

    /** Returns the current layer's resolution. */
    public float getResolution() {
        return mResolution;
    }

    /**
     * Sets the layer resolution. This value is used to determine how many pixels per
     * device pixel this layer was rendered at. This will be reflected by scaling by
     * the reciprocal of the resolution in the layer's transform() function.
     * Only valid inside a transaction. */
    public void setResolution(float newResolution) {
        if (!mInTransaction)
            throw new RuntimeException("setResolution() is only valid inside a transaction");
        mNewResolution = newResolution;
    }

    /**
     * Subclasses implement this method to perform drawing.
     *
     * Invariant: The current matrix mode must be GL_MODELVIEW both before and after this call.
     */
    protected abstract void onDraw(GL10 gl);

    /**
     * Subclasses may override this method to perform custom layer updates. This will be called
     * with the transaction lock held. Subclass implementations of this method must call the
     * superclass implementation.
     */
    protected void performUpdates(GL10 gl) {
        if (mNewOrigin != null) {
            mOrigin = mNewOrigin;
            mNewOrigin = null;
        }
        mResolution = mNewResolution;
    }
}

