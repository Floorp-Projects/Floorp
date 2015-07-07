/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 package org.mozilla.gecko.home;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.TwoWayView;

import java.util.ArrayList;
import java.util.List;

public class SearchEngineBar extends TwoWayView
                             implements AdapterView.OnItemClickListener {
    private static final String LOGTAG = "Gecko" + SearchEngineBar.class.getSimpleName();

    private static final float ICON_CONTAINER_MIN_WIDTH_DP = 72;
    private static final float LABEL_CONTAINER_WIDTH_DP = 48;
    private static final float DIVIDER_HEIGHT_DP = 1;

    public interface OnSearchBarClickListener {
        public void onSearchBarClickListener(SearchEngine searchEngine);
    }

    private final SearchEngineAdapter adapter;
    private final Paint dividerPaint;
    private final float minIconContainerWidth;
    private final float dividerHeight;
    private final int labelContainerWidth;

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
        labelContainerWidth = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, LABEL_CONTAINER_WIDTH_DP, displayMetrics);

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

        if (position == 0) {
            // Ignore click on label
            return;
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

        final int searchEngineCount = adapter.getCount() - 1;

        if (searchEngineCount > 0) {
            final float availableWidthPerContainer = (getMeasuredWidth() - labelContainerWidth) / searchEngineCount;

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
        private static final int VIEW_TYPE_SEARCH_ENGINE = 0;
        private static final int VIEW_TYPE_LABEL = 1;

        List<SearchEngine> searchEngines = new ArrayList<>();

        public void setSearchEngines(final List<SearchEngine> searchEngines) {
            this.searchEngines = searchEngines;
            notifyDataSetChanged();
        }

        @Override
        public int getCount() {
            // Adding offset for label at position 0 (Bug 1172071)
            return searchEngines.size() + 1;
        }

        @Override
        public SearchEngine getItem(final int position) {
            // Returning null for the label at position 0 (Bug 1172071)
            return position == 0 ? null : searchEngines.get(position - 1);
        }

        @Override
        public long getItemId(final int position) {
            return position;
        }

        @Override
        public int getItemViewType(int position) {
            return position == 0 ? VIEW_TYPE_LABEL : VIEW_TYPE_SEARCH_ENGINE;
        }

        @Override
        public int getViewTypeCount() {
            return 2;
        }

        @Override
        public View getView(final int position, final View convertView, final ViewGroup parent) {
            if (position == 0) {
                return getLabelView(convertView, parent);
            } else {
                return getSearchEngineView(position, convertView, parent);
            }
        }

        private View getLabelView(View view, final ViewGroup parent) {
            if (view == null) {
                view = LayoutInflater.from(getContext()).inflate(R.layout.search_engine_bar_label, parent, false);
            }

            final Drawable icon = DrawableCompat.wrap(
                    ContextCompat.getDrawable(parent.getContext(), R.drawable.search_icon_active).mutate());
            DrawableCompat.setTint(icon, getResources().getColor(R.color.disabled_grey));

            final ImageView iconView = (ImageView) view.findViewById(R.id.search_engine_label);
            iconView.setImageDrawable(icon);
            iconView.setScaleType(ImageView.ScaleType.FIT_XY);

            return view;
        }

        private View getSearchEngineView(final int position, View view, final ViewGroup parent) {
            if (view == null) {
                view = LayoutInflater.from(getContext()).inflate(R.layout.search_engine_bar_item, parent, false);
            }

            LayoutParams params = (LayoutParams) view.getLayoutParams();
            params.width = iconContainerWidth;
            view.setLayoutParams(params);

            final ImageView faviconView = (ImageView) view.findViewById(R.id.search_engine_icon);
            final SearchEngine searchEngine = getItem(position);
            faviconView.setImageBitmap(searchEngine.getIcon());

            final String desc = getResources().getString(R.string.search_bar_item_desc, searchEngine.getEngineIdentifier());
            view.setContentDescription(desc);

            return view;
        }
    }
}
