/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.menu.MenuItemActionView;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.view.ActionProvider;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnClickListener;

public class GeckoActionProvider extends ActionProvider {

    /**
     * A listener to know when a target was selected.
     * When setting a provider, the activity can listen to this,
     * to close the menu.
     */
    public interface OnTargetSelectedListener {
        public void onTargetSelected();
    }

    private final Context mContext;

    public static final String DEFAULT_HISTORY_FILE_NAME = "history.xml";

    //  History file.
    private String mHistoryFileName = DEFAULT_HISTORY_FILE_NAME;

    private OnTargetSelectedListener mOnTargetListener;

    private final Callbacks mCallbacks = new Callbacks();

    public GeckoActionProvider(Context context) {
        super(context);
        mContext = context;
    }

    @Override
    public View onCreateActionView() {
        // Create the view and set its data model.
        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
        MenuItemActionView view = new MenuItemActionView(mContext, null);
        view.setActionButtonClickListener(mCallbacks);

        PackageManager packageManager = mContext.getPackageManager();
        ResolveInfo defaultActivity = dataModel.getDefaultActivity();
        view.setActionButton(defaultActivity == null ? null : defaultActivity.loadIcon(packageManager));

        return view;
    }

    @Override
    public View onCreateActionView(MenuItem item) {
        MenuItemActionView view = (MenuItemActionView) onCreateActionView();
        view.setId(item.getItemId());
        view.setTitle(item.getTitle());
        view.setIcon(item.getIcon());
        view.setVisibility(item.isVisible() ? View.VISIBLE : View.GONE);
        view.setEnabled(item.isEnabled());
        return view;
    }

    public View getView(MenuItem item) {
        return onCreateActionView(item);
    }

    @Override
    public boolean hasSubMenu() {
        return true;
    }

    @Override
    public void onPrepareSubMenu(SubMenu subMenu) {
        // Clear since the order of items may change.
        subMenu.clear();

        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
        PackageManager packageManager = mContext.getPackageManager();

        // Populate the sub-menu with a sub set of the activities.
        final int count = dataModel.getActivityCount();
        for (int i = 0; i < count; i++) {
            ResolveInfo activity = dataModel.getActivity(i);
            subMenu.add(0, i, i, activity.loadLabel(packageManager))
                .setIcon(activity.loadIcon(packageManager))
                .setOnMenuItemClickListener(mCallbacks);
        }
    }

    public void setHistoryFileName(String historyFile) {
        mHistoryFileName = historyFile;
    }

    public Intent getIntent() {
        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
        return dataModel.getIntent();
    }

    public void setIntent(Intent intent) {
        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
        dataModel.setIntent(intent);
    }

    public void setOnTargetSelectedListener(OnTargetSelectedListener listener) {
        mOnTargetListener = listener;
    }

    /**
     * Listener for handling default activity / menu item clicks.
     */
    private class Callbacks implements OnMenuItemClickListener,
                                       OnClickListener {
        private void chooseActivity(int index) { 
            if (mOnTargetListener != null)
                mOnTargetListener.onTargetSelected();

            ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
            Intent launchIntent = dataModel.chooseActivity(index);
            if (launchIntent != null) {
                launchIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
                mContext.startActivity(launchIntent);
            }
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            chooseActivity(item.getItemId());
            return true;
        }

        @Override
        public void onClick(View view) {
            ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
            chooseActivity(dataModel.getActivityIndex(dataModel.getDefaultActivity()));
        }
    }
}

