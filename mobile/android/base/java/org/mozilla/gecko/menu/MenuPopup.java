/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.PopupWindow;

/**
 * A popup to show the inflated MenuPanel.
 */
public class MenuPopup extends PopupWindow {
    private final FrameLayout mPanel;

    private final int mYOffset;
    private final int mPopupWidth;
    private final int mPopupMinHeight;

    public MenuPopup(Context context) {
        super(context);

        setFocusable(true);

        mYOffset = context.getResources().getDimensionPixelSize(R.dimen.menu_popup_offset);
        mPopupWidth = context.getResources().getDimensionPixelSize(R.dimen.menu_popup_width);
        mPopupMinHeight = context.getResources().getDimensionPixelSize(R.dimen.menu_item_row_height);

        // Setting a null background makes the popup to not close on touching outside.
        setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        setWindowLayoutMode(ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(context);
        mPanel = (FrameLayout) inflater.inflate(R.layout.menu_popup, null);
        setContentView(mPanel);

        setAnimationStyle(R.style.PopupAnimation);
    }

    /**
     * Adds the panel with the menu to its content.
     *
     * @param view The panel view with the menu to be shown.
     */
    public void setPanelView(View view) {
        view.setLayoutParams(new FrameLayout.LayoutParams(mPopupWidth,
                                                          FrameLayout.LayoutParams.WRAP_CONTENT));

        mPanel.removeAllViews();
        mPanel.addView(view);
    }

    /**
     * A small little offset.
     */
    @Override
    public void showAsDropDown(View anchor) {
        // Set a height, so that the popup will not be displayed below the bottom of the screen.
        // We use the exact height of the internal content, which is the technique described in
        // http://stackoverflow.com/a/7698709
        setHeight(mPanel.getHeight());

        // Attempt to align the center of the popup with the center of the anchor. If the anchor is
        // near the edge of the screen, the popup will just align with the edge of the screen.
        final int xOffset = anchor.getWidth() / 2 - mPopupWidth / 2;
        showAsDropDown(anchor, xOffset, -mYOffset);
    }
}
