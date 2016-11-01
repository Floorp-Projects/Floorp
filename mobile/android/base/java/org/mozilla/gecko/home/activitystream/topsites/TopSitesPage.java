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
        extends RecyclerView {
    public TopSitesPage(Context context,
                        @Nullable AttributeSet attrs) {
        super(context, attrs);

        setLayoutManager(new GridLayoutManager(context, 1));
    }

    public void setTiles(int tiles) {
        setLayoutManager(new GridLayoutManager(getContext(), tiles));
    }

    private HomePager.OnUrlOpenListener onUrlOpenListener;
    private HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    public TopSitesPageAdapter getAdapter() {
        return (TopSitesPageAdapter) super.getAdapter();
    }
}
