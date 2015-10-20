/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.widget.LinearLayout;

public class RoundedCornerLayout extends LinearLayout {
    private static final String LOGTAG = "Gecko" + RoundedCornerLayout.class.getSimpleName();
    private float cornerRadius;

    private Path path;
    boolean cannotClipPath;

    public RoundedCornerLayout(Context context) {
        super(context);
        init(context);
    }

    public RoundedCornerLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public RoundedCornerLayout(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(context);
    }

    private void init(Context context) {
        // Bug 1201081 - clipPath with hardware acceleration crashes on r11-18.
        cannotClipPath = AppConstants.Versions.feature11Plus && !AppConstants.Versions.feature19Plus;

        final DisplayMetrics metrics = context.getResources().getDisplayMetrics();

        cornerRadius = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_PX,
                getResources().getDimensionPixelSize(R.dimen.doorhanger_rounded_corner_radius), metrics);

        setWillNotDraw(false);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (cannotClipPath) {
            return;
        }

        final RectF r = new RectF(0, 0, w, h);
        path = new Path();
        path.addRoundRect(r, cornerRadius, cornerRadius, Path.Direction.CW);
        path.close();
    }

    @Override
    public void draw(Canvas canvas) {
        if (cannotClipPath) {
            super.draw(canvas);
            return;
        }

        canvas.save();
        canvas.clipPath(path);
        super.draw(canvas);
        canvas.restore();
    }
}
