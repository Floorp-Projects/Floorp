/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.topsites;

import android.graphics.Color;
import android.support.v7.widget.RecyclerView;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.widget.FaviconView;

import java.util.concurrent.Future;

class TopSitesCard extends RecyclerView.ViewHolder implements IconCallback {
    private final FaviconView faviconView;

    private final TextView title;
    private final ImageView menuButton;
    private Future<IconResponse> ongoingIconLoad;

    public TopSitesCard(FrameLayout card) {
        super(card);

        faviconView = (FaviconView) card.findViewById(R.id.favicon);

        title = (TextView) card.findViewById(R.id.title);
        menuButton = (ImageView) card.findViewById(R.id.menu);
    }

    void bind(TopSitesPageAdapter.TopSite topSite) {
        final String label = ActivityStream.extractLabel(topSite.url, true);
        title.setText(label);

        if (ongoingIconLoad != null) {
            ongoingIconLoad.cancel(true);
        }

        ongoingIconLoad = Icons.with(itemView.getContext())
                .pageUrl(topSite.url)
                .skipNetwork()
                .build()
                .execute(this);
    }

    @Override
    public void onIconResponse(IconResponse response) {
        faviconView.updateImage(response);

        final int tintColor = !response.hasColor() || response.getColor() == Color.WHITE ? Color.LTGRAY : Color.WHITE;

        menuButton.setImageDrawable(
                DrawableUtil.tintDrawable(menuButton.getContext(), R.drawable.menu, tintColor));
    }
}
