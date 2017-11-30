/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.view.Surface;

/**
 * Displays implement this interface to provide GeckoSession
 * with a Surface for displaying content.
 */
public interface GeckoDisplay {
    /**
     * Displays notify GeckoSession of changes to its Surface through this interface that
     * GeckoSession implements. To ensure drawing only happens on a valid Surface,
     * GeckoSession will only use the provided Surface after {@link
     * #surfaceChanged(Surface, int, int)} is called and before {@link
     * #surfaceDestroyed()} returns.
     */
    interface Listener {
        /**
         * Required callback. The display's Surface has been created or changed. Must be
         * called on the application main thread. GeckoSession may block this call to
         * ensure the Surface is valid while resuming drawing.
         *
         * @param surface The new Surface.
         * @param width New width of the Surface.
         * @param height New height of the Surface.
         */
        void surfaceChanged(Surface surface, int width, int height);

        /**
         * Required callback. The display's Surface has been destroyed. Must be called on
         * the application main thread. GeckoSession may block this call to ensure the
         * Surface is valid while pausing drawing.
         */
        void surfaceDestroyed();

        /**
         * Optional callback. The display's coordinates on the screen has changed. Must be
         * called on the application main thread.
         *
         * @param left The X coordinate of the display on the screen, in screen pixels.
         * @param top The Y coordinate of the display on the screen, in screen pixels.
         */
        void screenOriginChanged(int left, int top);
    }

    /**
     * Get the current listener attached to this display. Must be called on the
     * application main thread.
     *
     * @return Current listener or null if there is no listener.
     */
    Listener getListener();

    /**
     * Set a new listener attached to this display. Must be called on the application main
     * thread. When attaching a new listener, and there is an existing valid Surface, the
     * display must call {@link Listener#surfaceChanged(Surface, int, int)} (and other
     * optional callbacks, if implemented) on the new listener. Similarly, when detaching
     * a previous listener, and there is an existing valid Surface, the display must call
     * {@link Listener#surfaceDestroyed()} on the previous listener.
     *
     * @param listener New listener or null if detaching previous listener.
     */
    void setListener(Listener listener);
}
