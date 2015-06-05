/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 package org.mozilla.gecko.home;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ImageView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.TwoWayView;

import java.util.ArrayList;
import java.util.List;

public class SearchEngineBar extends TwoWayView
                             implements AdapterView.OnItemClickListener {
    private static final String LOGTAG = "Gecko" + SearchEngineBar.class.getSimpleName();

    private static final float ICON_CONTAINER_MIN_WIDTH_DP = 72;
    private static final float DIVIDER_HEIGHT_DP = 1;

    public interface OnSearchBarClickListener {
        public void onSearchBarClickListener(SearchEngine searchEngine);
    }

    private final SearchEngineAdapter adapter;
    private final Paint dividerPaint;
    private final float minIconContainerWidth;
    private final float dividerHeight;

    private int iconContainerWidth;
    private OnSearchBarClickListener onSearchBarClickListener;

    public SearchEngineBar(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        dividerPaint = new Paint();
        dividerPaint.setColor(getResources().getColor(R.color.divider_light));
        dividerPaint.setStyle(Paint.Style.FILL_AND_STROKE);

        final DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
        minIconContainerWidth = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, ICON_CONTAINER_MIN_WIDTH_DP, displayMetrics);
        dividerHeight = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, DIVIDER_HEIGHT_DP, displayMetrics);

        iconContainerWidth =  (int) minIconContainerWidth;

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

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        final int searchEngineCount = getCount();

        if (searchEngineCount > 0) {
            final float availableWidthPerContainer = getMeasuredWidth() / searchEngineCount;

            final int desiredIconContainerSize = (int) Math.max(
                    availableWidthPerContainer,
                    minIconContainerWidth
            );

            if (desiredIconContainerSize != iconContainerWidth) {
                iconContainerWidth = desiredIconContainerSize;
                adapter.notifyDataSetChanged();
            }
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        canvas.drawRect(0, 0, getWidth(), dividerHeight, dividerPaint);
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

            view.setLayoutParams(new LayoutParams(iconContainerWidth, ViewGroup.LayoutParams.MATCH_PARENT));

            final ImageView faviconView = (ImageView) view.findViewById(R.id.search_engine_icon);
            final SearchEngine searchEngine = searchEngines.get(position);
            faviconView.setImageBitmap(searchEngine.getIcon());

            final String desc = getResources().getString(R.string.search_bar_item_desc, searchEngine.getEngineIdentifier());
            view.setContentDescription(desc);

            return view;
        }
    }
}
