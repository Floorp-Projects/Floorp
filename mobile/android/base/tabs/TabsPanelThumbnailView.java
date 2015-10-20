/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.ThumbnailHelper;
import org.mozilla.gecko.widget.CropImageView;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

/**
 *  A width constrained ImageView to show thumbnails of open tabs in the tabs panel.
 */
public class TabsPanelThumbnailView extends CropImageView {
    public static final String LOGTAG = "Gecko" + TabsPanelThumbnailView.class.getSimpleName();


    public TabsPanelThumbnailView(final Context context) {
        this(context, null);
    }

    public TabsPanelThumbnailView(final Context context, final AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TabsPanelThumbnailView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected float getAspectRatio() {
        return ThumbnailHelper.TABS_PANEL_THUMBNAIL_ASPECT_RATIO;
    }

    @Override
    public void setImageDrawable(Drawable drawable) {
        boolean resize = true;

        if (drawable == null) {
            drawable = getResources().getDrawable(R.drawable.tab_panel_tab_background);
            resize = false;
            setScaleType(ScaleType.FIT_XY);
        }

        super.setImageDrawable(drawable, resize);
    }
}
