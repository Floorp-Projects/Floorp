/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.view.Surface;

import org.mozilla.gecko.util.ThreadUtils;

import java.nio.ByteBuffer;

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
     * Update the safe area insets of the surface on the screen.
     *
     * @param left   left margin of safe area
     * @param top    top margin of safe area
     * @param right  right margin of safe area
     * @param bottom bottom margin of safe area
     */
    @UiThread
    public void safeAreaInsetsChanged(final int top, final int right, final int bottom, final int left) {
        ThreadUtils.assertOnUiThread();

        if (mSession.getDisplay() == this) {
            mSession.onSafeAreaInsetsChanged(top, right, bottom, left);
        }
    }
    /**
     * Set the maximum height of the dynamic toolbar(s).
     *
     * If the toolbar is dynamic, this function needs to be called with the maximum
     * possible toolbar height so that Gecko can make the ICB static even during the dynamic
     * toolbar height is being changed.
     *
     * @param height The maximum height of the dynamic toolbar(s).
     */
    @UiThread
    public void setDynamicToolbarMaxHeight(final int height) {
        ThreadUtils.assertOnUiThread();

        if (mSession != null) {
            mSession.setDynamicToolbarMaxHeight(height);
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
        return screenshot().capture();
    }

    /**
     * Builder to construct screenshot requests.
     */
    final static public class ScreenshotBuilder {
        private static final int NONE = 0;
        private static final int SCALE = 1;
        private static final int ASPECT = 2;
        private static final int FULL = 3;
        private static final int RECYCLE = 4;

        private final GeckoSession mSession;
        private int mOffsetX;
        private int mOffsetY;
        private int mSrcWidth;
        private int mSrcHeight;
        private int mOutWidth;
        private int mOutHeight;
        private int mAspectPreservingWidth;
        private float mScale;
        private Bitmap mRecycle;
        private int mSizeType;

        /* package */ ScreenshotBuilder(final GeckoSession session) {
            this.mSizeType = NONE;
            this.mSession = session;
        }

        /**
         * The screenshot will be of a region instead of the entire screen
         * @param x Left most pixel of the source region.
         * @param y Top most pixel of the source region.
         * @param width Width of the source region in screen pixels
         * @param height Height of the source region in screen pixels
         * @return The builder
         */
        @AnyThread
        public @NonNull ScreenshotBuilder source(final int x, final int y, final int width, final int height) {
            mOffsetX = x;
            mOffsetY = y;
            mSrcWidth = width;
            mSrcHeight = height;
            return this;
        }

        /**
         * The screenshot will be of a region instead of the entire screen
         * @param source Region of the screen to capture in screen pixels
         * @return The builder
         */
        @AnyThread
        public @NonNull ScreenshotBuilder source(final @NonNull Rect source) {
            mOffsetX = source.left;
            mOffsetY = source.top;
            mSrcWidth = source.width();
            mSrcHeight = source.height();
            return this;
        }

        private void checkAndSetSizeType(final int sizeType) {
            if (mSizeType != NONE) {
                throw new IllegalStateException("Size has already been set.");
            }
            mSizeType = sizeType;
        }

        /**
         * The width of the bitmap to create when taking the screenshot. The height will be
         * calculated to match the aspect ratio of the source as closely as possible. The source
         * screenshot will be scaled into the resulting Bitmap.
         * @param width of the result Bitmap in screen pixels.
         * @return The builder
         * @throws IllegalStateException if the size has already been set in some other way.
         */
        @AnyThread
        public @NonNull ScreenshotBuilder aspectPreservingSize(final int width) {
            checkAndSetSizeType(ASPECT);
            mAspectPreservingWidth = width;
            return this;
        }

        /**
         * The scale of the bitmap relative to the source. The height and width of the output
         * bitmap will be within one pixel of this multiple of the source dimensions. The source
         * screenshot will be scaled into the resulting Bitmap.
         * @param scale of the result Bitmap relative to the source.
         * @return The builder
         * @throws IllegalStateException if the size has already been set in some other way.
         */
        @AnyThread
        public @NonNull ScreenshotBuilder scale(final float scale) {
            checkAndSetSizeType(SCALE);
            mScale = scale;
            return this;
        }

        /**
         * Size of the bitmap to create when taking the screenshot. The source screenshot will be
         * scaled into the resulting Bitmap
         * @param width of the result Bitmap in screen pixels.
         * @param height of the result Bitmap in screen pixels.
         * @return The builder
         * @throws IllegalStateException if the size has already been set in some other way.
         */
        @AnyThread
        public @NonNull ScreenshotBuilder size(final int width, final int height) {
            checkAndSetSizeType(FULL);
            mOutWidth = width;
            mOutHeight = height;
            return this;
        }

        /**
         * Instead of creating a new Bitmap for the result, the builder will use the passed Bitmap.
         * @param bitmap The Bitmap to use in the result.
         * @return The builder.
         * @throws IllegalStateException if the size has already been set in some other way.
         */
        @AnyThread
        public @NonNull ScreenshotBuilder bitmap(final @Nullable Bitmap bitmap) {
            checkAndSetSizeType(RECYCLE);
            mRecycle = bitmap;
            return this;
        }

        /**
         * Request a {@link Bitmap} of the requested portion of the web page currently being
         * rendered using any parameters specified with the builder.
         *
         * This function must be called on the UI thread.
         *
         * @return A {@link GeckoResult} that completes with a {@link Bitmap} containing
         * the pixels and size information of the requested portion of the visible web page.
         */
        @UiThread
        public @NonNull GeckoResult<Bitmap> capture() {
            ThreadUtils.assertOnUiThread();
            if (!mSession.isCompositorReady()) {
                throw new IllegalStateException("Compositor must be ready before pixels can be captured");
            }

            final GeckoResult<ByteBuffer> result = new GeckoResult<>();
            final Bitmap target;
            final Rect rect = new Rect();

            if (mSrcWidth == 0 || mSrcHeight == 0) {
                // Source is unset or invalid, use defaults.
                mSession.getSurfaceBounds(rect);
                mSrcWidth = rect.width();
                mSrcHeight = rect.height();
            }

            switch (mSizeType) {
                case NONE:
                    mOutWidth = mSrcWidth;
                    mOutHeight = mSrcHeight;
                    break;
                case SCALE:
                    mSession.getSurfaceBounds(rect);
                    mOutWidth = (int) (rect.width() * mScale);
                    mOutHeight = (int) (rect.height() * mScale);
                    break;
                case ASPECT:
                    mSession.getSurfaceBounds(rect);
                    mOutWidth = mAspectPreservingWidth;
                    mOutHeight = (int) (rect.height() * (mAspectPreservingWidth / (double) rect.width()));
                    break;
                case RECYCLE:
                    mOutWidth = mRecycle.getWidth();
                    mOutHeight = mRecycle.getHeight();
                    break;
                // case FULL does not need to be handled, as width and height are already set.
            }

            if (mRecycle == null) {
                try {
                    target = Bitmap.createBitmap(mOutWidth, mOutHeight, Bitmap.Config.ARGB_8888);
                } catch (Throwable e) {
                    if (e instanceof NullPointerException || e instanceof OutOfMemoryError) {
                        return GeckoResult.fromException(new OutOfMemoryError("Not enough memory to allocate for bitmap"));
                    }
                    return GeckoResult.fromException(new Throwable("Failed to create bitmap", e));
                }
            } else {
                target = mRecycle;
            }

            mSession.mCompositor.requestScreenPixels(result, mOffsetX, mOffsetY, mSrcWidth, mSrcHeight, mOutWidth, mOutHeight);

            return result.then( buffer -> {
                target.copyPixelsFromBuffer(buffer);
                return GeckoResult.fromValue(target);
            });
        }
    }

    /**
     * Creates a new screenshot builder.
     * @return The new {@link ScreenshotBuilder}
     */
    @UiThread
    public @NonNull ScreenshotBuilder screenshot() {
        return new ScreenshotBuilder(mSession);
    }
}
