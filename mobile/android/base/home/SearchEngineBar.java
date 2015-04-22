/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 package org.mozilla.gecko.home;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.FrameLayout;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.FaviconView;
import org.mozilla.gecko.widget.TwoWayView;

import java.util.ArrayList;
import java.util.List;

public class SearchEngineBar extends TwoWayView
                             implements AdapterView.OnItemClickListener {
    private static final String LOGTAG = "Gecko" + SearchEngineBar.class.getSimpleName();

    public interface OnSearchBarClickListener {
        public void onSearchBarClickListener(SearchEngine searchEngine);
    }

    private final SearchEngineAdapter adapter;
    private OnSearchBarClickListener onSearchBarClickListener;

    public SearchEngineBar(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        adapter = new SearchEngineAdapter();
        setAdapter(adapter);
        setOnItemClickListener(this);
    }

    @Override
    public void onItemClick(final AdapterView<?> parent, final View view, final int position,
            final long id) {
        if (onSearchBarClickListener == null) {
            throw new IllegalStateException(
                    OnSearchBarClickListener.class.getSimpleName() + " is not initialized");
        }

        final SearchEngine searchEngine = adapter.getItem(position);
        onSearchBarClickListener.onSearchBarClickListener(searchEngine);
    }

    protected void setOnSearchBarClickListener(final OnSearchBarClickListener listener) {
        onSearchBarClickListener = listener;
    }

    protected void setSearchEngines(final List<SearchEngine> searchEngines) {
        adapter.setSearchEngines(searchEngines);
    }

    public class SearchEngineAdapter extends BaseAdapter {
        List<SearchEngine> searchEngines = new ArrayList<>();

        public void setSearchEngines(final List<SearchEngine> searchEngines) {
            this.searchEngines = searchEngines;
            notifyDataSetChanged();
        }

        @Override
        public int getCount() {
            return searchEngines.size();
        }

        @Override
        public SearchEngine getItem(final int position) {
            return searchEngines.get(position);
        }

        @Override
        public long getItemId(final int position) {
            return position;
        }

        @Override
        public View getView(final int position, final View convertView, final ViewGroup parent) {
            final View view;
            if (convertView == null) {
                view = LayoutInflater.from(getContext()).inflate(R.layout.search_engine_bar_item, parent, false);
            } else {
                view = convertView;
            }

            final FaviconView faviconView = (FaviconView) view.findViewById(R.id.search_engine_icon);
            final SearchEngine searchEngine = searchEngines.get(position);
            faviconView.updateAndScaleImage(searchEngine.getIcon(), searchEngine.getEngineIdentifier());

            return view;
        }
    }

    /**
     * A Container to surround the SearchEngineBar. This is necessary so we can draw
     * a divider across the entire width of the screen, but have the inner list layout
     * not take up the full width of the screen so it can be centered within this container
     * if there aren't enough items that it needs to scroll.
     *
     * Note: a better implementation would have this View inflating an inner layout so
     * the containing layout doesn't need two "SearchEngineBar" Views but it wasn't
     * worth the refactor time.
     */
    @SuppressWarnings("unused") // via XML
    public static class SearchEngineBarContainer extends FrameLayout {
        private final Paint dividerPaint;

        public SearchEngineBarContainer(final Context context, final AttributeSet attrs) {
            super(context, attrs);

            dividerPaint = new Paint();
            dividerPaint.setColor(getResources().getColor(R.color.divider_light));
        }

        @Override
        public void onDraw(final Canvas canvas) {
            super.onDraw(canvas);

            canvas.drawLine(0, 0, getWidth(), 0, dividerPaint);
        }
    }
}
