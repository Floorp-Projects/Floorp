/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.view.Menu;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.menu.MenuItemActionView;
import org.mozilla.gecko.overlays.ui.ShareDialog;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnClickListener;
import android.text.TextUtils;

import java.util.ArrayList;
import java.util.HashMap;

public class GeckoActionProvider {
    private static final int MAX_HISTORY_SIZE_DEFAULT = 2;

    /**
     * A listener to know when a target was selected.
     * When setting a provider, the activity can listen to this,
     * to close the menu.
     */
    public interface OnTargetSelectedListener {
        public void onTargetSelected();
    }

    final Context mContext;

    public final static String DEFAULT_MIME_TYPE = "text/plain";

    public static final String DEFAULT_HISTORY_FILE_NAME = "history.xml";

    //  History file.
    String mHistoryFileName = DEFAULT_HISTORY_FILE_NAME;

    OnTargetSelectedListener mOnTargetListener;

    private final Callbacks mCallbacks = new Callbacks();

    private static final HashMap<String, GeckoActionProvider> mProviders = new HashMap<String, GeckoActionProvider>();

    private static String getFilenameFromMimeType(String mimeType) {
        String[] mime = mimeType.split("/");

        // All text mimetypes use the default provider
        if ("text".equals(mime[0])) {
            return DEFAULT_HISTORY_FILE_NAME;
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
        mContext = context;
    }

    /**
     * Creates the action view using the default history size.
     */
    public View onCreateActionView(final ActionViewType viewType) {
        // Create the view and set its data model.
        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
        final MenuItemActionView view;
        switch (viewType) {
            case DEFAULT:
                view = new MenuItemActionView(mContext, null);
                break;

            case CONTEXT_MENU:
                view = new MenuItemActionView(mContext, null);
                view.initContextMenuStyles();
                break;

            default:
                throw new IllegalArgumentException(
                        "Unknown " + ActionViewType.class.getSimpleName() + ": " + viewType);
        }
        view.addActionButtonClickListener(mCallbacks);

        final PackageManager packageManager = mContext.getPackageManager();
        int historySize = dataModel.getDistinctActivityCountInHistory();
        if (historySize > MAX_HISTORY_SIZE_DEFAULT) {
            historySize = MAX_HISTORY_SIZE_DEFAULT;
        }

        // Historical data is dependent on past selection of activities.
        // Activity count is determined by the number of activities that can handle
        // the particular intent. When no intent is set, the activity count is 0,
        // while the history count can be a valid number.
        if (historySize > dataModel.getActivityCount()) {
            return view;
        }

        for (int i = 0; i < historySize; i++) {
            view.addActionButton(dataModel.getActivity(i).loadIcon(packageManager), 
                                 dataModel.getActivity(i).loadLabel(packageManager));
        }

        return view;
    }

    public boolean hasSubMenu() {
        return true;
    }

    public void onPrepareSubMenu(SubMenu subMenu) {
        // Clear since the order of items may change.
        subMenu.clear();

        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
        PackageManager packageManager = mContext.getPackageManager();

        // Populate the sub-menu with a sub set of the activities.
        final int count = dataModel.getActivityCount();
        for (int i = 0; i < count; i++) {
            ResolveInfo activity = dataModel.getActivity(i);
            final CharSequence activityLabel = activity.loadLabel(packageManager);

            // Pin internal actions to the top. Note:
            // the order here does not affect quick share.
            final int order = Menu.FIRST + (i | Menu.CATEGORY_SECONDARY);

            subMenu.add(0, i, order, activityLabel)
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

    public ArrayList<ResolveInfo> getSortedActivities() {
        ArrayList<ResolveInfo> infos = new ArrayList<ResolveInfo>();

        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);

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
        void chooseActivity(int index) {
            final ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
            final Intent launchIntent = dataModel.chooseActivity(index);
            if (launchIntent != null) {
                // This may cause a download to happen. Make sure we're on the background thread.
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        // Share image downloads the image before sharing it.
                        String type = launchIntent.getType();
                        if (Intent.ACTION_SEND.equals(launchIntent.getAction()) && type != null && type.startsWith("image/")) {
                            GeckoAppShell.downloadImageForIntent(launchIntent);
                        }

                        launchIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
                        mContext.startActivity(launchIntent);
                    }
                });
            }

            if (mOnTargetListener != null) {
                mOnTargetListener.onTargetSelected();
            }
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            chooseActivity(item.getItemId());

            // Context: Sharing via chrome mainmenu list (no explicit session is active)
            Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST);
            return true;
        }

        @Override
        public void onClick(View view) {
            Integer index = (Integer) view.getTag();
            chooseActivity(index);

            // Context: Sharing via chrome mainmenu and content contextmenu quickshare (no explicit session is active)
            Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.BUTTON);
        }
    }

    public enum ActionViewType {
        DEFAULT,
        CONTEXT_MENU,
    }
}
