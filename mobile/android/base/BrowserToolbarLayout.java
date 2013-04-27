/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.RelativeLayout;

public class BrowserToolbarLayout extends GeckoRelativeLayout {
    private static final String LOGTAG = "GeckoToolbarLayout";

    public BrowserToolbarLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // If the motion event has occured below the toolbar (due to the scroll
        // offset), let it pass through to the page.
        if (event != null && event.getY() > getHeight() - getScrollY()) {
            return false;
        }

        return super.onTouchEvent(event);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        if (h != oldh) {
            // Post this to happen outside of onSizeChanged, as this may cause
            // a layout change and relayouts within a layout change don't work.
            final int height = h;
            post(new Runnable() {
                @Override
                public void run() {
                    ((BrowserApp)GeckoApp.mAppContext).refreshToolbarHeight();
                }
            });
        }
    }
}

