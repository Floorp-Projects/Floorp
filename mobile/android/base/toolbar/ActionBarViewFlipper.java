/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.NewTabletUI;
import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.GeckoViewFlipper;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;

//TODO: (bug 1058909) Remove this class when old tablet is removed.
/**
 * A temporary view that sets its height based on whether we are on new tablet or not.
 * Note that this view should be removed when the old tablet is removed and replaced with using
 * browser_toolbar_height directly.
 */
public class ActionBarViewFlipper extends GeckoViewFlipper {

    public ActionBarViewFlipper(final Context context, final AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        if (NewTabletUI.isEnabled(getContext())) {
            final ViewGroup.LayoutParams lp = getLayoutParams();
            lp.height = getResources().getDimensionPixelSize(R.dimen.new_tablet_browser_toolbar_height);
            setLayoutParams(lp);
        }
    }
}
