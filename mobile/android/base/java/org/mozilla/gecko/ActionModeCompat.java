/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.view.ActionMode;
import android.view.Gravity;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;

import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuItem;
import org.mozilla.gecko.widget.ActionModePresenter;
import org.mozilla.gecko.widget.GeckoPopupMenu;

class ActionModeCompat extends ActionMode implements GeckoPopupMenu.OnMenuItemClickListener,
        GeckoPopupMenu.OnMenuItemLongClickListener,
        View.OnClickListener {
    private final String LOGTAG = "GeckoActionModeCompat";

    private final Callback mCallback;
    private final ActionModeCompatView mView;
    private final ActionModePresenter mPresenter;

    public ActionModeCompat(@NonNull ActionModePresenter presenter,
                            @Nullable Callback callback,
                            @NonNull ActionModeCompatView view) {

        mPresenter = presenter;
        mCallback = callback;

        mView = view;
        mView.initForMode(this);
    }

    public void finish() {
        // Clearing the menu will also clear the ActionItemBar
        final GeckoMenu menu = mView.getMenu();
        menu.clear();
        menu.close();

        if (mCallback != null) {
            mCallback.onDestroyActionMode(this);
        }
    }

    public CharSequence getTitle() {
        return mView.getTitle();
    }

    @Override
    public CharSequence getSubtitle() {
        throw new UnsupportedOperationException("This method is not supported by this class");
    }

    @Override
    public View getCustomView() {
        return mView;
    }

    @Override
    public MenuInflater getMenuInflater() {
        throw new UnsupportedOperationException("This method is not supported by this class");
    }

    public void setTitle(CharSequence title) {
        mView.setTitle(title);
    }

    public void setTitle(int resId) {
        mView.setTitle(resId);
    }

    @Override
    public void setSubtitle(CharSequence subtitle) {
        throw new UnsupportedOperationException("This method is not supported by this class");
    }

    @Override
    public void setSubtitle(int resId) {
        throw new UnsupportedOperationException("This method is not supported by this class");
    }

    @Override
    public void setCustomView(View view) {
        // ActionModeCompatView is custom view for this class
        throw new UnsupportedOperationException("This method is not supported by this class");
    }

    public GeckoMenu getMenu() {
        return mView.getMenu();
    }

    public void invalidate() {
        if (mCallback != null) {
            mCallback.onPrepareActionMode(this, mView.getMenu());
        }
        mView.invalidate();
    }

    public void animateIn() {
        mView.animateIn();
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
        mPresenter.endActionMode();
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
        toast.setGravity(Gravity.TOP | Gravity.LEFT, xOffset, yOffset);
        toast.show();
    }
}
