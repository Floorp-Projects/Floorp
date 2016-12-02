/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.topsites;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.view.PagerAdapter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomePager;

import java.util.LinkedList;

/**
 * The primary / top-level TopSites adapter: it handles the ViewPager, and also handles
 * all lower-level Adapters that populate the individual topsite items.
 */
public class TopSitesPagerAdapter extends PagerAdapter {
    public static final int PAGES = 4;
    public static final int SUGGESTED_SITES_MAX_PAGES = 2;

    private int tiles;
    private int tilesWidth;
    private int tilesHeight;

    private LinkedList<TopSitesPage> pages = new LinkedList<>();

    private final Context context;
    private final HomePager.OnUrlOpenListener onUrlOpenListener;
    private final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    private int count = 0;

    public TopSitesPagerAdapter(Context context,
                                HomePager.OnUrlOpenListener onUrlOpenListener,
                                HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        this.context = context;
        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;
    }

    public void setTilesSize(int tiles, int tilesWidth, int tilesHeight) {
        this.tilesWidth = tilesWidth;
        this.tilesHeight = tilesHeight;
        this.tiles = tiles;
    }

    @Override
    public int getCount() {
        return Math.min(count, 4);
    }

    @Override
    public boolean isViewFromObject(View view, Object object) {
        return view == object;
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        TopSitesPage page = pages.get(position);

        container.addView(page);

        return page;
    }

    @Override
    public int getItemPosition(Object object) {
        return PagerAdapter.POSITION_NONE;
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        container.removeView((View) object);
    }

    public void swapCursor(Cursor cursor) {
        // Divide while rounding up: 0 items = 0 pages, 1-ITEMS_PER_PAGE items = 1 page, etc.
        if (cursor != null) {
            count = (cursor.getCount() - 1) / tiles + 1;
        } else {
            count = 0;
        }

        pages.clear();
        final int pageDelta = count;

        if (pageDelta > 0) {
            final LayoutInflater inflater = LayoutInflater.from(context);
            for (int i = 0; i < pageDelta; i++) {
                final TopSitesPage page = (TopSitesPage) inflater.inflate(R.layout.activity_stream_topsites_page, null, false);

                page.setTiles(tiles);
                final TopSitesPageAdapter adapter = new TopSitesPageAdapter(context, tiles, tilesWidth, tilesHeight,
                        onUrlOpenListener, onUrlOpenInBackgroundListener);
                page.setAdapter(adapter);
                pages.add(page);
            }
        } else if (pageDelta < 0) {
            for (int i = 0; i > pageDelta; i--) {
                final TopSitesPage page = pages.getLast();

                // Ensure the page doesn't use the old/invalid cursor anymore
                page.getAdapter().swapCursor(null, 0);

                pages.removeLast();
            }
        } else {
            // do nothing: we will be updating all the pages below
        }

        int startIndex = 0;
        for (TopSitesPage page : pages) {
            page.getAdapter().swapCursor(cursor, startIndex);
            startIndex += tiles;
        }

        notifyDataSetChanged();
    }
}
