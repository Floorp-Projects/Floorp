/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.menu;

import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.design.widget.NavigationView;
import android.support.design.widget.Snackbar;
import android.view.MenuItem;
import android.view.View;

import org.mozilla.gecko.Clipboard;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.IntentHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.homepanel.model.WebpageModel;
import org.mozilla.gecko.activitystream.homepanel.topstories.PocketStoriesLoader;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.reader.SavedReaderViewHelper;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import java.util.EnumSet;

@RobocopTarget
public abstract class ActivityStreamContextMenu
        implements NavigationView.OnNavigationItemSelectedListener {

    public enum MenuMode {
        HIGHLIGHT,
        TOPSITE,
        TOPSTORY
    }

    private final Context context;
    private final WebpageModel item;
    @NonNull private final View snackbarAnchor;

    private final ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder;

    private final HomePager.OnUrlOpenListener onUrlOpenListener;
    private final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    public abstract MenuItem getItemByID(int id);

    public abstract void show();

    public abstract void dismiss();

    private final MenuMode mode;

    /* package-private */ ActivityStreamContextMenu(final View snackbarAnchor,
                                                    final ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder,
                                                    final MenuMode mode,
                                                    final WebpageModel item,
                                                    HomePager.OnUrlOpenListener onUrlOpenListener,
                                                    HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        this.context = snackbarAnchor.getContext();
        this.snackbarAnchor = snackbarAnchor;
        this.item = item;
        this.telemetryExtraBuilder = telemetryExtraBuilder;

        this.mode = mode;

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
        if (Boolean.TRUE.equals(item.isBookmarked())) {
            bookmarkItem.setTitle(R.string.bookmark_remove);
            bookmarkItem.setIcon(R.drawable.as_bookmark_filled);
        }

        final MenuItem pinItem = getItemByID(R.id.pin);
        if (Boolean.TRUE.equals(item.isPinned())) {
            pinItem.setTitle(R.string.contextmenu_top_sites_unpin);
        }

        // Disable "dismiss" for topsites and topstories here. Later when we know whether this item
        // has history we might re-enable this item (See AsyncTask below).
        final MenuItem dismissItem = getItemByID(R.id.dismiss);
        if (mode == MenuMode.TOPSTORY || mode == MenuMode.TOPSITE) {
            dismissItem.setVisible(false);
        }

        if (item.isBookmarked() == null) {
            // Disable the bookmark item until we know its bookmark state
            bookmarkItem.setEnabled(false);

            (new UIAsyncTask.WithoutParams<Boolean>(ThreadUtils.getBackgroundHandler()) {
                @Override
                protected Boolean doInBackground() {
                    return BrowserDB.from(context).isBookmark(context.getContentResolver(), item.getUrl());
                }

                @Override
                protected void onPostExecute(Boolean hasBookmark) {
                    if (hasBookmark) {
                        bookmarkItem.setTitle(R.string.bookmark_remove);
                        bookmarkItem.setIcon(R.drawable.as_bookmark_filled);
                    }

                    item.updateBookmarked(hasBookmark);
                    bookmarkItem.setEnabled(true);
                }
            }).execute();
        }

        if (item.isPinned() == null) {
            // Disable the pin item until we know its pinned state
            pinItem.setEnabled(false);

            (new UIAsyncTask.WithoutParams<Boolean>(ThreadUtils.getBackgroundHandler()) {
                @Override
                protected Boolean doInBackground() {
                    return BrowserDB.from(context).isPinnedForAS(context.getContentResolver(), item.getUrl());
                }

                @Override
                protected void onPostExecute(Boolean hasPin) {
                    if (hasPin) {
                        pinItem.setTitle(R.string.contextmenu_top_sites_unpin);
                    }

                    item.updatePinned(hasPin);
                    pinItem.setEnabled(true);
                }
            }).execute();
        }

        final MenuItem deleteHistoryItem = getItemByID(R.id.delete);
        // Only show the "remove from history" item if a page actually has history
        deleteHistoryItem.setVisible(false);

        (new UIAsyncTask.WithoutParams<Boolean>(ThreadUtils.getBackgroundHandler()) {
            @Override
            protected Boolean doInBackground() {
                final WebpageModel item = ActivityStreamContextMenu.this.item;

                final Cursor cursor = BrowserDB.from(context).getHistoryForURL(context.getContentResolver(), item.getUrl());
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

                if (!hasHistory && mode == MenuMode.TOPSITE) {
                    // For top sites items without history (suggested items) we show the dismiss
                    // item. This will allow users to remove a suggested site.
                    dismissItem.setVisible(true);
                }
            }
        }).execute();
    }


    @Override
    public boolean onNavigationItemSelected(MenuItem menuItem) {
        final int menuItemId = menuItem.getItemId();

        // Sets extra telemetry which doesn't require additional state information.
        // Pin and bookmark items are handled separately below, since they do require state
        // information to handle correctly.
        telemetryExtraBuilder.fromMenuItemId(menuItemId);

        final String referrerUri = mode == MenuMode.TOPSTORY ? PocketStoriesLoader.POCKET_REFERRER_URI : null;

        switch (menuItem.getItemId()) {
            case R.id.share:
                // NB: Generic menu item action event will be sent at the end of this function.
                // We have a seemingly duplicate telemetry event here because we want to emit
                // a concrete event in case it is used by other queries to estimate feature usage.
                Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST, "as_contextmenu");

                IntentHelper.openUriExternal(item.getUrl(), "text/plain", "", "", Intent.ACTION_SEND, item.getTitle(), false);
                break;

            case R.id.bookmark:
                final TelemetryContract.Event telemetryEvent;
                final String telemetryExtra;
                SavedReaderViewHelper rch = SavedReaderViewHelper.getSavedReaderViewHelper(context);
                final boolean isReaderViewPage = rch.isURLCached(item.getUrl());

                // While isBookmarked is nullable, behaviour of postInit - disabling 'bookmark' item
                // until we know value of isBookmarked - guarantees that it will be set when we get here.
                if (item.isBookmarked()) {
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

                        if (item.isBookmarked()) {
                            db.removeBookmarksWithURL(context.getContentResolver(), item.getUrl());

                            // See bug 1402521 for adding "options" to the snackbar.
                            Snackbar.make(ActivityStreamContextMenu.this.snackbarAnchor, R.string.bookmark_removed, Snackbar.LENGTH_LONG).show();
                        } else {
                            // We only store raw URLs in history (and bookmarks), hence we won't ever show about:reader
                            // URLs in AS topsites or highlights. Therefore we don't need to do any special about:reader handling here.
                            db.addBookmark(context.getContentResolver(), item.getTitle(), item.getUrl());

                            Snackbar.make(ActivityStreamContextMenu.this.snackbarAnchor, R.string.bookmark_added, Snackbar.LENGTH_LONG).show();
                        }
                        item.onStateCommitted();
                    }
                });
                break;

            case R.id.pin:
                // While isPinned is nullable, behaviour of postInit - disabling 'pin' item
                // until we know value of isPinned - guarantees that it will be set when we get here.
                if (item.isPinned()) {
                    telemetryExtraBuilder.set(ActivityStreamTelemetry.Contract.ITEM, ActivityStreamTelemetry.Contract.ITEM_UNPIN);
                } else {
                    telemetryExtraBuilder.set(ActivityStreamTelemetry.Contract.ITEM, ActivityStreamTelemetry.Contract.ITEM_PIN);
                }

                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        final BrowserDB db = BrowserDB.from(context);

                        if (item.isPinned()) {
                            db.unpinSiteForAS(context.getContentResolver(), item.getUrl());
                        } else {
                            db.pinSiteForAS(context.getContentResolver(), item.getUrl(), item.getTitle());
                        }
                        item.onStateCommitted();
                    }
                });
                break;

            case R.id.copy_url:
                Clipboard.setText(context, item.getUrl());
                break;

            case R.id.add_homescreen:
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        GeckoApplication.createBrowserShortcut(item.getTitle(), item.getUrl());
                    }
                });
                break;

            case R.id.open_new_tab:
                onUrlOpenInBackgroundListener.onUrlOpenInBackgroundWithReferrer(item.getUrl(), referrerUri,
                        EnumSet.noneOf(HomePager.OnUrlOpenInBackgroundListener.Flags.class));
                break;

            case R.id.open_new_private_tab:
                onUrlOpenInBackgroundListener.onUrlOpenInBackgroundWithReferrer(item.getUrl(), referrerUri,
                        EnumSet.of(HomePager.OnUrlOpenInBackgroundListener.Flags.PRIVATE));
                break;

            case R.id.dismiss:
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        BrowserDB db = BrowserDB.from(context);
                        if (db.hideSuggestedSite(item.getUrl())) {
                            context.getContentResolver().notifyChange(BrowserContract.SuggestedSites.CONTENT_URI, null);
                        } else {
                            db.blockActivityStreamSite(context.getContentResolver(), item.getUrl());
                        }
                    }
                });
                break;

            case R.id.delete:
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        BrowserDB.from(context)
                                .removeHistoryEntry(context.getContentResolver(), item.getUrl());
                    }
                });
                break;

            default:
                throw new IllegalArgumentException("Menu item with ID=" + menuItem.getItemId() + " not handled");
        }

        Telemetry.sendUIEvent(
                TelemetryContract.Event.ACTION,
                TelemetryContract.Method.CONTEXT_MENU,
                telemetryExtraBuilder.build()
        );

        dismiss();
        return true;
    }

    /**
     * @param tabletContextMenuAnchor A view to anchor the context menu on tablet, where it doesn't fill the screen.
     * @param snackbarAnchor A view to anchor the Snackbar on. Don't use items in the recyclerView because these views can be
     *               removed from the view hierarchy when the recyclerView scrolls.
     * @param shouldOverrideIconWithImageProvider true if the favicon should be replaced with an image provider,
     *                                            if applicable, false otherwise.
     */
    @RobocopTarget
    public static ActivityStreamContextMenu show(final View tabletContextMenuAnchor, final View snackbarAnchor,
                                                      ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder,
                                                      final MenuMode menuMode, final WebpageModel item,
                                                      final boolean shouldOverrideIconWithImageProvider,
                                                      HomePager.OnUrlOpenListener onUrlOpenListener,
                                                      HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener,
                                                      final int tilesWidth, final int tilesHeight) {
        final ActivityStreamContextMenu menu;

        if (!HardwareUtils.isTablet()) {
            menu = new BottomSheetContextMenu(snackbarAnchor,
                    telemetryExtraBuilder, menuMode,
                    item, shouldOverrideIconWithImageProvider, onUrlOpenListener, onUrlOpenInBackgroundListener,
                    tilesWidth, tilesHeight);
        } else {
            menu = new PopupContextMenu(tabletContextMenuAnchor, snackbarAnchor,
                    telemetryExtraBuilder, menuMode,
                    item, onUrlOpenListener, onUrlOpenInBackgroundListener);
        }

        menu.show();
        return menu;
    }
}
