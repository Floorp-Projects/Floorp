/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.topsites;

import android.graphics.Color;
import android.support.annotation.Nullable;
import android.support.v4.widget.TextViewCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.activitystream.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.util.TouchTargetUtil;
import org.mozilla.gecko.widget.FaviconView;

import java.util.EnumSet;
import java.util.concurrent.Future;

class TopSitesCard extends RecyclerView.ViewHolder
        implements IconCallback, View.OnClickListener {
    private final FaviconView faviconView;

    private final TextView title;
    private final ImageView menuButton;
    private Future<IconResponse> ongoingIconLoad;

    private String url;
    private int type;
    @Nullable private Boolean isBookmarked;

    private final HomePager.OnUrlOpenListener onUrlOpenListener;
    private final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    public TopSitesCard(FrameLayout card, final HomePager.OnUrlOpenListener onUrlOpenListener, final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        super(card);

        faviconView = (FaviconView) card.findViewById(R.id.favicon);

        title = (TextView) card.findViewById(R.id.title);
        menuButton = (ImageView) card.findViewById(R.id.menu);

        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;

        card.setOnClickListener(this);

        TouchTargetUtil.ensureTargetHitArea(menuButton, card);
        menuButton.setOnClickListener(this);

        ViewUtil.enableTouchRipple(menuButton);
    }

    void bind(final TopSitesPageAdapter.TopSite topSite) {
        ActivityStream.extractLabel(itemView.getContext(), topSite.url, true, new ActivityStream.LabelCallback() {
            @Override
            public void onLabelExtracted(String label) {
                title.setText(label);
            }
        });

        this.url = topSite.url;
        this.type = topSite.type;
        this.isBookmarked = topSite.isBookmarked;

        if (ongoingIconLoad != null) {
            ongoingIconLoad.cancel(true);
        }

        ongoingIconLoad = Icons.with(itemView.getContext())
                .pageUrl(topSite.url)
                .skipNetwork()
                .build()
                .execute(this);

        final int pinResourceId = (isPinned(this.type) ? R.drawable.pin : 0);
        TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(title, pinResourceId, 0, 0, 0);
    }

    @Override
    public void onIconResponse(IconResponse response) {
        faviconView.updateImage(response);

        final int tintColor = !response.hasColor() || response.getColor() == Color.WHITE ? Color.LTGRAY : Color.WHITE;

        menuButton.setImageDrawable(
                DrawableUtil.tintDrawable(menuButton.getContext(), R.drawable.menu, tintColor));
    }

    @Override
    public void onClick(View clickedView) {
        ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                .set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, ActivityStreamTelemetry.Contract.TYPE_TOPSITES)
                .forTopSiteType(type);

        if (clickedView == itemView) {
            onUrlOpenListener.onUrlOpen(url, EnumSet.noneOf(HomePager.OnUrlOpenListener.Flags.class));

            Telemetry.sendUIEvent(
                    TelemetryContract.Event.LOAD_URL,
                    TelemetryContract.Method.LIST_ITEM,
                    extras.build()
            );
        } else if (clickedView == menuButton) {
            ActivityStreamContextMenu.show(clickedView.getContext(),
                    menuButton,
                    extras,
                    ActivityStreamContextMenu.MenuMode.TOPSITE,
                    title.getText().toString(), url,

                    isBookmarked, isPinned(type),

                    onUrlOpenListener, onUrlOpenInBackgroundListener,
                    faviconView.getWidth(), faviconView.getHeight());

            Telemetry.sendUIEvent(
                    TelemetryContract.Event.SHOW,
                    TelemetryContract.Method.CONTEXT_MENU,
                    extras.build()
            );
        }
    }

    private boolean isPinned(int type) {
        return type == BrowserContract.TopSites.TYPE_PINNED;
    }
}
