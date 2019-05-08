/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.graphics.Bitmap;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;
import android.view.Surface;

import org.mozilla.gecko.util.ThreadUtils;

/**
 * Applications use a GeckoDisplay instance to provide {@link GeckoSession} with a {@link Surface} for
 * displaying content. To ensure drawing only happens on a valid {@link Surface}, {@link GeckoSession}
 * will only use the provided {@link Surface} after {@link #surfaceChanged(Surface, int, int)} or
 * {@link #surfaceChanged(Surface, int, int, int, int)} is called and before {@link #surfaceDestroyed()} returns.
 */
public class GeckoDisplay {
    private final GeckoSession mSession;

    protected GeckoDisplay(final GeckoSession session) {
        mSession = session;
    }

    /**
     * Sets a surface for the compositor render a surface.
     *
     * Required call. The display's Surface has been created or changed. Must be
     * called on the application main thread. GeckoSession may block this call to ensure
     * the Surface is valid while resuming drawing.
     *
     * @param surface The new Surface.
     * @param width New width of the Surface. Can not be negative.
     * @param height New height of the Surface. Can not be negative.
     */
    @UiThread
    public void surfaceChanged(@NonNull final Surface surface, final int width, final int height) {
        surfaceChanged(surface, 0, 0, width, height);
    }

    /**
     * Sets a surface for the compositor render a surface.
     *
     * Required call. The display's Surface has been created or changed. Must be
     * called on the application main thread. GeckoSession may block this call to ensure
     * the Surface is valid while resuming drawing. The origin of the content window
     * (0, 0) is the top left corner of the screen.
     *
     * @param surface The new Surface.
     * @param left The compositor origin offset in the X axis. Can not be negative.
     * @param top The compositor origin offset in the Y axis. Can not be negative.
     * @param width New width of the Surface. Can not be negative.
     * @param height New height of the Surface. Can not be negative.
     * @throws IllegalArgumentException if left or top are negative.
     */
    @UiThread
    public void surfaceChanged(@NonNull final Surface surface, final int left, final int top,
                               final int width, final int height) {
        ThreadUtils.assertOnUiThread();

        if ((left < 0) || (top < 0)) {
            throw new IllegalArgumentException("Parameters can not be negative.");
        }

        if (mSession.getDisplay() == this) {
            mSession.onSurfaceChanged(surface, left, top, width, height);
        }
    }

    /**
     * Removes the current surface registered with the compositor.
     *
     * Required call. The display's Surface has been destroyed. Must be called on the
     * application main thread. GeckoSession may block this call to ensure the Surface is
     * valid while pausing drawing.
     */
    @UiThread
    public void surfaceDestroyed() {
        ThreadUtils.assertOnUiThread();

        if (mSession.getDisplay() == this) {
            mSession.onSurfaceDestroyed();
        }
    }

    /**
     * Update the position of the surface on the screen.
     *
     * Optional call. The display's coordinates on the screen has changed. Must be
     * called on the application main thread.
     *
     * @param left The X coordinate of the display on the screen, in screen pixels.
     * @param top The Y coordinate of the display on the screen, in screen pixels.
     */
    @UiThread
    public void screenOriginChanged(final int left, final int top) {
        ThreadUtils.assertOnUiThread();

        if (mSession.getDisplay() == this) {
            mSession.onScreenOriginChanged(left, top);
        }
    }

    /**
     * Update the amount of vertical space that is clipped or visibly obscured in the bottom portion
     * of the display. Tells gecko where to put bottom fixed elements so they are fully visible.
     *
     * Optional call. The display's visible vertical space has changed. Must be
     * called on the application main thread.
     *
     * @param clippingHeight The height of the bottom clipped space in screen pixels.
     */
    @UiThread
    public void setVerticalClipping(final int clippingHeight) {
        ThreadUtils.assertOnUiThread();

        if (mSession != null) {
            mSession.setFixedBottomOffset(clippingHeight);
        }
    }

    /**
     * Return whether the display should be pinned on the screen.
     *
     * When pinned, the display should not be moved on the screen due to animation, scrolling, etc.
     * A common reason for the display being pinned is when the user is dragging a selection caret
     * inside the display; normal user interaction would be disrupted in that case if the display
     * was moved on screen.
     *
     * @return True if display should be pinned on the screen.
     */
    @UiThread
    public boolean shouldPinOnScreen() {
        ThreadUtils.assertOnUiThread();
        return mSession.getDisplay() == this && mSession.shouldPinOnScreen();
    }

    /**
     * Request a {@link Bitmap} of the visible portion of the web page currently being
     * rendered.
     *
     * Returned {@link Bitmap} will have the same dimensions as the {@link Surface} the
     * {@link GeckoDisplay} is currently using.
     *
     * If the {@link GeckoSession#isCompositorReady} is false the {@link GeckoResult} will complete
     * with an {@link IllegalStateException}.
     *
     * This function must be called on the UI thread.
     *
     * @return A {@link GeckoResult} that completes with a {@link Bitmap} containing
     * the pixels and size information of the currently visible rendered web page.
     */
    @UiThread
    public @NonNull GeckoResult<Bitmap> capturePixels() {
        ThreadUtils.assertOnUiThread();
        if (!mSession.isCompositorReady()) {
            return GeckoResult.fromException(
                    new IllegalStateException("Compositor must be ready before pixels can be captured"));
        }
        GeckoResult<Bitmap> result = new GeckoResult<>();
        mSession.mCompositor.requestScreenPixels(result);
        return result;
    }
}
