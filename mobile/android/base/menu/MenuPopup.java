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
import android.widget.LinearLayout;
import android.widget.PopupWindow;

/**
 * A popup to show the inflated MenuPanel.
 */
public class MenuPopup extends PopupWindow {
    private LinearLayout mPanel;

    private int mYOffset;
    private int mPopupWidth;

    public MenuPopup(Context context) {
        super(context);

        setFocusable(true);

        mYOffset = context.getResources().getDimensionPixelSize(R.dimen.menu_popup_offset);
        mPopupWidth = context.getResources().getDimensionPixelSize(R.dimen.menu_popup_width);

        // Setting a null background makes the popup to not close on touching outside.
        setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        setWindowLayoutMode(View.MeasureSpec.makeMeasureSpec(mPopupWidth, View.MeasureSpec.AT_MOST),
                            ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(context);
        mPanel = (LinearLayout) inflater.inflate(R.layout.menu_popup, null);
        setContentView(mPanel);

        setAnimationStyle(R.style.PopupAnimation);
    }

    /**
     * Adds the panel with the menu to its content.
     *
     * @param view The panel view with the menu to be shown.
     */
    public void setPanelView(View view) {
        view.setLayoutParams(new LinearLayout.LayoutParams(mPopupWidth,
                                                           LinearLayout.LayoutParams.WRAP_CONTENT));

        mPanel.removeAllViews();
        mPanel.addView(view);
    }

    /**
     * A small little offset.
     */
    @Override
    public void showAsDropDown(View anchor) {
        showAsDropDown(anchor, 0, -mYOffset);
    }
}
