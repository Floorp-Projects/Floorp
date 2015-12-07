/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.animation.ViewHelper;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ViewFlipper;

/* This extends the normal ViewFlipper only to fix bug 956075 on < 3.0 devices.
 * i.e. It ignores touch events on the ViewFlipper when its hidden. */

public class GeckoViewFlipper extends ViewFlipper {
    private final Rect mRect = new Rect();

    public GeckoViewFlipper(Context context) {
        super(context);
    }

    public GeckoViewFlipper(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (Versions.preHC) {
            // Fix bug 956075. Don't allow touching this View if its hidden.
            getHitRect(mRect);
            mRect.offset((int) ViewHelper.getTranslationX((View)getParent()), (int) ViewHelper.getTranslationY((View)getParent()));

            if (!mRect.contains((int) ev.getX(), (int) ev.getY())) {
                return false;
            }
        }

        return super.dispatchTouchEvent(ev);
    }
}
