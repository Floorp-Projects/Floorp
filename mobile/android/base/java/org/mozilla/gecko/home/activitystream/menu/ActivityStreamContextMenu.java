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
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
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

    private final Context context;

    private final String title;
    private final String url;

    private final ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder;

    // We might not know bookmarked/pinned states, so we allow for null values.
    // If we aren't told what these are in the constructor, we look them up in postInit.
    private @Nullable Boolean isBookmarked;
    private @Nullable Boolean isPinned;

    private final HomePager.OnUrlOpenListener onUrlOpenListener;
    private final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    public abstract MenuItem getItemByID(int id);

    public abstract void show();

    public abstract void dismiss();

    private final MenuMode mode;

    /* package-private */ ActivityStreamContextMenu(final Context context,
                                                    final ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder,
                                                    final MenuMode mode,
                                                    final String title, @NonNull final String url,
                                                    @Nullable final Boolean isBookmarked, @Nullable final Boolean isPinned,
                                                    HomePager.OnUrlOpenListener onUrlOpenListener,
                                                    HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        this.context = context;
        this.telemetryExtraBuilder = telemetryExtraBuilder;

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
    /* package-local */ void postInit() {
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

            (new UIAsyncTask.WithoutParams<Boolean>(ThreadUtils.getBackgroundHandler()) {
                @Override
                protected Boolean doInBackground() {
                    return BrowserDB.from(context).isBookmark(context.getContentResolver(), url);
                }

                @Override
                protected void onPostExecute(Boolean hasBookmark) {
                    if (hasBookmark) {
                        bookmarkItem.setTitle(R.string.bookmark_remove);
                    }

                    isBookmarked = hasBookmark;
                    bookmarkItem.setEnabled(true);
                }
            }).execute();
        }

        if (isPinned == null) {
            // Disable the pin item until we know its pinned state
            pinItem.setEnabled(false);

            (new UIAsyncTask.WithoutParams<Boolean>(ThreadUtils.getBackgroundHandler()) {
                @Override
                protected Boolean doInBackground() {
                    return BrowserDB.from(context).isPinnedForAS(context.getContentResolver(), url);
                }

                @Override
                protected void onPostExecute(Boolean hasPin) {
                    if (hasPin) {
                        pinItem.setTitle(R.string.contextmenu_top_sites_unpin);
                    }

                    isPinned = hasPin;
                    pinItem.setEnabled(true);
                }
            }).execute();
        }

        // Only show the "remove from history" item if a page actually has history
        final MenuItem deleteHistoryItem = getItemByID(R.id.delete);
        deleteHistoryItem.setVisible(false);

        (new UIAsyncTask.WithoutParams<Boolean>(ThreadUtils.getBackgroundHandler()) {
            @Override
            protected Boolean doInBackground() {
                final Cursor cursor = BrowserDB.from(context).getHistoryForURL(context.getContentResolver(), url);
                // It's tempting to throw here, but crashing because of a (hopefully) inconsequential
                // oddity is somewhat questionable.
                if (cursor == null) {
                    return false;
                }
                try {
                    return cursor.getCount() == 1;
                } finally {
                    cursor.close();
                }
            }

            @Override
            protected void onPostExecute(Boolean hasHistory) {
                deleteHistoryItem.setVisible(hasHistory);
            }
        }).execute();
    }


    @Override
    public boolean onNavigationItemSelected(MenuItem item) {
        final int menuItemId = item.getItemId();

        // Sets extra telemetry which doesn't require additional state information.
        // Pin and bookmark items are handled separately below, since they do require state
        // information to handle correctly.
        telemetryExtraBuilder.fromMenuItemId(menuItemId);

        switch (item.getItemId()) {
            case R.id.share:
                // NB: Generic menu item action event will be sent at the end of this function.
                // We have a seemingly duplicate telemetry event here because we want to emit
                // a concrete event in case it is used by other queries to estimate feature usage.
                Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST, "as_contextmenu");

                IntentHelper.openUriExternal(url, "text/plain", "", "", Intent.ACTION_SEND, title, false);
                break;

            case R.id.bookmark:
                final TelemetryContract.Event telemetryEvent;
                final String telemetryExtra;
                SavedReaderViewHelper rch = SavedReaderViewHelper.getSavedReaderViewHelper(context);
                final boolean isReaderViewPage = rch.isURLCached(url);

                // While isBookmarked is nullable, behaviour of postInit - disabling 'bookmark' item
                // until we know value of isBookmarked - guarantees that it will be set when we get here.
                if (isBookmarked) {
                    telemetryEvent = TelemetryContract.Event.UNSAVE;

                    if (isReaderViewPage) {
                        telemetryExtra = "as_bookmark_reader";
                    } else {
                        telemetryExtra = "as_bookmark";
                    }
                    telemetryExtraBuilder.set(ActivityStreamTelemetry.Contract.ITEM, ActivityStreamTelemetry.Contract.ITEM_REMOVE_BOOKMARK);
                } else {
                    telemetryEvent = TelemetryContract.Event.SAVE;
                    telemetryExtra = "as_bookmark";
                    telemetryExtraBuilder.set(ActivityStreamTelemetry.Contract.ITEM, ActivityStreamTelemetry.Contract.ITEM_ADD_BOOKMARK);
                }
                // NB: Generic menu item action event will be sent at the end of this function.
                // We have a seemingly duplicate telemetry event here because we want to emit
                // a concrete event in case it is used by other queries to estimate feature usage.
                Telemetry.sendUIEvent(telemetryEvent, TelemetryContract.Method.CONTEXT_MENU, telemetryExtra);

                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        final BrowserDB db = BrowserDB.from(context);

                        if (isBookmarked) {
                            db.removeBookmarksWithURL(context.getContentResolver(), url);

                        } else {
                            // We only store raw URLs in history (and bookmarks), hence we won't ever show about:reader
                            // URLs in AS topsites or highlights. Therefore we don't need to do any special about:reader handling here.
                            db.addBookmark(context.getContentResolver(), title, url);
                        }
                    }
                });
                break;

            case R.id.pin:
                // While isPinned is nullable, behaviour of postInit - disabling 'pin' item
                // until we know value of isPinned - guarantees that it will be set when we get here.
                if (isPinned) {
                    telemetryExtraBuilder.set(ActivityStreamTelemetry.Contract.ITEM, ActivityStreamTelemetry.Contract.ITEM_UNPIN);
                } else {
                    telemetryExtraBuilder.set(ActivityStreamTelemetry.Contract.ITEM, ActivityStreamTelemetry.Contract.ITEM_PIN);
                }

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

        Telemetry.sendUIEvent(
                TelemetryContract.Event.ACTION,
                TelemetryContract.Method.CONTEXT_MENU,
                telemetryExtraBuilder.build()
        );

        dismiss();
        return true;
    }


    @RobocopTarget
    public static ActivityStreamContextMenu show(Context context,
                                                      View anchor, ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder,
                                                      final MenuMode menuMode,
                                                      final String title, @NonNull final String url,
                                                      @Nullable final Boolean isBookmarked, @Nullable final Boolean isPinned,
                                                      HomePager.OnUrlOpenListener onUrlOpenListener,
                                                      HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener,
                                                      final int tilesWidth, final int tilesHeight) {
        final ActivityStreamContextMenu menu;

        if (!HardwareUtils.isTablet()) {
            menu = new BottomSheetContextMenu(context,
                    telemetryExtraBuilder, menuMode,
                    title, url, isBookmarked, isPinned,
                    onUrlOpenListener, onUrlOpenInBackgroundListener,
                    tilesWidth, tilesHeight);
        } else {
            menu = new PopupContextMenu(context,
                    anchor,
                    telemetryExtraBuilder, menuMode,
                    title, url, isBookmarked, isPinned,
                    onUrlOpenListener, onUrlOpenInBackgroundListener);
        }

        menu.show();
        return menu;
    }
}
