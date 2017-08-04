/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.GridSpacingDecoration;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;

class CompactTabsGridLayout extends TabsGridLayout {
    CompactTabsGridLayout(Context context, AttributeSet attrs) {
        super(context, attrs, 2);

        final Resources resources = context.getResources();
        final int desiredHorizontalItemSpacing = resources.getDimensionPixelSize(R.dimen.tab_panel_grid_hpadding_compact);
        final int viewPaddingVertical = resources.getDimensionPixelSize(R.dimen.tab_panel_grid_vpadding_compact);

        setPadding(desiredHorizontalItemSpacing, viewPaddingVertical, desiredHorizontalItemSpacing, viewPaddingVertical);


        final int horizontalItemPadding = resources.getDimensionPixelSize(R.dimen.tab_panel_grid_item_hpadding_compact);
        final int verticalItemPadding = resources.getDimensionPixelSize(R.dimen.tab_panel_grid_item_vpadding_compact);
        addItemDecoration(new GridSpacingDecoration(horizontalItemPadding, verticalItemPadding));
    }
}
