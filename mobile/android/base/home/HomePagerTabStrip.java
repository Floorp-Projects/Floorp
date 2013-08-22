/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.content.res.TypedArray;
import android.support.v4.view.PagerTabStrip;
import android.util.AttributeSet;

import org.mozilla.gecko.R;

/**
 * HomePagerTabStrip is a custom implementation of PagerTabStrip
 * that exposes XML attributes for the public methods.
 */

class HomePagerTabStrip extends PagerTabStrip {

    public HomePagerTabStrip(Context context) {
        super(context);
    }

    public HomePagerTabStrip(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.HomePagerTabStrip);
        int color = a.getColor(R.styleable.HomePagerTabStrip_tabIndicatorColor, 0x00);
        a.recycle();

        setTabIndicatorColor(color);
    }
}
