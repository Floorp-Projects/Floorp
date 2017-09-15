/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.topsites;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;

public class TopSitesPage extends RecyclerView {

    public static final int NUM_COLUMNS = 4;
    public static final int NUM_ROWS = 2;
    public static final int NUM_TILES = NUM_COLUMNS * NUM_ROWS;

    public TopSitesPage(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        setLayoutManager(new GridLayoutManager(getContext(), NUM_COLUMNS));
    }

    public TopSitesPageAdapter getAdapter() {
        return (TopSitesPageAdapter) super.getAdapter();
    }
}
