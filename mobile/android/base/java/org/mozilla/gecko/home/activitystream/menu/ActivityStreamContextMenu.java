/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.menu;

import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.design.widget.NavigationView;
import android.view.MenuItem;
import android.view.View;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.IntentHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import java.util.EnumSet;

@RobocopTarget
public abstract class ActivityStreamContextMenu
        implements NavigationView.OnNavigationItemSelectedListener {

    public enum MenuMode {
        HIGHLIGHT,
        TOPSITE
    }

    final Context context;

    final String title;
    final String url;

    final HomePager.OnUrlOpenListener onUrlOpenListener;
    final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    boolean isAlreadyBookmarked; // default false;

    public abstract MenuItem getItemByID(int id);

    public abstract void show();

    public abstract void dismiss();

    final MenuMode mode;

    /* package-private */ ActivityStreamContextMenu(final Context context,
                                                    final MenuMode mode,
                                                    final String title, @NonNull final String url,
                                                    HomePager.OnUrlOpenListener onUrlOpenListener,
                                                    HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        this.context = context;

        this.mode = mode;

        this.title = title;
        this.url = url;
        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;
    }

    /**
     * Must be called before the menu is shown.
     * <p/>
     * Your implementation must be ready to return items from getItemByID() before postInit() is
     * called, i.e. you should probably inflate your menu items before this call.
     */
    protected void postInit() {
        // Disable "dismiss" for topsites until we have decided on its behaviour for topsites
        // (currently "dismiss" adds the URL to a highlights-specific blocklist, which the topsites
        // query has no knowledge of).
        if (mode == MenuMode.TOPSITE) {
            final MenuItem dismissItem = getItemByID(R.id.dismiss);
            dismissItem.setVisible(false);
        }

        // Disable the bookmark item until we know its bookmark state
        final MenuItem bookmarkItem = getItemByID(R.id.bookmark);
        bookmarkItem.setEnabled(false);

        (new UIAsyncTask.WithoutParams<Void>(ThreadUtils.getBackgroundHandler()) {
            @Override
            protected Void doInBackground() {
                isAlreadyBookmarked = BrowserDB.from(context).isBookmark(context.getContentResolver(), url);
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                if (isAlreadyBookmarked) {
                    bookmarkItem.setTitle(R.string.bookmark_remove);
                }

                bookmarkItem.setEnabled(true);
            }
        }).execute();

        // Only show the "remove from history" item if a page actually has history
        final MenuItem deleteHistoryItem = getItemByID(R.id.delete);
        deleteHistoryItem.setVisible(false);

        (new UIAsyncTask.WithoutParams<Void>(ThreadUtils.getBackgroundHandler()) {
            boolean hasHistory;

            @Override
            protected Void doInBackground() {
                final Cursor cursor = BrowserDB.from(context).getHistoryForURL(context.getContentResolver(), url);
                try {
                    if (cursor != null &&
                            cursor.getCount() == 1) {
                        hasHistory = true;
                    } else {
                        hasHistory = false;
                    }
                } finally {
                    cursor.close();
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                if (hasHistory) {
                    deleteHistoryItem.setVisible(true);
                }
            }
        }).execute();
    }


    @Override
    public boolean onNavigationItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.share:
                Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST, "menu");
                IntentHelper.openUriExternal(url, "text/plain", "", "", Intent.ACTION_SEND, title, false);
                break;

            case R.id.bookmark:
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        final BrowserDB db = BrowserDB.from(context);

                        if (isAlreadyBookmarked) {
                            db.removeBookmarksWithURL(context.getContentResolver(), url);
                        } else {
                            db.addBookmark(context.getContentResolver(), title, url);
                        }

                    }
                });
                break;

            case R.id.copy_url:
                Clipboard.setText(url);
                break;

            case R.id.add_homescreen:
                GeckoAppShell.createShortcut(title, url);
                break;

            case R.id.open_new_tab:
                onUrlOpenInBackgroundListener.onUrlOpenInBackground(url, EnumSet.noneOf(HomePager.OnUrlOpenInBackgroundListener.Flags.class));
                break;

            case R.id.open_new_private_tab:
                onUrlOpenInBackgroundListener.onUrlOpenInBackground(url, EnumSet.of(HomePager.OnUrlOpenInBackgroundListener.Flags.PRIVATE));
                break;

            case R.id.dismiss:
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        BrowserDB.from(context)
                                .blockActivityStreamSite(context.getContentResolver(),
                                        url);
                    }
                });
                break;

            case R.id.delete:
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        BrowserDB.from(context)
                                .removeHistoryEntry(context.getContentResolver(),
                                        url);
                    }
                });
                break;

            default:
                throw new IllegalArgumentException("Menu item with ID=" + item.getItemId() + " not handled");
        }

        dismiss();
        return true;
    }


    @RobocopTarget
    public static ActivityStreamContextMenu show(Context context,
                                                      View anchor,
                                                      final MenuMode menuMode,
                                                      final String title, @NonNull final String url,
                                                      HomePager.OnUrlOpenListener onUrlOpenListener,
                                                      HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener,
                                                      final int tilesWidth, final int tilesHeight) {
        final ActivityStreamContextMenu menu;

        if (!HardwareUtils.isTablet()) {
            menu = new BottomSheetContextMenu(context,
                    menuMode,
                    title, url,
                    onUrlOpenListener, onUrlOpenInBackgroundListener,
                    tilesWidth, tilesHeight);
        } else {
            menu = new PopupContextMenu(context,
                    anchor,
                    menuMode,
                    title, url,
                    onUrlOpenListener, onUrlOpenInBackgroundListener);
        }

        menu.show();
        return menu;
    }
}
