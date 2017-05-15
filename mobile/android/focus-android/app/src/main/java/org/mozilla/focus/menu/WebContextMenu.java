/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.menu;

import android.app.Dialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.design.widget.NavigationView;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.web.Download;
import org.mozilla.focus.web.IWebView;

public class WebContextMenu {

    private static View createTitleView(final @NonNull Context context, final @NonNull String title) {
        final View titleView = LayoutInflater.from(context).inflate(R.layout.dialog_title, null);
        ((TextView) titleView.findViewById(R.id.title)).setText(title);

        return titleView;
    }

    public static void show(final @NonNull Context context, final @NonNull IWebView.Callback callback, final @NonNull IWebView.HitTarget hitTarget) {
        if (!(hitTarget.isLink || hitTarget.isImage)) {
            // We don't support any other classes yet:
            throw new IllegalStateException("WebContextMenu can only handle long-press on images and/or links.");
        }

        TelemetryWrapper.openWebContextMenuEvent();

        AlertDialog.Builder builder = new AlertDialog.Builder(context);

        final View titleView;
        if (hitTarget.isLink) {
           titleView = createTitleView(context, hitTarget.linkURL);
        } else if (hitTarget.isImage) {
            titleView = createTitleView(context, hitTarget.imageURL);
        } else {
            throw new IllegalStateException("Unhandled long press target type");
        }
        builder.setCustomTitle(titleView);

        final NavigationView menu = new NavigationView(context);
        builder.setView(menu);

        builder.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                // This even is only sent when the back button is pressed, or when a user
                // taps outside of the dialog:
                TelemetryWrapper.cancelWebContextMenuEvent();
            }
        });

        final Dialog dialog = builder.create();

        setupMenuForHitTarget(dialog, menu, callback, hitTarget);

        dialog.show();
    }

    /**
     * Set up the correct menu contents. Note: this method can only be called once the Dialog
     * has already been created - we need the dialog in order to be able to dismiss it in the
     * menu callbacks.
     */
    private static void setupMenuForHitTarget(final @NonNull Dialog dialog,
                                              final @NonNull NavigationView navigationView,
                                              final @NonNull IWebView.Callback callback,
                                              final @NonNull IWebView.HitTarget hitTarget) {
        navigationView.inflateMenu(R.menu.menu_browser_context);

        navigationView.getMenu().findItem(R.id.menu_link_share).setVisible(hitTarget.isLink);
        navigationView.getMenu().findItem(R.id.menu_link_copy).setVisible(hitTarget.isLink);
        navigationView.getMenu().findItem(R.id.menu_image_share).setVisible(hitTarget.isImage);
        navigationView.getMenu().findItem(R.id.menu_image_copy).setVisible(hitTarget.isImage);
        navigationView.getMenu().findItem(R.id.menu_image_save).setVisible(hitTarget.isImage);

        navigationView.setNavigationItemSelectedListener(new NavigationView.OnNavigationItemSelectedListener() {
            @Override
            public boolean onNavigationItemSelected(@NonNull MenuItem item) {
                dialog.dismiss();

                switch (item.getItemId()) {
                    case R.id.menu_link_share: {
                        TelemetryWrapper.shareLinkEvent();
                        final Intent shareIntent = new Intent(Intent.ACTION_SEND);
                        shareIntent.setType("text/plain");
                        shareIntent.putExtra(Intent.EXTRA_TEXT, hitTarget.linkURL);
                        dialog.getContext().startActivity(Intent.createChooser(shareIntent, dialog.getContext().getString(R.string.share_dialog_title)));
                        return true;
                    }
                    case R.id.menu_image_share: {
                        TelemetryWrapper.shareImageEvent();
                        final Intent shareIntent = new Intent(Intent.ACTION_SEND);
                        shareIntent.setType("text/plain");
                        shareIntent.putExtra(Intent.EXTRA_TEXT, hitTarget.imageURL);
                        dialog.getContext().startActivity(Intent.createChooser(shareIntent, dialog.getContext().getString(R.string.share_dialog_title)));
                        return true;
                    }
                    case R.id.menu_image_save: {
                        final Download download = new Download(hitTarget.imageURL, null, null, null, -1, Environment.DIRECTORY_PICTURES);
                        callback.onDownloadStart(download);
                        return true;
                    }
                    case R.id.menu_link_copy:
                    case R.id.menu_image_copy:
                        final ClipboardManager clipboard = (ClipboardManager)
                                dialog.getContext().getSystemService(Context.CLIPBOARD_SERVICE);
                        final Uri uri;

                        if (item.getItemId() == R.id.menu_link_copy) {
                            TelemetryWrapper.copyLinkEvent();
                            uri = Uri.parse(hitTarget.linkURL);
                        } else if (item.getItemId() == R.id.menu_image_copy) {
                            TelemetryWrapper.copyImageEvent();
                            uri = Uri.parse(hitTarget.imageURL);
                        } else {
                            throw new IllegalStateException("Unknown hitTarget type - cannot copy to clipboard");
                        }

                        final ClipData clip = ClipData.newUri(dialog.getContext().getContentResolver(), "URI", uri);
                        clipboard.setPrimaryClip(clip);
                        return true;
                    default:
                        throw new IllegalArgumentException("Unhandled menu item id=" + item.getItemId());
                }
            }
        });
    }
}
