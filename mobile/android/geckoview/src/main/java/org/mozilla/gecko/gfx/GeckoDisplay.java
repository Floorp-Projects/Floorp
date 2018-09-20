/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.view.Surface;

/**
 * Applications use a GeckoDisplay instance to provide GeckoSession with a Surface for
 * displaying content. To ensure drawing only happens on a valid Surface, GeckoSession
 * will only use the provided Surface after {@link #surfaceChanged(Surface, int, int)} is
 * called and before {@link #surfaceDestroyed()} returns.
 */
public class GeckoDisplay {
    private final LayerSession mSession;

    /* package */ GeckoDisplay(final LayerSession session) {
        mSession = session;
    }

    /**
     * Required callback. The display's Surface has been created or changed. Must be
     * called on the application main thread. GeckoSession may block this call to ensure
     * the Surface is valid while resuming drawing.
     *
     * @param surface The new Surface.
     * @param width New width of the Surface.
     * @param height New height of the Surface.
     */
    public void surfaceChanged(Surface surface, int width, int height) {
        mSession.onSurfaceChanged(surface, width, height);
    }

    /**
     * Required callback. The display's Surface has been destroyed. Must be called on the
     * application main thread. GeckoSession may block this call to ensure the Surface is
     * valid while pausing drawing.
     */
    public void surfaceDestroyed() {
        mSession.onSurfaceDestroyed();
    }

    /**
     * Optional callback. The display's coordinates on the screen has changed. Must be
     * called on the application main thread.
     *
     * @param left The X coordinate of the display on the screen, in screen pixels.
     * @param top The Y coordinate of the display on the screen, in screen pixels.
     */
    public void screenOriginChanged(final int left, final int top) {
        mSession.onScreenOriginChanged(left, top);
    }
}
