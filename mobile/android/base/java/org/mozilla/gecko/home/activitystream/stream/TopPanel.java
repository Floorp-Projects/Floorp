/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home.activitystream.stream;

import android.content.res.Resources;
import android.database.Cursor;
import android.support.v4.view.ViewPager;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.activitystream.topsites.CirclePageIndicator;
import org.mozilla.gecko.home.activitystream.topsites.TopSitesPagerAdapter;

public class TopPanel extends StreamItem {
    public static final int LAYOUT_ID = R.layout.activity_stream_main_toppanel;

    private final ViewPager topSitesPager;

    public TopPanel(View itemView, HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        super(itemView);

        topSitesPager = (ViewPager) itemView.findViewById(R.id.topsites_pager);
        topSitesPager.setAdapter(new TopSitesPagerAdapter(itemView.getContext(), onUrlOpenListener, onUrlOpenInBackgroundListener));

        CirclePageIndicator indicator = (CirclePageIndicator) itemView.findViewById(R.id.topsites_indicator);
        indicator.setViewPager(topSitesPager);
    }

    public void bind(Cursor cursor, int tiles, int tilesWidth, int tilesHeight) {
        final TopSitesPagerAdapter adapter = (TopSitesPagerAdapter) topSitesPager.getAdapter();
        adapter.setTilesSize(tiles, tilesWidth, tilesHeight);
        adapter.swapCursor(cursor);

        final Resources resources = itemView.getResources();
        final int tilesMargin = resources.getDimensionPixelSize(R.dimen.activity_stream_base_margin);
        final int textHeight = resources.getDimensionPixelSize(R.dimen.activity_stream_top_sites_text_height);

        ViewGroup.LayoutParams layoutParams = topSitesPager.getLayoutParams();
        layoutParams.height = tilesHeight + tilesMargin + textHeight;
        topSitesPager.setLayoutParams(layoutParams);
    }
}