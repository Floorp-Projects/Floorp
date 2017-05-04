/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.menu;

import android.app.Activity;
import android.content.Context;
import android.support.design.widget.BottomSheetBehavior;
import android.support.design.widget.BottomSheetDialog;
import android.support.design.widget.NavigationView;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.activitystream.homepanel.model.Item;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.widget.FaviconView;

import static org.mozilla.gecko.activitystream.ActivityStream.extractLabel;

/* package-private */ class BottomSheetContextMenu
        extends ActivityStreamContextMenu {


    private final BottomSheetDialog bottomSheetDialog;

    private final NavigationView navigationView;

    final View content;
    final View activityView;

    public BottomSheetContextMenu(final Context context,
                                  final ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder,
                                  final MenuMode mode,
                                  final Item item,
                                  HomePager.OnUrlOpenListener onUrlOpenListener,
                                  HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener,
                                  final int tilesWidth, final int tilesHeight) {

        super(context,
                telemetryExtraBuilder,
                mode,
                item,
                onUrlOpenListener,
                onUrlOpenInBackgroundListener);

        // The View encompassing the activity area
        this.activityView = ((Activity) context).findViewById(android.R.id.content);

        bottomSheetDialog = new BottomSheetDialog(context);
        final LayoutInflater inflater = LayoutInflater.from(context);
        this.content = inflater.inflate(R.layout.activity_stream_contextmenu_bottomsheet, (ViewGroup) activityView, false);

        bottomSheetDialog.setContentView(content);

        ((TextView) content.findViewById(R.id.title)).setText(item.getTitle());

        extractLabel(context, item.getUrl(), false, new ActivityStream.LabelCallback() {
                public void onLabelExtracted(String label) {
                    ((TextView) content.findViewById(R.id.url)).setText(label);
                }
        });

        // Copy layouted parameters from the Highlights / TopSites items to ensure consistency
        final FaviconView faviconView = (FaviconView) content.findViewById(R.id.icon);
        ViewGroup.LayoutParams layoutParams = faviconView.getLayoutParams();
        layoutParams.width = tilesWidth;
        layoutParams.height = tilesHeight;
        faviconView.setLayoutParams(layoutParams);

        Icons.with(context)
                .pageUrl(item.getUrl())
                .skipNetwork()
                .build()
                .execute(new IconCallback() {
                    @Override
                    public void onIconResponse(IconResponse response) {
                        faviconView.updateImage(response);
                    }
                });

        navigationView = (NavigationView) content.findViewById(R.id.menu);
        navigationView.setNavigationItemSelectedListener(this);

        super.postInit();
    }

    @Override
    public MenuItem getItemByID(int id) {
        return navigationView.getMenu().findItem(id);
    }

    @Override
    public void show() {
        // Try to use a 16:9 "keyline", i.e. we leave a 16:9 window of activity content visible
        // above the menu. We only do this in portrait mode - in landscape mode, 16:9 is likely
        // to be similar to the size of the display, so we'd only show very little, or even none of,
        // the menu.
        // Note that newer versions of the support library (possibly 25+) will do this automatically,
        // so we can remove that code then.
        if (activityView.getHeight() > activityView.getWidth()) {
            final int peekHeight = activityView.getHeight() - (activityView.getWidth() * 9 / 16);

            BottomSheetBehavior<View> bsBehaviour = BottomSheetBehavior.from((View) content.getParent());
            bsBehaviour.setPeekHeight(peekHeight);
        }

        bottomSheetDialog.show();
    }

    public void dismiss() {
        bottomSheetDialog.dismiss();
    }

}
