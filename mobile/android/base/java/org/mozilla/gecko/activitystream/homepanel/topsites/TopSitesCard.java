/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.topsites;

import android.content.Context;
import android.support.annotation.UiThread;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.homepanel.model.TopSite;
import org.mozilla.gecko.activitystream.homepanel.stream.TopPanelRow;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
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
    private final ImageView pinIconView;
    private Future<IconResponse> ongoingIconLoad;

    private TopSite topSite;
    private int absolutePosition;

    /* package-local */ TopSitesCard(final FrameLayout card, final TopPanelRow.OnCardLongClickListener onCardLongClickListener) {
        super(card);

        faviconView = (FaviconView) card.findViewById(R.id.favicon);
        title = (TextView) card.findViewById(R.id.title);
        pinIconView = (ImageView) card.findViewById(R.id.pin_icon);

        card.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                if (onCardLongClickListener != null) {
                    return onCardLongClickListener.onLongClick(topSite, absolutePosition, card, faviconView.getWidth(), faviconView.getHeight());
                }
                return false;
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

        if (TextUtils.isEmpty(topSite.getUrl())) {
            // Sometimes we get top sites without or with an empty URL - even though we do not allow
            // this anywhere in our UI. However with 'sync' we are not in full control of the data.
            // Whenever the URL is empty or null we just clear a potentially previously set icon.
            faviconView.clearImage();
        } else {
            ongoingIconLoad = Icons.with(itemView.getContext())
                    .pageUrl(topSite.getUrl())
                    .skipNetwork()
                    .forActivityStream()
                    .build()
                    .execute(this);
        }

        pinIconView.setVisibility(topSite.isPinned() ? View.VISIBLE : View.GONE);

        setTopSiteTitle(topSite);
    }

    private void setTopSiteTitle(final TopSite topSite) {
        URI topSiteURI = null; // not final so we can use in the Exception case.
        boolean isInvalidURI = false;
        try {
            topSiteURI = new URI(topSite.getUrl());
        } catch (final URISyntaxException e) {
            isInvalidURI = true;
        }

        final boolean isSiteSuggestedFromDistribution = BrowserDB.from(itemView.getContext()).getSuggestedSites()
                .containsSiteAndSiteIsFromDistribution(topSite.getUrl());

        // Some already installed distributions are unlikely to be updated (OTA, system) and their suggested
        // site titles were written for the old top sites, where we had more room to display titles: we want
        // to provide them with more lines. However, it's complex to distinguish a distribution intended for
        // the old top sites and the new one so for code simplicity, we allow all distributions more lines for titles.
        title.setMaxLines(isSiteSuggestedFromDistribution ? 2 : 1);

        // We use page titles with distributions because that's what the creators of those distributions expect to
        // be shown. Also, we need a valid URI for our preferred case so we stop here if we don't have one.
        final String pageTitle = topSite.getTitle();
        if (isInvalidURI || isSiteSuggestedFromDistribution) {
            final String updateText = !TextUtils.isEmpty(pageTitle) ? pageTitle : topSite.getUrl();
            setTopSiteTitleHelper(title, updateText); // See comment below regarding setCenteredText.

        // This is our preferred case: we display "subdomain.domain". People refer to sites by their domain ("it's
        // on wikipedia!") and it's a clean look so we display the domain if we can.
        //
        // If the path is non-empty, we'd normally go to the case below (see that comment). However, if there's no
        // title, we'd prefer to fallback on "subdomain.domain" rather than a url, which is really ugly.
        } else if (URIUtils.isPathEmpty(topSiteURI) ||
                (!URIUtils.isPathEmpty(topSiteURI) && TextUtils.isEmpty(pageTitle))) {
            // Our AsyncTask calls setCenteredText(), which needs to have all drawable's in place to correctly
            // layout the text, so we need to wait with requesting the title until we've set our pin icon.
            final UpdateCardTitleAsyncTask titleAsyncTask = new UpdateCardTitleAsyncTask(itemView.getContext(),
                    topSiteURI, title);
            titleAsyncTask.execute();

        // We have a site with a path that has a non-empty title. It'd be impossible to distinguish multiple sites
        // with "subdomain.domain" so if there's a path, we have to use something else: "domain/path" would overflow
        // before it's useful so we use the page title.
        } else {
            setTopSiteTitleHelper(title, pageTitle); // See comment above regarding setCenteredText.
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
