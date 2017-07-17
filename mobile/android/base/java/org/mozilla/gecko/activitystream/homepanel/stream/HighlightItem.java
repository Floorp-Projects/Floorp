/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.graphics.Color;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.activitystream.homepanel.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.TouchTargetUtil;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.widget.FaviconView;

import java.util.concurrent.Future;

import static org.mozilla.gecko.activitystream.ActivityStream.extractLabel;

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

        pageTitleView.setText(highlight.getTitle());
        vTimeSince.setText(highlight.getRelativeTimeSpan());

        ViewGroup.LayoutParams layoutParams = pageIconView.getLayoutParams();
        layoutParams.width = tilesWidth - tilesMargin;
        layoutParams.height = tilesHeight;
        pageIconView.setLayoutParams(layoutParams);

        updateUiForSource(highlight.getSource());
        updatePage();

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

    private void updatePage() {
        // First try to set the provider name from the page's metadata.
        if (highlight.getMetadata().hasProvider()) {
            pageDomainView.setText(highlight.getMetadata().getProvider());
            return;
        }

        // If there's no provider name available then let's try to extract one from the URL.
        extractLabel(itemView.getContext(), highlight.getUrl(), false, new ActivityStream.LabelCallback() {
            @Override
            public void onLabelExtracted(String label) {
                pageDomainView.setText(TextUtils.isEmpty(label) ? highlight.getUrl() : label);
            }
        });
    }

    @Override
    public void onIconResponse(IconResponse response) {
        pageIconView.updateImage(response);
    }
}
