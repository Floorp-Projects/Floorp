/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.Arrays;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.AttributeSet;
import android.widget.ImageView;

/**
 * A base implementations of the browser toolbar for phones.
 * This class manages any Views, variables, etc. that are exclusive to phone.
 */
abstract class BrowserToolbarPhoneBase extends BrowserToolbar {

    protected final ImageView urlBarTranslatingEdge;

    private final Path roundCornerShape;
    private final Paint roundCornerPaint;

    public BrowserToolbarPhoneBase(final Context context, final AttributeSet attrs) {
        super(context, attrs);
        final Resources res = context.getResources();

        urlBarTranslatingEdge = (ImageView) findViewById(R.id.url_bar_translating_edge);

        // This will clip the translating edge's image at 60% of its width
        urlBarTranslatingEdge.getDrawable().setLevel(6000);

        focusOrder.add(this);
        focusOrder.addAll(urlDisplayLayout.getFocusOrder());
        focusOrder.addAll(Arrays.asList(tabsButton, menuButton));

        roundCornerShape = new Path();
        roundCornerShape.moveTo(0, 0);
        roundCornerShape.lineTo(30, 0);
        roundCornerShape.cubicTo(0, 0, 0, 0, 0, 30);
        roundCornerShape.lineTo(0, 0);

        roundCornerPaint = new Paint();
        roundCornerPaint.setAntiAlias(true);
        roundCornerPaint.setColor(res.getColor(R.color.background_tabs));
        roundCornerPaint.setStrokeWidth(0.0f);
    }

    @Override
    public void draw(final Canvas canvas) {
        super.draw(canvas);

        if (uiMode == UIMode.DISPLAY) {
            canvas.drawPath(roundCornerShape, roundCornerPaint);
        }
    }

    /**
     * Returns the number of pixels the url bar translating edge
     * needs to translate to the right to enter its editing mode state.
     * A negative value means the edge must translate to the left.
     */
    protected int getUrlBarEntryTranslation() {
        // Find the distance from the right-edge of the url bar (where we're translating from) to
        // the left-edge of the cancel button (where we're translating to; note that the cancel
        // button must be laid out, i.e. not View.GONE).
        return editCancel.getLeft() - urlBarEntry.getRight();
    }

    protected int getUrlBarCurveTranslation() {
        return getWidth() - tabsButton.getLeft();
    }
}
