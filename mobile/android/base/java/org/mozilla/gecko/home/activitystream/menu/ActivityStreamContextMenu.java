/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.menu;

import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.design.widget.BottomSheetBehavior;
import android.support.design.widget.BottomSheetDialog;
import android.support.design.widget.NavigationView;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.IntentHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;
import org.mozilla.gecko.widget.FaviconView;

import java.util.EnumSet;

import static org.mozilla.gecko.activitystream.ActivityStream.extractLabel;

public class ActivityStreamContextMenu
    extends BottomSheetDialog
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

    boolean isAlreadyBookmarked = false;

    private ActivityStreamContextMenu(final Context context,
                                      final MenuMode mode,
                                      final String title, @NonNull final String url,
                                      HomePager.OnUrlOpenListener onUrlOpenListener,
                                      HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener,
                                      final int tilesWidth, final int tilesHeight) {
        super(context);

        this.context = context;

        this.title = title;
        this.url = url;
        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;

        final LayoutInflater inflater = LayoutInflater.from(context);

        final View content = inflater.inflate(R.layout.activity_stream_contextmenu_layout, null);
        setContentView(content);

        ((TextView) findViewById(R.id.title)).setText(title);
        final String label = extractLabel(url, false);
        ((TextView) findViewById(R.id.url)).setText(label);

        // Copy layouted parameters from the Highlights / TopSites items to ensure consistency
        final FaviconView faviconView = (FaviconView) findViewById(R.id.icon);
        ViewGroup.LayoutParams layoutParams = faviconView.getLayoutParams();
        layoutParams.width = tilesWidth;
        layoutParams.height = tilesHeight;
        faviconView.setLayoutParams(layoutParams);

        Icons.with(context)
                .pageUrl(url)
                .skipNetwork()
                .build()
                .execute(new IconCallback() {
                    @Override
                    public void onIconResponse(IconResponse response) {
                        faviconView.updateImage(response);
                    }
                });

        NavigationView navigationView = (NavigationView) findViewById(R.id.menu);
        navigationView.setNavigationItemSelectedListener(this);

        // Disable "dismiss" for topsites until we have decided on its behaviour for topsites
        // (currently "dismiss" adds the URL to a highlights-specific blocklist, which the topsites
        // query has no knowledge of).
        if (mode == MenuMode.TOPSITE) {
            final MenuItem dismissItem = navigationView.getMenu().findItem(R.id.dismiss);
            dismissItem.setVisible(false);
        }

        // Disable the bookmark item until we know its bookmark state
        final MenuItem bookmarkItem = navigationView.getMenu().findItem(R.id.bookmark);
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
        final MenuItem deleteHistoryItem = navigationView.getMenu().findItem(R.id.delete);
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

        BottomSheetBehavior<View> bsBehaviour = BottomSheetBehavior.from((View) content.getParent());
        bsBehaviour.setPeekHeight(context.getResources().getDimensionPixelSize(R.dimen.activity_stream_contextmenu_peek_height));
    }

    public static ActivityStreamContextMenu show(Context context,
                            final MenuMode menuMode,
                            final String title, @NonNull  final String url,
                            HomePager.OnUrlOpenListener onUrlOpenListener,
                            HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener,
                            final int tilesWidth, final int tilesHeight) {
        final ActivityStreamContextMenu menu = new ActivityStreamContextMenu(context,
                menuMode,
                title, url,
                onUrlOpenListener, onUrlOpenInBackgroundListener,
                tilesWidth, tilesHeight);
        menu.show();

        return menu;
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

            default:
                throw new IllegalArgumentException("Menu item with ID=" + item.getItemId() + " not handled");
        }

        dismiss();
        return true;
    }
}
