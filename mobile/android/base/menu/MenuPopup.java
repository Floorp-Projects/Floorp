/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;
import android.widget.RelativeLayout.LayoutParams;

/**
 * A popup to show the inflated MenuPanel. This has an arrow pointing to the anchor.
 */
public class MenuPopup extends PopupWindow {
    private Resources mResources;

    private ImageView mArrowTop;
    private ImageView mArrowBottom;
    private RelativeLayout mPanel;

    private int mYOffset;
    private int mArrowMargin;
    private int mPopupWidth;
    private boolean mShowArrow;

    public MenuPopup(Context context) {
        super(context);
        mResources = context.getResources();

        setFocusable(true);

        mYOffset = mResources.getDimensionPixelSize(R.dimen.menu_popup_offset);
        mArrowMargin = mResources.getDimensionPixelSize(R.dimen.menu_popup_arrow_margin);
        mPopupWidth = mResources.getDimensionPixelSize(R.dimen.menu_popup_width);

        // Setting a null background makes the popup to not close on touching outside.
        setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        setWindowLayoutMode(View.MeasureSpec.makeMeasureSpec(mPopupWidth, View.MeasureSpec.AT_MOST),
                            ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(context);
        RelativeLayout layout = (RelativeLayout) inflater.inflate(R.layout.menu_popup, null);
        setContentView(layout);

        mArrowTop = (ImageView) layout.findViewById(R.id.menu_arrow_top);
        mArrowBottom = (ImageView) layout.findViewById(R.id.menu_arrow_bottom);
        mPanel = (RelativeLayout) layout.findViewById(R.id.menu_panel);
        mShowArrow = true;
        setAnimationStyle(R.style.PopupAnimation);
    }

    /**
     * Adds the panel with the menu to its content.
     *
     * @param view The panel view with the menu to be shown.
     */
    public void setPanelView(View view) {
        mPanel.removeAllViews();
        mPanel.addView(view);
        mPanel.measure(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    /**
     * Show/hide the arrow pointing to the anchor.
     *
     * @param show Show/hide the arrow.
     */
    public void showArrowToAnchor(boolean show) {
        mShowArrow = show;
    }

    /**
     * A small little offset for the arrow to overlap the anchor.
     */
    @Override
    public void showAsDropDown(View anchor) {
        if (!mShowArrow) {
            mArrowTop.setVisibility(View.GONE);
            mArrowBottom.setVisibility(View.GONE);
            showAsDropDown(anchor, 0, -mYOffset);
            return;
        }

        int[] anchorLocation = new int[2];
        anchor.getLocationOnScreen(anchorLocation);

        int screenWidth = mResources.getDisplayMetrics().widthPixels;
        int arrowWidth = mResources.getDimensionPixelSize(R.dimen.menu_popup_arrow_width);
        int arrowOffset = (anchor.getWidth() - arrowWidth)/2;

        if (anchorLocation[0] + mPopupWidth <= screenWidth) {
            // left align
            ((LayoutParams) mArrowTop.getLayoutParams()).rightMargin = mPopupWidth - anchor.getWidth() + arrowOffset;
            ((LayoutParams) mArrowBottom.getLayoutParams()).rightMargin = mPopupWidth - anchor.getWidth() + arrowOffset;
        } else {
            // right align
            ((LayoutParams) mArrowTop.getLayoutParams()).rightMargin = screenWidth - anchorLocation[0] - anchor.getWidth()/2 - arrowWidth/2;
            ((LayoutParams) mArrowBottom.getLayoutParams()).rightMargin = mArrowMargin;
        }

        // shown below anchor
        mArrowTop.setVisibility(View.VISIBLE);
        mArrowBottom.setVisibility(View.GONE);
        showAsDropDown(anchor, 0, -mYOffset);
    }
}
