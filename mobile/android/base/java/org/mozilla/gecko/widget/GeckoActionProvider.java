/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.app.Activity;
import android.net.Uri;
import android.support.design.widget.Snackbar;
import android.util.Base64;
import android.view.Menu;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SnackbarHelper;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.overlays.ui.ShareDialog;
import org.mozilla.gecko.menu.MenuItemSwitcherLayout;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.IntentUtils;
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
import android.webkit.MimeTypeMap;
import android.webkit.URLUtil;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
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
        return onCreateActionView(MAX_HISTORY_SIZE_DEFAULT, viewType);
    }

    public View onCreateActionView(final int maxHistorySize, final ActionViewType viewType) {
        // Create the view and set its data model.
        ActivityChooserModel dataModel = ActivityChooserModel.get(mContext, mHistoryFileName);
        final MenuItemSwitcherLayout view;
        switch (viewType) {
            case DEFAULT:
                view = new MenuItemSwitcherLayout(mContext, null);
                break;

            case CONTEXT_MENU:
                view = new MenuItemSwitcherLayout(mContext, null);
                view.initContextMenuStyles();
                break;

            default:
                throw new IllegalArgumentException(
                        "Unknown " + ActionViewType.class.getSimpleName() + ": " + viewType);
        }
        view.addActionButtonClickListener(mCallbacks);

        final PackageManager packageManager = mContext.getPackageManager();
        int historySize = dataModel.getDistinctActivityCountInHistory();
        if (historySize > maxHistorySize) {
            historySize = maxHistorySize;
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
        final String shareDialogClassName = ShareDialog.class.getCanonicalName();
        final String sendTabLabel = mContext.getResources().getString(R.string.overlay_share_send_other);
        final int count = dataModel.getActivityCount();
        for (int i = 0; i < count; i++) {
            ResolveInfo activity = dataModel.getActivity(i);
            final CharSequence activityLabel = activity.loadLabel(packageManager);

            // Pin internal actions to the top. Note:
            // the order here does not affect quick share.
            final int order;
            if (shareDialogClassName.equals(activity.activityInfo.name) &&
                    sendTabLabel.equals(activityLabel)) {
                order = Menu.FIRST + i;
            } else {
                order = Menu.FIRST + (i | Menu.CATEGORY_SECONDARY);
            }

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
                            downloadImageForIntent(launchIntent);
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
            Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST, "actionprovider");
            return true;
        }

        @Override
        public void onClick(View view) {
            Integer index = (Integer) view.getTag();
            chooseActivity(index);

            // Context: Sharing via chrome mainmenu and content contextmenu quickshare (no explicit session is active)
            Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.BUTTON, "actionprovider");
        }
    }

    public enum ActionViewType {
        DEFAULT,
        CONTEXT_MENU,
    }


    /**
     * Downloads the URI pointed to by a share intent, and alters the intent to point to the
     * locally stored file.
     *
     * @param intent share intent to alter in place.
     */
    public void downloadImageForIntent(final Intent intent) {
        final String src = IntentUtils.getStringExtraSafe(intent, Intent.EXTRA_TEXT);
        final File dir = GeckoApp.getTempDirectory();

        if (src == null || dir == null) {
            // We should be, but currently aren't, statically guaranteed an Activity context.
            // Try our best.
            if (mContext instanceof Activity) {
                SnackbarHelper.showSnackbar((Activity) mContext,
                        mContext.getApplicationContext().getString(R.string.share_image_failed),
                        Snackbar.LENGTH_LONG);
            }
            return;
        }

        GeckoApp.deleteTempFiles();

        String type = intent.getType();
        OutputStream os = null;
        try {
            // Create a temporary file for the image
            if (src.startsWith("data:")) {
                final int dataStart = src.indexOf(",");

                String extension = MimeTypeMap.getSingleton().getExtensionFromMimeType(type);

                // If we weren't given an explicit mimetype, try to dig one out of the data uri.
                if (TextUtils.isEmpty(extension) && dataStart > 5) {
                    type = src.substring(5, dataStart).replace(";base64", "");
                    extension = MimeTypeMap.getSingleton().getExtensionFromMimeType(type);
                }

                final File imageFile = File.createTempFile("image", "." + extension, dir);
                os = new FileOutputStream(imageFile);

                byte[] buf = Base64.decode(src.substring(dataStart + 1), Base64.DEFAULT);
                os.write(buf);

                // Only alter the intent when we're sure everything has worked
                intent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(imageFile));
            } else {
                InputStream is = null;
                try {
                    final byte[] buf = new byte[2048];
                    final URL url = new URL(src);
                    final String filename = URLUtil.guessFileName(src, null, type);
                    is = url.openStream();

                    final File imageFile = new File(dir, filename);
                    os = new FileOutputStream(imageFile);

                    int length;
                    while ((length = is.read(buf)) != -1) {
                        os.write(buf, 0, length);
                    }

                    // Only alter the intent when we're sure everything has worked
                    intent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(imageFile));
                } finally {
                    IOUtils.safeStreamClose(is);
                }
            }
        } catch (IOException ex) {
            // If something went wrong, we'll just leave the intent un-changed
        } finally {
            IOUtils.safeStreamClose(os);
        }
    }
}
