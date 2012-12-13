/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
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

    private ImageView mArrow;
    private RelativeLayout mPanel;

    private int mYOffset;
    private int mArrowMargin;
    private int mPopupWidth;

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

        mArrow = (ImageView) layout.findViewById(R.id.menu_arrow);
        mPanel = (RelativeLayout) layout.findViewById(R.id.menu_panel);
    }

    /**
     * Adds the panel with the menu to its content.
     *
     * @param view The panel view with the menu to be shown.
     */
    public void setPanelView(View view) {
        mPanel.removeAllViews();
        mPanel.addView(view);
    }

    /**
     * A small little offset for the arrow to overlap the anchor.
     */
    @Override
    public void showAsDropDown(View anchor) {
        int[] anchorLocation = new int[2];
        anchor.getLocationOnScreen(anchorLocation);

        int screenWidth = mResources.getDisplayMetrics().widthPixels;
        int arrowWidth = mResources.getDimensionPixelSize(R.dimen.menu_popup_arrow_width);
        LayoutParams params = (LayoutParams) mArrow.getLayoutParams();
        int arrowOffset = (anchor.getWidth() - arrowWidth)/2;
       
        if (anchorLocation[0] + mPopupWidth <= screenWidth) {
            // left align
            params.rightMargin = mPopupWidth - anchor.getWidth() + arrowOffset;
        } else {
            // right align
            params.rightMargin = mArrowMargin;
        }

        showAsDropDown(anchor, 0, -mYOffset);
    }
}
