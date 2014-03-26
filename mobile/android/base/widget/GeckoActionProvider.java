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
import android.text.TextUtils;

import java.util.ArrayList;
import java.util.HashMap;

public class GeckoActionProvider extends ActionProvider {
    private static int MAX_HISTORY_SIZE = 2;

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

    private static HashMap<String, GeckoActionProvider> mProviders = new HashMap<String, GeckoActionProvider>();

    private static String getFilenameFromMimeType(String mimeType) {
        String[] mime = mimeType.split("/");
        if (mime.length == 1) {
            return "history-" + mime[0] + ".xml";
        }

        // Separate out tel and mailto for their own media types
        if ("text".equals(mime[0])) {
            if ("tel".equals(mime[1])) {
                return "history-phone.xml";
            } else if ("mailto".equals(mime[1])) {
                return "history-email.xml";
            } else if ("html".equals(mime[1])) {
                return DEFAULT_HISTORY_FILE_NAME;
            }
        }

        return "history-" + mime[0] + ".xml";
    }

    // Gets the action provider for a particular mimetype
    public static GeckoActionProvider getForType(String mimeType, Context context) {
        if (!mProviders.keySet().contains(mimeType)) {
            GeckoActionProvider provider = new GeckoActionProvider(context);

            // For empty types, we just return a default provider
            if (TextUtils.isEmpty(mimeType)) {
                return provider;
            }

            provider.setHistoryFileName(getFilenameFromMimeType(mimeType));
            mProviders.put(mimeType, provider);
        }
        return mProviders.get(mimeType);
    }

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

        final PackageManager packageManager = mContext.getPackageManager();
        int historySize = dataModel.getDistinctActivityCountInHistory();
        if (historySize > MAX_HISTORY_SIZE) {
            historySize = MAX_HISTORY_SIZE;
        }

        // Historical data is dependent on past selection of activities.
        // Activity count is determined by the number of activities that can handle
        // the particular intent. When no intent is set, the activity count is 0,
        // while the history count can be a valid number.
        if (historySize > dataModel.getActivityCount()) {
            return view;
        }

        for (int i = 0; i < historySize; i++) {
            view.addActionButton(dataModel.getActivity(i).loadIcon(packageManager));
        }

        return view;
    }

    public View getView() {
        return onCreateActionView();
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

        // Inform the target listener to refresh it's UI, if needed.
        if (mOnTargetListener != null) {
            mOnTargetListener.onTargetSelected();
        }
    }

    public void setOnTargetSelectedListener(OnTargetSelectedListener listener) {
        mOnTargetListener = listener;
    }

    public ArrayList<ResolveInfo> getSortedActivites() {
        ArrayList<ResolveInfo> infos = new ArrayList<ResolveInfo>();

        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
        PackageManager packageManager = mContext.getPackageManager();

        // Populate the sub-menu with a sub set of the activities.
        final int count = dataModel.getActivityCount();
        for (int i = 0; i < count; i++) {
            infos.add(dataModel.getActivity(i));
        }
        return infos;
    }

    public void chooseActivity(int position) {
        mCallbacks.chooseActivity(position);
    }

    /**
     * Listener for handling default activity / menu item clicks.
     */
    private class Callbacks implements OnMenuItemClickListener,
                                       OnClickListener {
        private void chooseActivity(int index) { 
            ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
            Intent launchIntent = dataModel.chooseActivity(index);
            if (launchIntent != null) {
                launchIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
                mContext.startActivity(launchIntent);
            }

            if (mOnTargetListener != null) {
                mOnTargetListener.onTargetSelected();
            }
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            chooseActivity(item.getItemId());
            return true;
        }

        @Override
        public void onClick(View view) {
            Integer index = (Integer) view.getTag();
            chooseActivity(index);
        }
    }
}
