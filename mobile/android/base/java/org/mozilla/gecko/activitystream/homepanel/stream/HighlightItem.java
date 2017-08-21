/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.content.Context;
import android.graphics.Color;
import android.support.annotation.UiThread;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.activitystream.homepanel.StreamHighlightItemContextMenuListener;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.TouchTargetUtil;
import org.mozilla.gecko.util.URIUtils;
import org.mozilla.gecko.util.ViewUtil;

import java.lang.ref.WeakReference;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.UUID;

public class HighlightItem extends StreamItem {
    private static final String LOGTAG = "GeckoHighlightItem";

    public static final int LAYOUT_ID = R.layout.activity_stream_card_history_item;
    private static final double SIZE_RATIO = 0.75;

    private int position;

    private final StreamOverridablePageIconLayout pageIconLayout;
    private final TextView pageTitleView;
    private final TextView pageSourceView;
    private final TextView pageDomainView;
    private final ImageView pageSourceIconView;
    private final ImageView menuButton;

    public HighlightItem(final View itemView, final StreamHighlightItemContextMenuListener contextMenuListener) {
        super(itemView);

        pageTitleView = (TextView) itemView.findViewById(R.id.card_history_label);
        pageIconLayout = (StreamOverridablePageIconLayout) itemView.findViewById(R.id.icon);
        pageSourceView = (TextView) itemView.findViewById(R.id.card_history_source);
        pageDomainView = (TextView) itemView.findViewById(R.id.page);
        pageSourceIconView = (ImageView) itemView.findViewById(R.id.source_icon);

        menuButton = (ImageView) itemView.findViewById(R.id.menu);
        menuButton.setImageDrawable(
                DrawableUtil.tintDrawable(menuButton.getContext(), R.drawable.menu, Color.LTGRAY));
        TouchTargetUtil.ensureTargetHitArea(menuButton, itemView);
        ViewUtil.enableTouchRipple(menuButton);
        menuButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                contextMenuListener.openContextMenu(HighlightItem.this, position,
                        ActivityStreamTelemetry.Contract.INTERACTION_MENU_BUTTON);
            }
        });
    }

    public void bind(Highlight highlight, int position, int tilesWidth) {
        this.position = position;

        final String backendHightlightTitle = highlight.getTitle();
        final String uiHighlightTitle = !TextUtils.isEmpty(backendHightlightTitle) ? backendHightlightTitle : highlight.getUrl();
        pageTitleView.setText(uiHighlightTitle);

        ViewGroup.LayoutParams layoutParams = pageIconLayout.getLayoutParams();
        layoutParams.width = tilesWidth;
        layoutParams.height = (int) Math.floor(tilesWidth * SIZE_RATIO);
        pageIconLayout.setLayoutParams(layoutParams);

        updateUiForSource(highlight.getSource());
        updatePageDomain(highlight);
        pageIconLayout.updateIcon(highlight.getUrl(), highlight.getImageUrl());
    }

    private void updateUiForSource(Utils.HighlightSource source) {
        switch (source) {
            case BOOKMARKED:
                pageSourceView.setText(R.string.activity_stream_highlight_label_bookmarked);
                pageSourceView.setVisibility(View.VISIBLE);
                pageSourceIconView.setImageResource(R.drawable.ic_as_bookmarked);
                break;
            case VISITED:
                pageSourceView.setText(R.string.activity_stream_highlight_label_visited);
                pageSourceView.setVisibility(View.VISIBLE);
                pageSourceIconView.setImageResource(R.drawable.ic_as_visited);
                break;
            default:
                pageSourceView.setVisibility(View.INVISIBLE);
                pageSourceIconView.setImageResource(0);
                break;
        }
    }

    private void updatePageDomain(final Highlight highlight) {
        final URI highlightURI;
        try {
            highlightURI = new URI(highlight.getUrl());
        } catch (final URISyntaxException e) {
            // If the URL is invalid, there's not much extra processing we can do on it.
            pageDomainView.setText(highlight.getUrl());
            return;
        }

        final UpdatePageDomainAsyncTask updatePageDomainTask = new UpdatePageDomainAsyncTask(itemView.getContext(),
                highlightURI, pageDomainView);
        updatePageDomainTask.execute();
    }

    public View getContextMenuAnchor() {
        return menuButton;
    }

    public int getTileWidth() {
        return pageIconLayout.getWidth();
    }

    public int getTileHeight() {
        return pageIconLayout.getHeight();
    }

    /** Updates the text of the given view to the host second level domain. */
    private static class UpdatePageDomainAsyncTask extends URIUtils.GetFormattedDomainAsyncTask {
        private static final int VIEW_TAG_ID = R.id.page; // same as the view.

        private final WeakReference<TextView> pageDomainViewWeakReference;
        private final UUID viewTagAtStart;

        UpdatePageDomainAsyncTask(final Context contextReference, final URI uri, final TextView pageDomainView) {
            super(contextReference, uri, false, 0); // hostSLD.
            this.pageDomainViewWeakReference = new WeakReference<>(pageDomainView);

            // See isTagSameAsStartTag for details.
            viewTagAtStart = UUID.randomUUID();
            pageDomainView.setTag(VIEW_TAG_ID, viewTagAtStart);
        }

        @Override
        protected void onPostExecute(final String hostSLD) {
            super.onPostExecute(hostSLD);
            final TextView viewToUpdate = pageDomainViewWeakReference.get();

            if (viewToUpdate == null || !isTagSameAsStartTag(viewToUpdate)) {
                return;
            }

            // hostSLD will be the empty String if the host cannot be determined. This is fine: it's very unlikely
            // and the page title will be URL if there's an error there so we wouldn't want to write the URL again here.
            viewToUpdate.setText(hostSLD);
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
