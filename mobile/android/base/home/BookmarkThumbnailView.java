/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.ThumbnailHelper;

import android.content.Context;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;

/**
 * A height constrained ImageView to show thumbnails of top bookmarks.
 */
public class BookmarkThumbnailView extends ImageView {
    private static final String LOGTAG = "GeckoBookmarkThumbnailView";

    // 27.34% opacity filter for the dominant color.
    private static final int COLOR_FILTER = 0x46FFFFFF;

    // Default filter color for "Add a bookmark" views.
    private static final int DEFAULT_COLOR = 0x46ECF0F3;

    public BookmarkThumbnailView(Context context) {
        this(context, null);
    }

    public BookmarkThumbnailView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.bookmarkThumbnailViewStyle);
    }

    public BookmarkThumbnailView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    /**
     * Measure the view to determine the measured width and height.
     * The height is constrained by the measured width.
     *
     * @param widthMeasureSpec horizontal space requirements as imposed by the parent.
     * @param heightMeasureSpec vertical space requirements as imposed by the parent, but ignored.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Default measuring.
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // Force the height based on the aspect ratio.
        final int width = getMeasuredWidth();
        final int height = (int) (width * ThumbnailHelper.THUMBNAIL_ASPECT_RATIO);
        setMeasuredDimension(width, height);
    }

    /**
     * Sets the background to a Drawable by applying the specified color as a filter.
     *
     * @param color the color filter to apply over the drawable.
     */
    @Override
    public void setBackgroundColor(int color) {
        int colorFilter = color == 0 ? DEFAULT_COLOR : color & COLOR_FILTER;
        Drawable drawable = getResources().getDrawable(R.drawable.favicon_bg);
        drawable.setColorFilter(colorFilter, Mode.SRC_ATOP);
        setBackgroundDrawable(drawable);
    }
}
