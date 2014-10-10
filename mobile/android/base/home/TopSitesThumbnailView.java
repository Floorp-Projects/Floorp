/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.ThumbnailHelper;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;

/**
 * A height constrained ImageView to show thumbnails of top and pinned sites.
 */
public class TopSitesThumbnailView extends ImageView {
    private static final String LOGTAG = "GeckoTopSitesThumbnailView";

    // 27.34% opacity filter for the dominant color.
    private static final int COLOR_FILTER = 0x46FFFFFF;

    // Default filter color for "Add a bookmark" views.
    private static final int DEFAULT_COLOR = 0xFFECF0F3;

    // Stroke width for the border.
    private final float mStrokeWidth = getResources().getDisplayMetrics().density * 2;

    // Paint for drawing the border.
    private static final Paint sBorderPaint;

    // Initializing the static border paint.
    static {
        sBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        sBorderPaint.setColor(0xFFCFD9E1);
        sBorderPaint.setStyle(Paint.Style.STROKE);
    }

    public TopSitesThumbnailView(Context context) {
        this(context, null);

        // A border will be drawn if needed.
        setWillNotDraw(false);
    }

    public TopSitesThumbnailView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.topSitesThumbnailViewStyle);
    }

    public TopSitesThumbnailView(Context context, AttributeSet attrs, int defStyle) {
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
     * {@inheritDoc}
     */
    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (getBackground() == null) {
            sBorderPaint.setStrokeWidth(mStrokeWidth);
            canvas.drawRect(0, 0, getWidth(), getHeight(), sBorderPaint);
        }
    }

    /**
     * Sets the background color with a filter to reduce the color opacity.
     *
     * @param color the color filter to apply over the drawable.
     */
    public void setBackgroundColorWithOpacityFilter(int color) {
        setBackgroundColor(color & COLOR_FILTER);
    }

    /**
     * Sets the background to a Drawable by applying the specified color as a filter.
     *
     * @param color the color filter to apply over the drawable.
     */
    @Override
    public void setBackgroundColor(int color) {
        if (color == 0) {
            color = DEFAULT_COLOR;
        }

        Drawable drawable = getResources().getDrawable(R.drawable.top_sites_thumbnail_bg);
        drawable.setColorFilter(color, Mode.SRC_ATOP);
        setBackgroundDrawable(drawable);
    }
}
