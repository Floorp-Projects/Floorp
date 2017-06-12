/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.firstrun;

import android.content.Context;
import android.content.res.Resources;
import android.support.v7.widget.CardView;
import android.util.AttributeSet;

import org.mozilla.focus.R;

public class FirstrunCardView extends CardView {
    private int maxWidth;
    private int maxHeight;

    public FirstrunCardView(Context context) {
        super(context);
        init();
    }

    public FirstrunCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public FirstrunCardView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        final Resources resources = getResources();

        maxWidth = resources.getDimensionPixelSize(R.dimen.firstrun_card_width);
        maxHeight = resources.getDimensionPixelSize(R.dimen.firstrun_card_height);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // The view is set to match_parent in the layout file. So width and height should be the
        // value needed to fill the whole parent view.
        final int availableWidth = MeasureSpec.getSize(widthMeasureSpec);
        final int availableHeight = MeasureSpec.getSize(heightMeasureSpec);

        // Now let's use those sizes to measure - but let's not exceed our defined max sizes (We do
        // not want to have gigantic cards on large devices like tablets.)
        final int measuredWidth = Math.min(availableWidth, maxWidth);
        final int measuredHeight = Math.min(availableHeight, maxHeight);

        // Let's use the measured values to hand them to the super class to measure the child views etc.
        widthMeasureSpec = MeasureSpec.makeMeasureSpec(measuredWidth, MeasureSpec.EXACTLY);
        heightMeasureSpec = MeasureSpec.makeMeasureSpec(measuredHeight, MeasureSpec.EXACTLY);

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
