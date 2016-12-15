/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.menu;

import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
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
import org.mozilla.gecko.reader.SavedReaderViewHelper;
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

    // We might not know bookmarked/pinned states, so we allow for null values.
    private @Nullable Boolean isBookmarked;
    private @Nullable Boolean isPinned;

    final HomePager.OnUrlOpenListener onUrlOpenListener;
    final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;


    public abstract MenuItem getItemByID(int id);

    public abstract void show();

    public abstract void dismiss();

    final MenuMode mode;

    /* package-private */ ActivityStreamContextMenu(final Context context,
                                                    final MenuMode mode,
                                                    final String title, @NonNull final String url,
                                                    @Nullable final Boolean isBookmarked, @Nullable final Boolean isPinned,
                                                    HomePager.OnUrlOpenListener onUrlOpenListener,
                                                    HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        this.context = context;

        this.mode = mode;

        this.title = title;
        this.url = url;
        this.isBookmarked = isBookmarked;
        this.isPinned = isPinned;
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
        final MenuItem bookmarkItem = getItemByID(R.id.bookmark);
        if (Boolean.TRUE.equals(this.isBookmarked)) {
            bookmarkItem.setTitle(R.string.bookmark_remove);
        }

        final MenuItem pinItem = getItemByID(R.id.pin);
        if (Boolean.TRUE.equals(this.isPinned)) {
            pinItem.setTitle(R.string.contextmenu_top_sites_unpin);
        }

        // Disable "dismiss" for topsites until we have decided on its behaviour for topsites
        // (currently "dismiss" adds the URL to a highlights-specific blocklist, which the topsites
        // query has no knowledge of).
        if (mode == MenuMode.TOPSITE) {
            final MenuItem dismissItem = getItemByID(R.id.dismiss);
            dismissItem.setVisible(false);
        }

        if (isBookmarked == null) {
            // Disable the bookmark item until we know its bookmark state
            bookmarkItem.setEnabled(false);

            (new UIAsyncTask.WithoutParams<Void>(ThreadUtils.getBackgroundHandler()) {
                @Override
                protected Void doInBackground() {
                    isBookmarked = BrowserDB.from(context).isBookmark(context.getContentResolver(), url);
                    return null;
                }

                @Override
                protected void onPostExecute(Void aVoid) {
                    if (isBookmarked) {
                        bookmarkItem.setTitle(R.string.bookmark_remove);
                    }

                    bookmarkItem.setEnabled(true);
                }
            }).execute();
        }

        if (isPinned == null) {
            // Disable the pin item until we know its pinned state
            pinItem.setEnabled(false);

            (new UIAsyncTask.WithoutParams<Void>(ThreadUtils.getBackgroundHandler()) {
                @Override
                protected Void doInBackground() {
                    isPinned = BrowserDB.from(context).isPinnedForAS(context.getContentResolver(), url);
                    return null;
                }

                @Override
                protected void onPostExecute(Void aVoid) {
                    if (isPinned) {
                        pinItem.setTitle(R.string.contextmenu_top_sites_unpin);
                    }

                    pinItem.setEnabled(true);
                }
            }).execute();
        }

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
                Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST, "as_contextmenu");

                IntentHelper.openUriExternal(url, "text/plain", "", "", Intent.ACTION_SEND, title, false);
                break;

            case R.id.bookmark:
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        final BrowserDB db = BrowserDB.from(context);

                        final TelemetryContract.Event telemetryEvent;
                        final String telemetryExtra;
                        if (isBookmarked) {
                            db.removeBookmarksWithURL(context.getContentResolver(), url);

                            SavedReaderViewHelper rch = SavedReaderViewHelper.getSavedReaderViewHelper(context);
                            final boolean isReaderViewPage = rch.isURLCached(url);

                            telemetryEvent = TelemetryContract.Event.UNSAVE;

                            if (isReaderViewPage) {
                                telemetryExtra = "as_bookmark_reader";
                            } else {
                                telemetryExtra = "as_bookmark";
                            }
                        } else {
                            // We only store raw URLs in history (and bookmarks), hence we won't ever show about:reader
                            // URLs in AS topsites or highlights. Therefore we don't need to do any special about:reader handling here.
                            db.addBookmark(context.getContentResolver(), title, url);

                            telemetryEvent = TelemetryContract.Event.SAVE;
                            telemetryExtra = "as_bookmark";
                        }

                        Telemetry.sendUIEvent(telemetryEvent, TelemetryContract.Method.CONTEXT_MENU, telemetryExtra);
                    }
                });
                break;

            case R.id.pin:
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        final BrowserDB db = BrowserDB.from(context);

                        if (isPinned) {
                            db.unpinSiteForAS(context.getContentResolver(), url);
                        } else {
                            db.pinSiteForAS(context.getContentResolver(), url, title);
                        }
                    }
                });

            case R.id.copy_url:
                Clipboard.setText(url);
                break;

            case R.id.add_homescreen:
                GeckoAppShell.createShortcut(title, url);

                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.CONTEXT_MENU, "as_add_to_launcher");
                break;

            case R.id.open_new_tab:
                onUrlOpenInBackgroundListener.onUrlOpenInBackground(url, EnumSet.noneOf(HomePager.OnUrlOpenInBackgroundListener.Flags.class));

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.CONTEXT_MENU, "as_new_tab");
                break;

            case R.id.open_new_private_tab:
                onUrlOpenInBackgroundListener.onUrlOpenInBackground(url, EnumSet.of(HomePager.OnUrlOpenInBackgroundListener.Flags.PRIVATE));

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.CONTEXT_MENU, "as_private_tab");
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
                                                      @Nullable final Boolean isBookmarked, @Nullable final Boolean isPinned,
                                                      HomePager.OnUrlOpenListener onUrlOpenListener,
                                                      HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener,
                                                      final int tilesWidth, final int tilesHeight) {
        final ActivityStreamContextMenu menu;

        if (!HardwareUtils.isTablet()) {
            menu = new BottomSheetContextMenu(context,
                    menuMode,
                    title, url, isBookmarked, isPinned,
                    onUrlOpenListener, onUrlOpenInBackgroundListener,
                    tilesWidth, tilesHeight);
        } else {
            menu = new PopupContextMenu(context,
                    anchor,
                    menuMode,
                    title, url, isBookmarked, isPinned,
                    onUrlOpenListener, onUrlOpenInBackgroundListener);
        }

        menu.show();
        return menu;
    }
}
