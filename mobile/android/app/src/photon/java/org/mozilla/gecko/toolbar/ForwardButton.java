/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.graphics.Path;
import android.graphics.RectF;
import android.util.AttributeSet;

import org.mozilla.gecko.R;

public class ForwardButton extends NavButton {
    public ForwardButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        mPath.reset();
        mPath.setFillType(Path.FillType.INVERSE_EVEN_ODD);

        final RectF rect = new RectF(0, 0, width, height);
        final int radius = getResources().getDimensionPixelSize(R.dimen.browser_toolbar_menu_radius);
        mPath.addRoundRect(rect, radius, radius, Path.Direction.CW);

    }
}
