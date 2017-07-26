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
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.activitystream.homepanel.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;
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

public class HighlightItem extends StreamItem implements IconCallback {
    private static final String LOGTAG = "GeckoHighlightItem";

    public static final int LAYOUT_ID = R.layout.activity_stream_card_history_item;

    private Highlight highlight;
    private int position;

    private final FaviconView pageIconView;
    private final TextView pageTitleView;
    private final TextView vTimeSince;
    private final TextView pageSourceView;
    private final TextView pageDomainView;
    private final ImageView pageSourceIconView;

    private Future<IconResponse> ongoingIconLoad;
    private int tilesMargin;

    public HighlightItem(final View itemView,
                         final HomePager.OnUrlOpenListener onUrlOpenListener,
                         final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        super(itemView);

        tilesMargin = itemView.getResources().getDimensionPixelSize(R.dimen.activity_stream_base_margin);

        pageTitleView = (TextView) itemView.findViewById(R.id.card_history_label);
        vTimeSince = (TextView) itemView.findViewById(R.id.card_history_time_since);
        pageIconView = (FaviconView) itemView.findViewById(R.id.icon);
        pageSourceView = (TextView) itemView.findViewById(R.id.card_history_source);
        pageDomainView = (TextView) itemView.findViewById(R.id.page);
        pageSourceIconView = (ImageView) itemView.findViewById(R.id.source_icon);

        final ImageView menuButton = (ImageView) itemView.findViewById(R.id.menu);

        menuButton.setImageDrawable(
                DrawableUtil.tintDrawable(menuButton.getContext(), R.drawable.menu, Color.LTGRAY));

        TouchTargetUtil.ensureTargetHitArea(menuButton, itemView);

        menuButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                        .set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, ActivityStreamTelemetry.Contract.TYPE_HIGHLIGHTS)
                        .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, position)
                        .forHighlightSource(highlight.getSource());

                ActivityStreamContextMenu.show(itemView.getContext(),
                        menuButton,
                        extras,
                        ActivityStreamContextMenu.MenuMode.HIGHLIGHT,
                        highlight,
                        onUrlOpenListener, onUrlOpenInBackgroundListener,
                        pageIconView.getWidth(), pageIconView.getHeight());

                Telemetry.sendUIEvent(
                        TelemetryContract.Event.SHOW,
                        TelemetryContract.Method.CONTEXT_MENU,
                        extras.build()
                );
            }
        });

        ViewUtil.enableTouchRipple(menuButton);
    }

    public void bind(Highlight highlight, int position, int tilesWidth, int tilesHeight) {
        this.highlight = highlight;
        this.position = position;

        final String backendHightlightTitle = highlight.getTitle();
        final String uiHighlightTitle = !TextUtils.isEmpty(backendHightlightTitle) ? backendHightlightTitle : highlight.getUrl();
        pageTitleView.setText(uiHighlightTitle);
        vTimeSince.setText(highlight.getRelativeTimeSpan());

        ViewGroup.LayoutParams layoutParams = pageIconView.getLayoutParams();
        layoutParams.width = tilesWidth - tilesMargin;
        layoutParams.height = tilesHeight;
        pageIconView.setLayoutParams(layoutParams);

        updateUiForSource(highlight.getSource());
        updatePageDomain();

        if (ongoingIconLoad != null) {
            ongoingIconLoad.cancel(true);
        }

        ongoingIconLoad = Icons.with(itemView.getContext())
                .pageUrl(highlight.getUrl())
                .skipNetwork()
                .build()
                .execute(this);
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

    private void updatePageDomain() {
        final UpdatePageDomainAsyncTask hostSLDTask = new UpdatePageDomainAsyncTask(itemView.getContext(),
                highlight.getUrl(), pageDomainView);
        hostSLDTask.execute();
    }

    @Override
    public void onIconResponse(IconResponse response) {
        pageIconView.updateImage(response);
    }

    /** Updates the text of the given view to the host second level domain. */
    private static class UpdatePageDomainAsyncTask extends URIUtils.GetHostSecondLevelDomainAsyncTask {
        private static final int VIEW_TAG_ID = R.id.page; // same as the view.

        private final WeakReference<TextView> pageDomainViewWeakReference;
        private final UUID viewTagAtStart;

        UpdatePageDomainAsyncTask(final Context contextReference, final String uriString, final TextView pageDomainView) {
            super(contextReference, uriString);
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
