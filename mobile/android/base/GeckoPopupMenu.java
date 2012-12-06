/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

/**
 * A PopupMenu that uses the custom GeckoMenu. This menu is 
 * usually tied to an anchor, and show as a dropdrown from the anchor.
 */
public class GeckoPopupMenu implements GeckoMenu.Callback, 
                                       GeckoMenu.MenuPresenter {

    // An interface for listeners for dismissal.
    public static interface OnDismissListener {
        public boolean onDismiss(GeckoMenu menu);
    }

    // An interface for listeners for menu item click events.
    public static interface OnMenuItemClickListener {
        public boolean onMenuItemClick(MenuItem item);
    }

    private View mAnchor;

    private MenuPopup mMenuPopup;
    private MenuPanel mMenuPanel;

    private GeckoMenu mMenu;
    private GeckoMenuInflater mMenuInflater;

    private OnDismissListener mDismissListener;
    private OnMenuItemClickListener mClickListener;

    public GeckoPopupMenu(Context context) {
        initialize(context, null);
    }

    public GeckoPopupMenu(Context context, View anchor) {
        initialize(context, anchor);
    }

    /**
     * This method creates an empty menu and attaches the necessary listeners.
     * If an anchor is supplied, it is stored as well.
     */
    private void initialize(Context context, View anchor) {
        mMenu = new GeckoMenu(context, null);
        mMenu.setCallback(this);
        mMenu.setMenuPresenter(this);
        mMenuInflater = new GeckoMenuInflater(context);

        mMenuPopup = new MenuPopup(context);
        mMenuPanel = new MenuPanel(context, null);

        setAnchor(anchor);
    }

    /**
     * Returns the menu that is current being shown.
     *
     * @return The menu being shown.
     */
    public Menu getMenu() {
        return mMenu;
    }

    /**
     * Returns the menu inflater that was used to create the menu.
     *
     * @return The menu inflater used.
     */
    public MenuInflater getMenuInflater() {
        return mMenuInflater;
    }

    /**
     * Inflates a menu resource to the menu using the menu inflater.
     *
     * @param menuRes The menu resource to be inflated.
     */
    public void inflate(int menuRes) {
        mMenuInflater.inflate(menuRes, mMenu);

        mMenuPanel.addView(mMenu);
        mMenuPopup.setPanelView(mMenuPanel);
    }

    /**
     * Set a different anchor after the menu is inflated.
     *
     * @param anchor The new anchor for the popup.
     */
    public void setAnchor(View anchor) {
        mAnchor = anchor;
    }

    public void setOnDismissListener(OnDismissListener listener) {
        mDismissListener = listener;
    }

    public void setOnMenuItemClickListener(OnMenuItemClickListener listener) {
        mClickListener = listener;
    }

    /**
     * Show the inflated menu.
     */
    public void show() {
        if (!mMenuPopup.isShowing())
            mMenuPopup.showAsDropDown(mAnchor);
    }

    /**
     * Hide the inflated menu.
     */
    public void dismiss() {
        if (mMenuPopup.isShowing()) {
            mMenuPopup.dismiss();

            if (mDismissListener != null)
                mDismissListener.onDismiss(mMenu);
        }
    }

    @Override
    public boolean onMenuItemSelected(MenuItem item) {
        if (mClickListener != null)
            return mClickListener.onMenuItemClick(item);

        return false;
    }

    @Override
    public void openMenu() {
        show();
    }

    @Override
    public void showMenu(View menu) {
        mMenuPanel.removeAllViews();
        mMenuPanel.addView(menu);

        openMenu();
    }

    @Override
    public void closeMenu() {
        dismiss();
    }
}
