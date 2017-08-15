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
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.URIUtils;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.widget.FaviconView;

import java.lang.ref.WeakReference;
import java.net.URI;
import java.net.URISyntaxException;
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
                        /* shouldOverrideWithImageProvider */ false, // we only use favicons for top sites.
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

        setTopSiteTitle(topSite);
    }

    private void setTopSiteTitle(final TopSite topSite) {
        URI topSiteURI = null; // not final so we can use in the Exception case.
        boolean wasException = false;
        try {
            topSiteURI = new URI(topSite.getUrl());
        } catch (final URISyntaxException e) {
            wasException = true;
        }

        // At a high level, the logic is: if the path empty, use "subdomain.domain", otherwise use the
        // page title. From a UX perspective, people refer to domains by their name ("it's on wikipedia")
        // and it's a clean look. However, if a url has a path, it will not fit on the screen with the domain
        // so we need an alternative: the page title is an easy win (though not always perfect, e.g. when SEO
        // keywords are added). If we ever want better titles, we could create a heuristic to pull the title
        // from parts of the URL, page title, etc.
        if (wasException || !URIUtils.isPathEmpty(topSiteURI)) {
            // See comment below regarding setCenteredText.
            final String pageTitle = topSite.getTitle();
            final String updateText = !TextUtils.isEmpty(pageTitle) ? pageTitle : topSite.getUrl();
            setTopSiteTitleHelper(title, updateText);

        } else {
            // Our AsyncTask calls setCenteredText(), which needs to have all drawable's in place to correctly
            // layout the text, so we need to wait with requesting the title until we've set our pin icon.
            final UpdateCardTitleAsyncTask titleAsyncTask = new UpdateCardTitleAsyncTask(itemView.getContext(),
                    topSiteURI, title);
            titleAsyncTask.execute();
        }
    }

    private static void setTopSiteTitleHelper(final TextView textView, final String title) {
        // We use consistent padding all around the title, and the top padding is never modified,
        // so we can pass that in as the default padding:
        ViewUtil.setCenteredText(textView, title, textView.getPaddingTop());
    }

    @Override
    public void onIconResponse(IconResponse response) {
        faviconView.updateImage(response);
    }

    /** Updates the text of the given view to the page domain. */
    private static class UpdateCardTitleAsyncTask extends URIUtils.GetFormattedDomainAsyncTask {
        private static final int VIEW_TAG_ID = R.id.title; // same as the view.

        private final WeakReference<TextView> titleViewWeakReference;
        private final UUID viewTagAtStart;

        UpdateCardTitleAsyncTask(final Context contextReference, final URI uri, final TextView titleView) {
            super(contextReference, uri, false, 1); // subdomain.domain.
            this.titleViewWeakReference = new WeakReference<>(titleView);

            // See isTagSameAsStartTag for details.
            viewTagAtStart = UUID.randomUUID();
            titleView.setTag(VIEW_TAG_ID, viewTagAtStart);
        }

        @Override
        protected void onPostExecute(final String hostText) {
            super.onPostExecute(hostText);
            final TextView titleView = titleViewWeakReference.get();
            if (titleView == null || !isTagSameAsStartTag(titleView)) {
                return;
            }

            final String updateText;
            if (TextUtils.isEmpty(hostText)) {
                updateText = "";
            } else {
                updateText = StringUtils.stripCommonSubdomains(hostText);
            }
            setTopSiteTitleHelper(titleView, updateText);
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
