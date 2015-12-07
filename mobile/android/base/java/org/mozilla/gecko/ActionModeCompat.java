/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.widget.GeckoPopupMenu;
import org.mozilla.gecko.menu.GeckoMenuItem;

import android.view.Gravity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;

class ActionModeCompat implements GeckoPopupMenu.OnMenuItemClickListener,
                                  GeckoPopupMenu.OnMenuItemLongClickListener,
                                  View.OnClickListener {
    private final String LOGTAG = "GeckoActionModeCompat";

    private final Callback mCallback;
    private final ActionModeCompatView mView;
    private final Presenter mPresenter;

    /* A set of callbacks to be called during this ActionMode's lifecycle. These will control the
     * creation, interaction with, and destruction of menuitems for the view */
    public static interface Callback {
        /* Called when action mode is first created. Implementors should use this to inflate menu resources. */
        public boolean onCreateActionMode(ActionModeCompat mode, Menu menu);

        /* Called to refresh an action mode's action menu. Called whenever the mode is invalidated. Implementors
         * should use this to enable/disable/show/hide menu items. */
        public boolean onPrepareActionMode(ActionModeCompat mode, Menu menu);

        /* Called to report a user click on an action button. */
        public boolean onActionItemClicked(ActionModeCompat mode, MenuItem item);

        /* Called when an action mode is about to be exited and destroyed. */
        public void onDestroyActionMode(ActionModeCompat mode);
    }

    /* Presenters handle the actual showing/hiding of the action mode UI in the app. Its their responsibility
     * to create an action mode, and assign it Callbacks and ActionModeCompatView's. */
    public static interface Presenter {
        /* Called when an action mode should be shown */
        public void startActionModeCompat(final Callback callback);

        /* Called when whatever action mode is showing should be hidden */
        public void endActionModeCompat();
    }

    public ActionModeCompat(Presenter presenter, Callback callback, ActionModeCompatView view) {
        mPresenter = presenter;
        mCallback = callback;

        mView = view;
        mView.initForMode(this);
    }

    public void finish() {
        // Clearing the menu will also clear the ActionItemBar
        mView.getMenu().clear();
        if (mCallback != null) {
            mCallback.onDestroyActionMode(this);
        }
    }

    public CharSequence getTitle() {
        return mView.getTitle();
    }

    public void setTitle(CharSequence title) {
        mView.setTitle(title);
    }

    public void setTitle(int resId) {
        mView.setTitle(resId);
    }

    public Menu getMenu() {
        return mView.getMenu();
    }

    public void invalidate() {
        if (mCallback != null) {
            mCallback.onPrepareActionMode(this, mView.getMenu());
        }
        mView.invalidate();
    }

    /* GeckoPopupMenu.OnMenuItemClickListener */
    @Override
    public boolean onMenuItemClick(MenuItem item) {
        if (mCallback != null) {
            return mCallback.onActionItemClicked(this, item);
        }
        return false;
    }

    /* GeckoPopupMenu.onMenuItemLongClickListener */
    @Override
    public boolean onMenuItemLongClick(MenuItem item) {
        showTooltip((GeckoMenuItem) item);
        return true;
    }

    /* View.OnClickListener*/
    @Override
    public void onClick(View v) {
        mPresenter.endActionModeCompat();
    }

    private void showTooltip(GeckoMenuItem item) {
        // Computes the tooltip toast screen position (shown when long-tapping the menu item) with regards to the
        // menu item's position (i.e below the item and slightly to the left)
        int[] location = new int[2];
        final View view = item.getActionView();
        view.getLocationOnScreen(location);

        int xOffset = location[0] - view.getWidth();
        int yOffset = location[1] + view.getHeight() / 2;

        Toast toast = Toast.makeText(view.getContext(), item.getTitle(), Toast.LENGTH_SHORT);
        toast.setGravity(Gravity.TOP|Gravity.LEFT, xOffset, yOffset);
        toast.show();
    }
}
