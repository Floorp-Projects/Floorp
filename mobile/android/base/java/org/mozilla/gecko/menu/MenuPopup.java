/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.v7.widget.CardView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.PopupWindow;

/**
 * A popup to show the inflated MenuPanel.
 */
public class MenuPopup extends PopupWindow {
    private final CardView mPanel;

    private final int mPopupWidth;

    public MenuPopup(Context context) {
        super(context);

        setFocusable(true);

        mPopupWidth = context.getResources().getDimensionPixelSize(R.dimen.menu_popup_width);

        // Setting a null background makes the popup to not close on touching outside.
        setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        setWindowLayoutMode(ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(context);
        mPanel = (CardView) inflater.inflate(R.layout.menu_popup, null);
        setContentView(mPanel);

        // Disable corners on < lollipop:
        // CardView only supports clipping content on API >= 21 (for performance reasons). Without
        // content clipping the "action bar" will look ugly because it has its own background:
        // by default there's a 2px white edge along the top and sides (i.e. an inset corresponding
        // to the corner radius), if we disable the inset then the corners overlap.
        // It's possible to implement custom clipping, however given that the support library
        // chose not to support this for performance reasons, we too have chosen to just disable
        // corners on < 21, see Bug 1271428.
        if (AppConstants.Versions.preLollipop) {
            mPanel.setRadius(0);
        }

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
        showAsDropDown(anchor, xOffset, -anchor.getHeight());
    }
}
