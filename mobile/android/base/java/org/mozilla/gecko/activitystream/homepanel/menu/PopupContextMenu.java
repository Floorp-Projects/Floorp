/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.menu;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.design.widget.NavigationView;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupWindow;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.activitystream.homepanel.model.WebpageModel;

/* package-private */ class PopupContextMenu
        extends ActivityStreamContextMenu {

    private final PopupWindow popupWindow;
    private final NavigationView navigationView;

    private final View contextMenuAnchor;

    public PopupContextMenu(final View contextMenuAnchor,
                            final View snackbarAnchor,
                            final ActivityStreamTelemetry.Extras.Builder telemetryExtraBuilder,
                            final MenuMode mode,
                            final WebpageModel item,
                            HomePager.OnUrlOpenListener onUrlOpenListener,
                            HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        super(snackbarAnchor,
                telemetryExtraBuilder,
                mode,
                item,
                onUrlOpenListener,
                onUrlOpenInBackgroundListener);
        final Context context = contextMenuAnchor.getContext();

        this.contextMenuAnchor = contextMenuAnchor;

        final LayoutInflater inflater = LayoutInflater.from(context);

        View card = inflater.inflate(R.layout.activity_stream_contextmenu_popupmenu, null);
        navigationView = (NavigationView) card.findViewById(R.id.menu);
        navigationView.setNavigationItemSelectedListener(this);

        popupWindow = new PopupWindow(context);
        popupWindow.setContentView(card);
        popupWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        popupWindow.setFocusable(true);

        // On a variety of devices running Android 4 and 5, PopupWindow is assigned height/width = 0 - we therefore
        // need to manually override that behaviour. There don't appear to be any reported issues
        // with devices running 6 (Marshmallow) or newer, so we should restrict the workaround
        // as much as possible:
        if (AppConstants.Versions.preMarshmallow) {
            popupWindow.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
            popupWindow.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        super.postInit();
    }

    @Override
    public MenuItem getItemByID(int id) {
        return navigationView.getMenu().findItem(id);
    }

    @Override
    public void show() {
        // By default popupWindow follows the pre-material convention of displaying the popup
        // below a View. We need to shift it over the view:
        popupWindow.showAsDropDown(contextMenuAnchor,
                0,
                -(contextMenuAnchor.getHeight() + contextMenuAnchor.getPaddingBottom()));
    }

    public void dismiss() {
        popupWindow.dismiss();
    }
}
