/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.R;
import org.mozilla.gecko.lwt.LightweightThemeDrawable;
import org.mozilla.gecko.util.ColorUtils;
import org.mozilla.gecko.widget.themed.ThemedImageButton;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

public class ShapedButton extends ThemedImageButton
                          implements CanvasDelegate.DrawManager {

    protected final Path mPath;
    protected final CanvasDelegate mCanvasDelegate;

    public ShapedButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Path is clipped.
        mPath = new Path();

        final Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setColor(ColorUtils.getColor(context, R.color.canvas_delegate_paint));
        paint.setStrokeWidth(0.0f);
        mCanvasDelegate = new CanvasDelegate(this, Mode.DST_IN, paint);

        setWillNotDraw(false);
    }

    @Override
    public void draw(Canvas canvas) {
        if (mCanvasDelegate != null)
            mCanvasDelegate.draw(canvas, mPath, getWidth(), getHeight());
        else
            defaultDraw(canvas);
    }

    @Override
    public void defaultDraw(Canvas canvas) {
        super.draw(canvas);
    }

    // The drawable is constructed as per @drawable/shaped_button.
    @Override
    public void onLightweightThemeChanged() {
        final int background = ColorUtils.getColor(getContext(), R.color.text_and_tabs_tray_grey);
        final LightweightThemeDrawable lightWeight = getTheme().getColorDrawable(this, background);

        if (lightWeight == null)
            return;

        lightWeight.setAlpha(34, 34);

        final StateListDrawable stateList = new StateListDrawable();
        stateList.addState(PRESSED_ENABLED_STATE_SET, getColorDrawable(R.color.highlight_shaped));
        stateList.addState(FOCUSED_STATE_SET, getColorDrawable(R.color.highlight_shaped_focused));
        stateList.addState(PRIVATE_STATE_SET, getColorDrawable(R.color.text_and_tabs_tray_grey));
        stateList.addState(EMPTY_STATE_SET, lightWeight);

        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.shaped_button);
    }

    @Override
    public void setBackgroundDrawable(Drawable drawable) {
        if (getBackground() == null || drawable == null) {
            super.setBackgroundDrawable(drawable);
            return;
        }

        int[] padding =  new int[] { getPaddingLeft(),
                                     getPaddingTop(),
                                     getPaddingRight(),
                                     getPaddingBottom()
                                   };
        drawable.setLevel(getBackground().getLevel());
        super.setBackgroundDrawable(drawable);

        setPadding(padding[0], padding[1], padding[2], padding[3]);
    }

    @Override
    public void setBackgroundResource(int resId) {
        setBackgroundDrawable(getResources().getDrawable(resId));
    }
}
