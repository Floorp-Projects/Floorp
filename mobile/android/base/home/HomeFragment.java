/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.EditBookmarkDialog;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.ReaderModeUtils;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.HomeListView.HomeContextMenuInfo;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;

/**
 * HomeFragment is an empty fragment that can be added to the HomePager.
 * Subclasses can add their own views. 
 */
abstract class HomeFragment extends Fragment {
    // Log Tag.
    private static final String LOGTAG="GeckoHomeFragment";

    // Share MIME type.
    private static final String SHARE_MIME_TYPE = "text/plain";

    // Whether the fragment can load its content or not
    // This is used to defer data loading until the editing
    // mode animation ends.
    private boolean mCanLoadHint;

    // Whether the fragment has loaded its content
    private boolean mIsLoaded;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Bundle args = getArguments();
        if (args != null) {
            mCanLoadHint = args.getBoolean(HomePager.CAN_LOAD_ARG, false);
        } else {
            mCanLoadHint = false;
        }

        mIsLoaded = false;
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        if (menuInfo == null || !(menuInfo instanceof HomeContextMenuInfo)) {
            return;
        }

        HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;

        // Don't show the context menu for folders.
        if (info.isFolder) {
            return;
        }

        MenuInflater inflater = new MenuInflater(view.getContext());
        inflater.inflate(R.menu.home_contextmenu, menu);

        menu.setHeaderTitle(info.getDisplayTitle());

        // Hide the "Edit" menuitem if this item isn't a bookmark.
        if (info.bookmarkId < 0) {
            menu.findItem(R.id.home_edit_bookmark).setVisible(false);
        }

        // Hide the "Remove" menuitem if this item doesn't have a bookmark or history ID.
        if (info.bookmarkId < 0 && info.historyId < 0) {
            menu.findItem(R.id.home_remove).setVisible(false);
        }

        menu.findItem(R.id.home_share).setVisible(!GeckoProfile.get(getActivity()).inGuestMode());

        final boolean canOpenInReader = (info.display == Combined.DISPLAY_READER);
        menu.findItem(R.id.home_open_in_reader).setVisible(canOpenInReader);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        // onContextItemSelected() is first dispatched to the activity and
        // then dispatched to its fragments. Since fragments cannot "override"
        // menu item selection handling, it's better to avoid menu id collisions
        // between the activity and its fragments.

        ContextMenuInfo menuInfo = item.getMenuInfo();
        if (menuInfo == null || !(menuInfo instanceof HomeContextMenuInfo)) {
            return false;
        }

        HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;
        final Context context = getActivity().getApplicationContext();

