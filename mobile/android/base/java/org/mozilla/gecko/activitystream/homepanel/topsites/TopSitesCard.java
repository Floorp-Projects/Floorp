/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.topsites;

import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.support.v4.widget.TextViewCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.homepanel.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.activitystream.homepanel.model.TopSite;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.widget.FaviconView;

import java.util.concurrent.Future;

/* package-local */ class TopSitesCard extends RecyclerView.ViewHolder
        implements IconCallback {
    private final FaviconView faviconView;

    private final TextView title;
    private Future<IconResponse> ongoingIconLoad;

    private TopSite topSite;
    private int absolutePosition;

    private final HomePager.OnUrlOpenListener onUrlOpenListener;
    private final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    /* package-local */ TopSitesCard(final FrameLayout card, final HomePager.OnUrlOpenListener onUrlOpenListener, final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        super(card);

        faviconView = (FaviconView) card.findViewById(R.id.favicon);

        title = (TextView) card.findViewById(R.id.title);

        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;

        card.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                        .forTopSite(topSite)
                        .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, absolutePosition);

                ActivityStreamContextMenu.show(itemView.getContext(),
                        card,
                        extras,
                        ActivityStreamContextMenu.MenuMode.TOPSITE,
                        topSite,
                        onUrlOpenListener, onUrlOpenInBackgroundListener,
                        faviconView.getWidth(), faviconView.getHeight());

                Telemetry.sendUIEvent(
                        TelemetryContract.Event.SHOW,
                        TelemetryContract.Method.CONTEXT_MENU,
                        extras.build()
                );

                return true;
            }
        });

        ViewUtil.enableTouchRipple(card);
    }

    void bind(final TopSite topSite, final int absolutePosition) {
        this.topSite = topSite;
        this.absolutePosition = absolutePosition;

        if (ongoingIconLoad != null) {
            ongoingIconLoad.cancel(true);
        }

        ongoingIconLoad = Icons.with(itemView.getContext())
                .pageUrl(topSite.getUrl())
                .skipNetwork()
                .build()
                .execute(this);

        final Drawable pinDrawable;
        if (topSite.isPinned()) {
            pinDrawable = DrawableUtil.tintDrawable(itemView.getContext(), R.drawable.as_pin, Color.WHITE);
        } else {
            pinDrawable = null;
        }
        TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(title, pinDrawable, null, null, null);

        // setCenteredText() needs to have all drawable's in place to correctly layout the text,
        // so we need to wait with requesting the title until we've set our pin icon.
        ActivityStream.extractLabel(itemView.getContext(), topSite.getUrl(), true, new ActivityStream.LabelCallback() {
            @Override
            public void onLabelExtracted(String label) {
                // We use consistent padding all around the title, and the top padding is never modified,
                // so we can pass that in as the default padding:
                ViewUtil.setCenteredText(title, label, title.getPaddingTop());
            }
        });
    }

    @Override
    public void onIconResponse(IconResponse response) {
        faviconView.updateImage(response);
    }
}
