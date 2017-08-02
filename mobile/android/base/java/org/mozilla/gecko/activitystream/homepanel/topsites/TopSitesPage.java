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

import org.mozilla.gecko.activitystream.homepanel.ActivityStreamPanel;
import org.mozilla.gecko.home.HomePager;

public class TopSitesPage extends RecyclerView {
    public TopSitesPage(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        setLayoutManager(new GridLayoutManager(getContext(), ActivityStreamPanel.TOP_SITES_COLUMNS));
    }

    public TopSitesPageAdapter getAdapter() {
        return (TopSitesPageAdapter) super.getAdapter();
    }
}