        final int itemId = item.getItemId();
        if (itemId == R.id.home_share) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't share because URL is null");
            } else {
                GeckoAppShell.openUriExternal(info.url, SHARE_MIME_TYPE, "", "",
                                              Intent.ACTION_SEND, info.getDisplayTitle());
            }
        }

        if (itemId == R.id.home_add_to_launcher) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't add to home screen because URL is null");
                return false;
            }

            new AddToLauncherTask(info.url, info.getDisplayTitle()).execute();
            return true;
        }

        if (itemId == R.id.home_open_private_tab || itemId == R.id.home_open_new_tab) {
            if (info.url == null) {
                Log.e(LOGTAG, "Can't open in new tab because URL is null");
                return false;
            }

            int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_BACKGROUND;
            if (item.getItemId() == R.id.home_open_private_tab)
                flags |= Tabs.LOADURL_PRIVATE;

            final String url = (info.inReadingList ? ReaderModeUtils.getAboutReaderForUrl(info.url, true) : info.url);
            Tabs.getInstance().loadUrl(url, flags);
            Toast.makeText(context, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
            return true;
        }

        if (itemId == R.id.home_edit_bookmark) {
            // UI Dialog associates to the activity context, not the applications'.
            new EditBookmarkDialog(getActivity()).show(info.url);
            return true;
        }

        if (itemId == R.id.home_open_in_reader) {
            final String url = ReaderModeUtils.getAboutReaderForUrl(info.url, true);
            Tabs.getInstance().loadUrl(url, Tabs.LOADURL_NONE);
            return true;
        }

        if (itemId == R.id.home_remove) {
            // Prioritize removing a history entry over a bookmark in the case of a combined item.
            final int historyId = info.historyId;
            if (historyId > -1) {
                new RemoveHistoryTask(context, historyId).execute();
                return true;
            }

            final int bookmarkId = info.bookmarkId;
            if (bookmarkId > -1) {
                new RemoveBookmarkTask(context, bookmarkId, info.url, info.inReadingList).execute();
                return true;
            }
        }

        return false;
    }

    @Override
    public void setUserVisibleHint (boolean isVisibleToUser) {
        if (isVisibleToUser == getUserVisibleHint()) {
            return;
        }

        super.setUserVisibleHint(isVisibleToUser);
        loadIfVisible();
    }

    void setCanLoadHint(boolean canLoadHint) {
        if (mCanLoadHint == canLoadHint) {
            return;
        }

        mCanLoadHint = canLoadHint;
        loadIfVisible();
    }

    boolean getCanLoadHint() {
        return mCanLoadHint;
    }

    protected abstract void load();

    protected void loadIfVisible() {
        if (!mCanLoadHint || !isVisible() || !getUserVisibleHint()) {
            return;
        }

        if (!mIsLoaded) {
            load();
            mIsLoaded = true;
        }
    }

    private static class AddToLauncherTask extends UiAsyncTask<Void, Void, String> {
        private final String mUrl;
        private final String mTitle;

        public AddToLauncherTask(String url, String title) {
            super(ThreadUtils.getBackgroundHandler());

            mUrl = url;
            mTitle = title;
        }

        @Override
        public String doInBackground(Void... params) {
            return Favicons.getFaviconUrlForPageUrl(mUrl);
        }

        @Override
        public void onPostExecute(String faviconUrl) {
            OnFaviconLoadedListener listener = new OnFaviconLoadedListener() {
                @Override
                public void onFaviconLoaded(String url, Bitmap favicon) {
                    GeckoAppShell.createShortcut(mTitle, mUrl, favicon, "");
                }
            };

            Favicons.loadFavicon(mUrl, faviconUrl, 0, listener);
        }
    }

    private static class RemoveBookmarkTask extends UiAsyncTask<Void, Void, Void> {
        private final Context mContext;
        private final int mId;
        private final String mUrl;
        private final boolean mInReadingList;

        public RemoveBookmarkTask(Context context, int id, String url, boolean inReadingList) {
            super(ThreadUtils.getBackgroundHandler());

            mContext = context;
            mId = id;
            mUrl = url;
            mInReadingList = inReadingList;
        }

        @Override
        public Void doInBackground(Void... params) {
            ContentResolver cr = mContext.getContentResolver();
            BrowserDB.removeBookmark(cr, mId);
            if (mInReadingList) {
                GeckoEvent e = GeckoEvent.createBroadcastEvent("Reader:Remove", mUrl);
                GeckoAppShell.sendEventToGecko(e);

                int count = BrowserDB.getReadingListCount(cr);
                e = GeckoEvent.createBroadcastEvent("Reader:ListCountUpdated", Integer.toString(count));
                GeckoAppShell.sendEventToGecko(e);
            }
            return null;
        }

        @Override
        public void onPostExecute(Void result) {
            int messageId = mInReadingList ? R.string.reading_list_removed : R.string.bookmark_removed;
            Toast.makeText(mContext, messageId, Toast.LENGTH_SHORT).show();
        }
    }

    private static class RemoveHistoryTask extends UiAsyncTask<Void, Void, Void> {
        private final Context mContext;
        private final int mId;

        public RemoveHistoryTask(Context context, int id) {
            super(ThreadUtils.getBackgroundHandler());

            mContext = context;
            mId = id;
        }

        @Override
        public Void doInBackground(Void... params) {
            BrowserDB.removeHistoryEntry(mContext.getContentResolver(), mId);
            return null;
        }

        @Override
        public void onPostExecute(Void result) {
            Toast.makeText(mContext, R.string.history_removed, Toast.LENGTH_SHORT).show();
        }
    }
}
