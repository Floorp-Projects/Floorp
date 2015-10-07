/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.view.ViewGroup;
import android.widget.LinearLayout;

import android.content.res.ColorStateList;
import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;

/**
 * {@code TabMenuStripLayout} is the view that draws the {@code HomePager}
 * tabs that are displayed in {@code TabMenuStrip}.
 */
class TabMenuStripLayout extends LinearLayout
                         implements View.OnFocusChangeListener {

    private TabMenuStrip.OnTitleClickListener onTitleClickListener;
    private Drawable strip;
    private TextView selectedView;

    // Data associated with the scrolling of the strip drawable.
    private View toTab;
    private View fromTab;
    private int fromPosition;
    private int toPosition;
    private float progress;

    // This variable is used to predict the direction of scroll.
    private float prevProgress;
    private int tabContentStart;
    private boolean titlebarFill;
    private int activeTextColor;
    private ColorStateList inactiveTextColor;

    TabMenuStripLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabMenuStrip);
        final int stripResId = a.getResourceId(R.styleable.TabMenuStrip_strip, -1);

        titlebarFill = a.getBoolean(R.styleable.TabMenuStrip_titlebarFill, false);
        tabContentStart = a.getDimensionPixelSize(R.styleable.TabMenuStrip_tabContentStart, 0);
        activeTextColor = a.getColor(R.styleable.TabMenuStrip_activeTextColor, R.color.text_and_tabs_tray_grey);
        inactiveTextColor = a.getColorStateList(R.styleable.TabMenuStrip_inactiveTextColor);
        a.recycle();

        if (stripResId != -1) {
            strip = getResources().getDrawable(stripResId);
        }

        setWillNotDraw(false);
    }

    void onAddPagerView(String title) {
        final TextView button = (TextView) LayoutInflater.from(getContext()).inflate(R.layout.tab_menu_strip, this, false);
        button.setText(title.toUpperCase());
        button.setTextColor(inactiveTextColor);

        // Set titles width to weight, or wrap text width.
        if (titlebarFill) {
            button.setLayoutParams(new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f));
        } else {
            button.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }

        if (getChildCount() == 0) {
            button.setPadding(button.getPaddingLeft() + tabContentStart,
                              button.getPaddingTop(),
                              button.getPaddingRight(),
                              button.getPaddingBottom());
        }

        addView(button);
        button.setOnClickListener(new ViewClickListener(getChildCount() - 1));
        button.setOnFocusChangeListener(this);
    }

    void onPageSelected(final int position) {
        if (selectedView != null) {
            selectedView.setTextColor(inactiveTextColor);
        }

        selectedView = (TextView) getChildAt(position);
        selectedView.setTextColor(activeTextColor);

        // Callback to measure and draw the strip after the view is visible.
        ViewTreeObserver vto = selectedView.getViewTreeObserver();
        if (vto.isAlive()) {
            vto.addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
                @Override
                public void onGlobalLayout() {
                    selectedView.getViewTreeObserver().removeGlobalOnLayoutListener(this);

                    if (strip != null) {
                        strip.setBounds(selectedView.getLeft() + (position == 0 ? tabContentStart : 0),
                                        selectedView.getTop(),
                                        selectedView.getRight(),
                                        selectedView.getBottom());
                    }

                    prevProgress = position;
                }
            });
        }
    }

    // Page scroll animates the drawable and its bounds from the previous to next child view.
    void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
        if (strip == null) {
            return;
        }

        setScrollingData(position, positionOffset);

        if (fromTab == null || toTab == null) {
            return;
        }

        final int fromTabLeft =  fromTab.getLeft();
        final int fromTabRight = fromTab.getRight();

        final int toTabLeft =  toTab.getLeft();
        final int toTabRight = toTab.getRight();

        // The first tab has a padding applied (tabContentStart). We don't want the 'strip' to jump around so we remove
        // this padding slowly (modifier) when scrolling to or from the first tab.
        final int modifier;

        if (fromPosition == 0 && toPosition == 1) {
            // Slowly remove extra padding (tabContentStart) based on scroll progress
            modifier = (int) (tabContentStart * (1 - progress));
        } else if (fromPosition == 1 && toPosition == 0) {
            // Slowly add extra padding (tabContentStart) based on scroll progress
            modifier = (int) (tabContentStart * progress);
        } else {
            // We are not scrolling tab 0 in any way, no modifier needed
            modifier = 0;
        }

        strip.setBounds((int) (fromTabLeft + ((toTabLeft - fromTabLeft) * progress)) + modifier,
                0,
                (int) (fromTabRight + ((toTabRight - fromTabRight) * progress)),
                getHeight());
        invalidate();
    }

    /*
     * position + positionOffset goes from 0 to 2 as we scroll from page 1 to 3.
     * Normalized progress is relative to the the direction the page is being scrolled towards.
     * For this, we maintain direction of scroll with a state, and the child view we are moving towards and away from.
     */
    void setScrollingData(int position, float positionOffset) {
        if (position >= getChildCount() - 1) {
            return;
        }

        final float currProgress = position + positionOffset;

        if (prevProgress > currProgress) {
            toPosition = position;
            fromPosition = position + 1;
            progress = 1 - positionOffset;
        } else {
            toPosition = position + 1;
            fromPosition = position;
            progress = positionOffset;
        }

        toTab = getChildAt(toPosition);
        fromTab = getChildAt(fromPosition);

        prevProgress = currProgress;
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (strip != null) {
            strip.draw(canvas);
        }
    }

    @Override
    public void onFocusChange(View v, boolean hasFocus) {
        if (v == this && hasFocus && getChildCount() > 0) {
            selectedView.requestFocus();
            return;
        }

        if (!hasFocus) {
            return;
        }

        int i = 0;
        final int numTabs = getChildCount();

        while (i < numTabs) {
            View view = getChildAt(i);
            if (view == v) {
                view.requestFocus();
                if (isShown()) {
                    // A view is focused so send an event to announce the menu strip state.
                    sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
                }
                break;
            }

            i++;
        }
    }

    void setOnTitleClickListener(TabMenuStrip.OnTitleClickListener onTitleClickListener) {
        this.onTitleClickListener = onTitleClickListener;
    }

    private class ViewClickListener implements OnClickListener {
        private final int mIndex;

        public ViewClickListener(int index) {
            mIndex = index;
        }

        @Override
        public void onClick(View view) {
            if (onTitleClickListener != null) {
                onTitleClickListener.onTitleClicked(mIndex);
            }
        }
    }
}
