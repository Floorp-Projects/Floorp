/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.widget;

import android.content.Context;
import android.support.v7.widget.CardView;
import android.util.AttributeSet;

import org.mozilla.gecko.AppConstants;

/**
 * CardView that ensures its content can fill the entire card. Use this instead of CardView
 * if you want to fill the card with e.g. images, backgrounds, etc.
 *
 * On API < 21, CardView content isn't clipped for performance reasons. We work around this by disabling
 * rounded corners on those devices.
 */
public class FilledCardView extends CardView {

    public FilledCardView(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Disable corners on < lollipop:
        // CardView only supports clipping content on API >= 21 (for performance reasons). Without
        // content clipping, any cards that provide their own content that fills the card will look
        // ugly: by default there is a 2px white edge along the top and sides (i.e. an inset corresponding
        // to the corner radius), if we disable the inset then the corners overlap.
        // It's possible to implement custom clipping, however given that the support library
        // chose not to support this for performance reasons, we too have chosen to just disable
        // corners on < 21, see Bug 1271428.
        if (AppConstants.Versions.preLollipop) {
            setRadius(0);
        }

        setUseCompatPadding(true);
    }
}
