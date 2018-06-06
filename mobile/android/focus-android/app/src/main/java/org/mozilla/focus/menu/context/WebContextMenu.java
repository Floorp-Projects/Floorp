/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.menu.context;

import android.app.Dialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.design.widget.NavigationView;
import android.support.v7.app.AlertDialog;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.session.Source;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.telemetry.TelemetryWrapper.BrowserContextMenuValue;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.web.Download;
import org.mozilla.focus.web.IWebView;

/**
 * The context menu shown when long pressing a URL or an image inside the WebView.
 */
public class WebContextMenu {

    private static View createTitleView(final @NonNull Context context, final @NonNull String title) {
        final TextView titleView = (TextView) LayoutInflater.from(context).inflate(R.layout.context_menu_title, null);
        titleView.setText(title);
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

        final View view = LayoutInflater.from(context).inflate(R.layout.context_menu, null);
        builder.setView(view);

        builder.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                // What type of element was long-pressed
                final BrowserContextMenuValue value;
                if (hitTarget.isImage && hitTarget.isLink) {
                    value = BrowserContextMenuValue.ImageWithLink;
                } else if (hitTarget.isImage) {
                    value = BrowserContextMenuValue.Image;
                } else {
                    value = BrowserContextMenuValue.Link;
                }

                // This even is only sent when the back button is pressed, or when a user
                // taps outside of the dialog:
                TelemetryWrapper.cancelWebContextMenuEvent(value);
            }
        });

        final Dialog dialog = builder.create();
        dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));

        final NavigationView menu = (NavigationView) view.findViewById(R.id.context_menu);
        setupMenuForHitTarget(dialog, menu, callback, hitTarget);

        final TextView warningView = (TextView) view.findViewById(R.id.warning);
        if (hitTarget.isImage) {
            //noinspection deprecation -- Mew API is only available on 24+
            warningView.setText(Html.fromHtml(
                    context.getString(R.string.contextmenu_image_warning, context.getString(R.string.app_name))));
        } else {
            warningView.setVisibility(View.GONE);
        }

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

        navigationView.getMenu().findItem(R.id.menu_new_tab).setVisible(hitTarget.isLink);
        navigationView.getMenu().findItem(R.id.menu_link_share).setVisible(hitTarget.isLink);
        navigationView.getMenu().findItem(R.id.menu_link_copy).setVisible(hitTarget.isLink);
        navigationView.getMenu().findItem(R.id.menu_image_share).setVisible(hitTarget.isImage);
        navigationView.getMenu().findItem(R.id.menu_image_copy).setVisible(hitTarget.isImage);

        navigationView.getMenu().findItem(R.id.menu_image_save).setVisible(
                hitTarget.isImage && UrlUtils.isHttpOrHttps(hitTarget.imageURL));

        navigationView.setNavigationItemSelectedListener(new NavigationView.OnNavigationItemSelectedListener() {
            @Override
            public boolean onNavigationItemSelected(@NonNull MenuItem item) {
                dialog.dismiss();

                switch (item.getItemId()) {
                    case R.id.menu_new_tab: {
                        SessionManager.getInstance().createSession(Source.MENU, hitTarget.linkURL);
                        TelemetryWrapper.openLinkInNewTabEvent();
                        return true;
                    }
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
                        TelemetryWrapper.saveImageEvent();
                        return true;
                    }
                    case R.id.menu_link_copy:
                    case R.id.menu_image_copy:
                        final ClipboardManager clipboard = (ClipboardManager)
                                dialog.getContext().getSystemService(Context.CLIPBOARD_SERVICE);
                        if (clipboard == null) {
                            return true;
                        }
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
