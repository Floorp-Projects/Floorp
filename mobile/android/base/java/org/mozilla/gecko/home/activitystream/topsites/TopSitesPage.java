/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.topsites;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;

import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import java.util.EnumSet;

public class TopSitesPage
        extends RecyclerView
        implements RecyclerViewClickSupport.OnItemClickListener {
    public TopSitesPage(Context context,
                        @Nullable AttributeSet attrs) {
        super(context, attrs);

        setLayoutManager(new GridLayoutManager(context, TopSitesPagerAdapter.GRID_WIDTH));

        RecyclerViewClickSupport.addTo(this)
                .setOnItemClickListener(this);
    }

    private HomePager.OnUrlOpenListener onUrlOpenListener = null;

    public TopSitesPageAdapter getAdapter() {
        return (TopSitesPageAdapter) super.getAdapter();
    }

    public void setOnUrlOpenListener(HomePager.OnUrlOpenListener onUrlOpenListener) {
        this.onUrlOpenListener = onUrlOpenListener;
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        if (onUrlOpenListener != null) {
            final String url = getAdapter().getURLForPosition(position);

            onUrlOpenListener.onUrlOpen(url, EnumSet.noneOf(HomePager.OnUrlOpenListener.Flags.class));
        }
    }
}
