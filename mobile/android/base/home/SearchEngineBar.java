/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.content.Intent;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.ColorUtils;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import java.util.List;

public class SearchEngineBar extends RecyclerView
        implements RecyclerViewClickSupport.OnItemClickListener {
    private static final String LOGTAG = SearchEngineBar.class.getSimpleName();

    private static final float ICON_CONTAINER_MIN_WIDTH_DP = 72;
    private static final float LABEL_CONTAINER_WIDTH_DP = 48;
    private static final float DIVIDER_HEIGHT_DP = 1;

    public interface OnSearchBarClickListener {
        void onSearchBarClickListener(SearchEngine searchEngine);
    }

    private final SearchEngineAdapter mAdapter;
    private final LinearLayoutManager mLayoutManager;
    private final Paint mDividerPaint;
    private final float mMinIconContainerWidth;
    private final float mDividerHeight;
    private final int mLabelContainerWidth;

    private int mIconContainerWidth;
    private OnSearchBarClickListener mOnSearchBarClickListener;

    public SearchEngineBar(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        mDividerPaint = new Paint();
        mDividerPaint.setColor(ColorUtils.getColor(context, R.color.divider_light));
        mDividerPaint.setStyle(Paint.Style.FILL_AND_STROKE);

        final DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
        mMinIconContainerWidth = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, ICON_CONTAINER_MIN_WIDTH_DP, displayMetrics);
        mDividerHeight = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, DIVIDER_HEIGHT_DP, displayMetrics);
        mLabelContainerWidth = Math.round(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, LABEL_CONTAINER_WIDTH_DP, displayMetrics));

        mIconContainerWidth = Math.round(mMinIconContainerWidth);

        mAdapter = new SearchEngineAdapter(context);
        mAdapter.setIconContainerWidth(mIconContainerWidth);
        mLayoutManager = new LinearLayoutManager(context);
        mLayoutManager.setOrientation(LinearLayoutManager.HORIZONTAL);

        setAdapter(mAdapter);
        setLayoutManager(mLayoutManager);

        RecyclerViewClickSupport.addTo(this)
            .setOnItemClickListener(this);
    }

    public void setSearchEngines(List<SearchEngine> searchEngines) {
        mAdapter.setSearchEngines(searchEngines);
    }

    public void setOnSearchBarClickListener(OnSearchBarClickListener listener) {
        mOnSearchBarClickListener = listener;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        final int searchEngineCount = mAdapter.getItemCount() - 1;

        if (searchEngineCount > 0) {
            final int availableWidth = getMeasuredWidth() - mLabelContainerWidth;
            final double searchEnginesToDisplay;

            if (searchEngineCount * mMinIconContainerWidth <= availableWidth) {
                // All search engines fit int: So let's just display all.
                searchEnginesToDisplay = searchEngineCount;
            } else {
                // If only (n) search engines fit into the available space then display (n - 0.5): The last search
                // engine will be cut-off to show ability to scroll this view
                searchEnginesToDisplay = Math.floor(availableWidth / mMinIconContainerWidth) - 0.5;
            }

            // Use all available width and spread search engine icons
            final int availableWidthPerContainer = (int) (availableWidth / searchEnginesToDisplay);

            if (availableWidthPerContainer != mIconContainerWidth) {
                mIconContainerWidth = availableWidthPerContainer;
            }
            mAdapter.setIconContainerWidth(mIconContainerWidth);
        }
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        canvas.drawRect(0, 0, getWidth(), mDividerHeight, mDividerPaint);
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        if (mOnSearchBarClickListener == null) {
            throw new IllegalStateException(
                    OnSearchBarClickListener.class.getSimpleName() + " is not initializer."
            );
        }

        if (position == 0) {
            final Intent settingsIntent = new Intent(getContext(), GeckoPreferences.class);
            GeckoPreferences.setResourceToOpen(settingsIntent, "preferences_search");
            getContext().startActivity(settingsIntent);
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "searchenginebar-settings");
            return;
        }

        final SearchEngine searchEngine = mAdapter.getItem(position);
        mOnSearchBarClickListener.onSearchBarClickListener(searchEngine);
    }

    /**
     * We manually add the override for getAdapter because we see this method getting stripped
     * out during compile time by aggressive proguard rules.
     */
    @RobocopTarget
    @Override
    public SearchEngineAdapter getAdapter() {
        return mAdapter;
    }
}
