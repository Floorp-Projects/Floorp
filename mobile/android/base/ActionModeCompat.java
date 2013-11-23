/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.widget.GeckoPopupMenu;

import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

class ActionModeCompat implements GeckoPopupMenu.OnMenuItemClickListener,
                                  View.OnClickListener {
    private final String LOGTAG = "GeckoActionModeCompat";

    private Callback mCallback;
    private ActionModeCompatView mView;
    private Presenter mPresenter;

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

    /* View.OnClickListener*/
    @Override
    public void onClick(View v) {
        mPresenter.endActionModeCompat();
    }
}
