/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.topsites;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.support.annotation.UiThread;
import android.support.v4.widget.TextViewCompat;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.homepanel.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.activitystream.homepanel.model.TopSite;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.TouchTargetUtil;
import org.mozilla.gecko.util.URIUtils;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.widget.FaviconView;

import java.lang.ref.WeakReference;
import java.util.UUID;
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

        // Our AsyncTask calls setCenteredText(), which needs to have all drawable's in place to correctly
        // layout the text, so we need to wait with requesting the title until we've set our pin icon.
        //
        // todo (bug 1382036): iOS' algorithm actually looks like:
        // if let provider = site.metadata?.providerName {
        //     titleLabel.text = provider.lowercased()
        // } else {
        //     titleLabel.text = site.tileURL.hostSLD
        // }
        //
        // But it's non-trivial to get the provider name with the top site so it's a follow-up.
        final UpdateCardTitleAsyncTask titleAsyncTask = new UpdateCardTitleAsyncTask(itemView.getContext(),
                topSite.getUrl(), title);
        titleAsyncTask.execute();
    }

    @Override
    public void onIconResponse(IconResponse response) {
        faviconView.updateImage(response);
    }

    /** Updates the text of the given view to the page domain. */
    private static class UpdateCardTitleAsyncTask extends URIUtils.GetHostSecondLevelDomainAsyncTask {
        private static final int VIEW_TAG_ID = R.id.title; // same as the view.

        private final WeakReference<TextView> titleViewWeakReference;
        private final UUID viewTagAtStart;

        UpdateCardTitleAsyncTask(final Context contextReference, final String uriString, final TextView titleView) {
            super(contextReference, uriString);
            this.titleViewWeakReference = new WeakReference<>(titleView);

            // See isTagSameAsStartTag for details.
            viewTagAtStart = UUID.randomUUID();
            titleView.setTag(VIEW_TAG_ID, viewTagAtStart);
        }

        @Override
        protected void onPostExecute(final String hostSLD) {
            super.onPostExecute(hostSLD);
            final TextView titleView = titleViewWeakReference.get();
            if (titleView == null || !isTagSameAsStartTag(titleView)) {
                return;
            }

            final String updateText = !TextUtils.isEmpty(hostSLD) ? hostSLD : uriString;

            // We use consistent padding all around the title, and the top padding is never modified,
            // so we can pass that in as the default padding:
            ViewUtil.setCenteredText(titleView, updateText, titleView.getPaddingTop());
        }

        /**
         * Returns true if the tag on the given view matches the tag from the constructor. We do this to ensure
         * the View we're making this request for hasn't been re-used by the time this request completes.
         */
        @UiThread
        private boolean isTagSameAsStartTag(final View viewToCheck) {
            return viewTagAtStart.equals(viewToCheck.getTag(VIEW_TAG_ID));
        }
    }
}
