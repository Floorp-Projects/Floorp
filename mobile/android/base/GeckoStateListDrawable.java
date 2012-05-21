/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.graphics.Color;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.StateListDrawable;
import android.graphics.LightingColorFilter;

public class GeckoStateListDrawable extends StateListDrawable {
    private LightingColorFilter mFilter;

    public void initializeFilter(int color) {
        mFilter = new LightingColorFilter(Color.WHITE, color);
    }

    @Override
    protected boolean onStateChange(int[] stateSet) {
        for (int state: stateSet) {
            if (state == android.R.attr.state_pressed || state == android.R.attr.state_focused) {
                super.onStateChange(stateSet);
                ((LayerDrawable) getCurrent()).getDrawable(0).setColorFilter(mFilter);
                return true;
            }
        }

        return super.onStateChange(stateSet);
    }
}
