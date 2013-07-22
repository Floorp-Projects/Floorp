/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.EditBookmarkDialog;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.HomeListView.HomeContextMenuInfo;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.support.v4.app.Fragment;
import android.text.TextUtils;
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
class HomeFragment extends Fragment {
    // Log Tag.
    private static final String LOGTAG="GeckoHomeFragment";

    // Share MIME type.
    private static final String SHARE_MIME_TYPE = "text/plain";

    // URL to Title replacement regex.
    private static final String REGEX_URL_TO_TITLE = "^([a-z]+://)?(www\\.)?";

    protected void showSubPage(Fragment subPage) {
        getActivity().getSupportFragmentManager().beginTransaction()
                .addToBackStack(null).replace(R.id.home_pager_container, subPage, HomePager.SUBPAGE_TAG)
                .commitAllowingStateLoss();
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

        // Show Open Private Tab if we're in private mode, Open New Tab otherwise
        boolean isPrivate = false;
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            isPrivate = tab.isPrivate();
        }
        menu.findItem(R.id.open_new_tab).setVisible(!isPrivate);
        menu.findItem(R.id.open_private_tab).setVisible(isPrivate);

        // Hide "Remove" item if there isn't a valid history ID
        if (info.rowId < 0) {
            menu.findItem(R.id.remove_history).setVisible(false);
        }
        menu.setHeaderTitle(info.title);

        menu.findItem(R.id.remove_history).setVisible(false);
        menu.findItem(R.id.open_in_reader).setVisible(false);
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
        final Activity activity = getActivity();

        switch(item.getItemId()) {
            case R.id.home_share: {
                if (info.url == null) {
                    Log.e(LOGTAG, "Can't share because URL is null");
                    break;
                }

                GeckoAppShell.openUriExternal(info.url, SHARE_MIME_TYPE, "", "",
                                              Intent.ACTION_SEND, info.title);
                return true;
            }

            case R.id.home_add_to_launcher: {
                if (info.url == null) {
                    Log.e(LOGTAG, "Can't add to home screen because URL is null");
                    break;
                }

                Bitmap bitmap = null;
                if (info.favicon != null && info.favicon.length > 0) {
                    bitmap = BitmapUtils.decodeByteArray(info.favicon);
                }

                String shortcutTitle = TextUtils.isEmpty(info.title) ? info.url.replaceAll(REGEX_URL_TO_TITLE, "") : info.title;
                GeckoAppShell.createShortcut(shortcutTitle, info.url, bitmap, "");
                return true;
            }

            case R.id.open_private_tab:
            case R.id.open_new_tab: {
                if (info.url == null) {
                    Log.e(LOGTAG, "Can't open in new tab because URL is null");
                    break;
                }

                int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_BACKGROUND;
                if (item.getItemId() == R.id.open_private_tab)
                    flags |= Tabs.LOADURL_PRIVATE;

                Tabs.getInstance().loadUrl(info.url, flags);
                Toast.makeText(activity, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
                return true;
            }

            case R.id.edit_bookmark: {
                new EditBookmarkDialog(activity).show(info.url);
                return true;
            }

            case R.id.remove_bookmark: {
                final int rowId = info.rowId;
                final String url = info.url;
                final boolean inReadingList = info.inReadingList;

                (new UiAsyncTask<Void, Void, Integer>(ThreadUtils.getBackgroundHandler()) {
                    @Override
                    public Integer doInBackground(Void... params) {
                        BrowserDB.removeBookmark(activity.getContentResolver(), rowId);
                        return BrowserDB.getReadingListCount(activity.getContentResolver());
                    }

                    @Override
                    public void onPostExecute(Integer aCount) {
                        int messageId = R.string.bookmark_removed;
                        if (inReadingList) {
                            messageId = R.string.reading_list_removed;

                            GeckoEvent e = GeckoEvent.createBroadcastEvent("Reader:Remove", url);
                            GeckoAppShell.sendEventToGecko(e);

                            e = GeckoEvent.createBroadcastEvent("Reader:ListCountUpdated", Integer.toString(aCount));
                            GeckoAppShell.sendEventToGecko(e);
                        }

                        Toast.makeText(activity, messageId, Toast.LENGTH_SHORT).show();
                    }
                }).execute();
                return true;
            }
        }
        return false;
    }
}
