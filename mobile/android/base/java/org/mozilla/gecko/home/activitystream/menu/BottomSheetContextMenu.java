/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.menu;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
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
import org.mozilla.gecko.home.activitystream.model.Item;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.widget.FaviconView;

import static org.mozilla.gecko.activitystream.ActivityStream.extractLabel;

/* package-private */ class BottomSheetContextMenu
        extends ActivityStreamContextMenu {


    private final BottomSheetDialog bottomSheetDialog;

    private final NavigationView navigationView;

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

        final LayoutInflater inflater = LayoutInflater.from(context);
        final View content = inflater.inflate(R.layout.activity_stream_contextmenu_bottomsheet, null);

        bottomSheetDialog = new BottomSheetDialog(context);
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
        bottomSheetDialog.show();
    }

    public void dismiss() {
        bottomSheetDialog.dismiss();
    }

}
