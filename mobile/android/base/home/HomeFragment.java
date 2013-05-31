/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.HomeListView.HomeContextMenuInfo;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Bitmap;
import android.support.v4.app.Fragment;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

/**
 * HomeFragment is an empty fragment that can be added to the HomePager.
 * Subclasses can add their own views. 
 */
public class HomeFragment extends Fragment {
    // Log Tag.
    private static final String LOGTAG="GeckoHomeFragment";

    // Share MIME type.
    private static final String SHARE_MIME_TYPE = "text/plain";

    // URL to Title replacement regex.
    private static final String REGEX_URL_TO_TITLE = "^([a-z]+://)?(www\\.)?";

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        final Activity activity = getActivity();
        HomeContextMenuInfo info = null;

        try {
            ContextMenuInfo menuInfo = item.getMenuInfo();
            info = (HomeContextMenuInfo) menuInfo;
        } catch(ClassCastException e) {
            throw new IllegalArgumentException("ContextMenuInfo is not of type HomeContextMenuInfo");
        }

        switch(item.getItemId()) {
            case R.id.share: {
                if (info.url == null) {
                    Log.e(LOGTAG, "Can't share because URL is null");
                    break;
                }

                GeckoAppShell.openUriExternal(info.url, SHARE_MIME_TYPE, "", "",
                                              Intent.ACTION_SEND, info.title);
                return true;
            }

            case R.id.add_to_launcher: {
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

                int flags = Tabs.LOADURL_NEW_TAB;
                if (item.getItemId() == R.id.open_private_tab)
                    flags |= Tabs.LOADURL_PRIVATE;

                Tabs.getInstance().loadUrl(info.url, flags);
                Toast.makeText(activity, R.string.new_tab_opened, Toast.LENGTH_SHORT).show();
                return true;
            }

            case R.id.edit_bookmark: {
                AlertDialog.Builder editPrompt = new AlertDialog.Builder(activity);
                final View editView = LayoutInflater.from(activity).inflate(R.layout.bookmark_edit, null);
                editPrompt.setTitle(R.string.bookmark_edit_title);
                editPrompt.setView(editView);

                final EditText nameText = ((EditText) editView.findViewById(R.id.edit_bookmark_name));
                final EditText locationText = ((EditText) editView.findViewById(R.id.edit_bookmark_location));
                final EditText keywordText = ((EditText) editView.findViewById(R.id.edit_bookmark_keyword));
                nameText.setText(info.title);
                locationText.setText(info.url);
                keywordText.setText(info.keyword);

                final int rowId = info.rowId;
                editPrompt.setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int whichButton) {
                        (new UiAsyncTask<Void, Void, Void>(ThreadUtils.getBackgroundHandler()) {
                            @Override
                            public Void doInBackground(Void... params) {
                                String newUrl = locationText.getText().toString().trim();
                                String newKeyword = keywordText.getText().toString().trim();
                                BrowserDB.updateBookmark(activity.getContentResolver(), rowId, newUrl, nameText.getText().toString(), newKeyword);
                                return null;
                            }

                            @Override
                            public void onPostExecute(Void result) {
                                Toast.makeText(activity, R.string.bookmark_updated, Toast.LENGTH_SHORT).show();
                            }
                        }).execute();
                    }
                });

                editPrompt.setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int whichButton) {
                          // do nothing
                      }
                });

                final AlertDialog dialog = editPrompt.create();
                dialog.show();
                return true;
            }

            case R.id.remove_bookmark: {
                final int rowId = info.rowId;
                (new UiAsyncTask<Void, Void, Void>(ThreadUtils.getBackgroundHandler()) {
                    @Override
                    public Void doInBackground(Void... params) {
                        BrowserDB.removeBookmark(activity.getContentResolver(), rowId);
                        return null;
                    }

                    @Override
                    public void onPostExecute(Void result) {
                        Toast.makeText(activity, R.string.bookmark_removed, Toast.LENGTH_SHORT).show();
                    }
                }).execute();
                return true;
            }
        }
        return false;
    }
}
