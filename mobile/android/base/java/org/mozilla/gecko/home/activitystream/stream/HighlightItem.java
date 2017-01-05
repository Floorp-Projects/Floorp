/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home.activitystream.stream;

import android.database.Cursor;
import android.graphics.Color;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.activitystream.menu.ActivityStreamContextMenu;
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

    String title;
    String url;
    JSONObject metadata;

    @Nullable
    Boolean isPinned;
    @Nullable Boolean isBookmarked;

    Utils.HighlightSource source;

    final FaviconView vIconView;
    final TextView vLabel;
    final TextView vTimeSince;
    final TextView vSourceView;
    final TextView vPageView;
    final ImageView vSourceIconView;

    private Future<IconResponse> ongoingIconLoad;
    private int tilesMargin;

    public HighlightItem(final View itemView,
                         final HomePager.OnUrlOpenListener onUrlOpenListener,
                         final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        super(itemView);

        tilesMargin = itemView.getResources().getDimensionPixelSize(R.dimen.activity_stream_base_margin);

        vLabel = (TextView) itemView.findViewById(R.id.card_history_label);
        vTimeSince = (TextView) itemView.findViewById(R.id.card_history_time_since);
        vIconView = (FaviconView) itemView.findViewById(R.id.icon);
        vSourceView = (TextView) itemView.findViewById(R.id.card_history_source);
        vPageView = (TextView) itemView.findViewById(R.id.page);
        vSourceIconView = (ImageView) itemView.findViewById(R.id.source_icon);

        final ImageView menuButton = (ImageView) itemView.findViewById(R.id.menu);

        menuButton.setImageDrawable(
                DrawableUtil.tintDrawable(menuButton.getContext(), R.drawable.menu, Color.LTGRAY));

        TouchTargetUtil.ensureTargetHitArea(menuButton, itemView);

        menuButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                        .set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, ActivityStreamTelemetry.Contract.TYPE_HIGHLIGHTS)
                        .forHighlightSource(source);

                ActivityStreamContextMenu.show(v.getContext(),
                        menuButton,
                        extras,
                        ActivityStreamContextMenu.MenuMode.HIGHLIGHT,
                        title, url, isBookmarked, isPinned,
                        onUrlOpenListener, onUrlOpenInBackgroundListener,
                        vIconView.getWidth(), vIconView.getHeight());

                Telemetry.sendUIEvent(
                        TelemetryContract.Event.SHOW,
                        TelemetryContract.Method.CONTEXT_MENU,
                        extras.build()
                );
            }
        });

        ViewUtil.enableTouchRipple(menuButton);
    }

    public void bind(Cursor cursor, int tilesWidth, int tilesHeight) {
        final long time = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Highlights.DATE));
        final String ago = DateUtils.getRelativeTimeSpanString(time, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS, 0).toString();

        title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.History.TITLE));
        url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));
        source = Utils.highlightSource(cursor);

        try {
            final String rawMetadata = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Highlights.METADATA));
            if (rawMetadata != null) {
                metadata = new JSONObject(rawMetadata);
            }
        } catch (JSONException e) {
            Log.w(LOGTAG, "JSONException while parsing page metadata", e);
        }

        vLabel.setText(title);
        vTimeSince.setText(ago);

        ViewGroup.LayoutParams layoutParams = vIconView.getLayoutParams();
        layoutParams.width = tilesWidth - tilesMargin;
        layoutParams.height = tilesHeight;
        vIconView.setLayoutParams(layoutParams);

        updateStateForSource(source);
        updateUiForSource(source);
        updatePage(metadata, url);

        if (ongoingIconLoad != null) {
            ongoingIconLoad.cancel(true);
        }

        ongoingIconLoad = Icons.with(itemView.getContext())
                .pageUrl(url)
                .skipNetwork()
                .build()
                .execute(this);
    }

    private void updateStateForSource(Utils.HighlightSource source) {
        // We can only be certain of bookmark state if an item is a bookmark item.
        // Otherwise, due to the underlying highlights query, we have to look up states when
        // menus are displayed.
        switch (source) {
            case BOOKMARKED:
                isBookmarked = true;
                isPinned = null;
                break;
            case VISITED:
                isBookmarked = null;
                isPinned = null;
                break;
            default:
                throw new IllegalArgumentException("Unknown source: " + source);
        }
    }

    private void updateUiForSource(Utils.HighlightSource source) {
        switch (source) {
            case BOOKMARKED:
                vSourceView.setText(R.string.activity_stream_highlight_label_bookmarked);
                vSourceView.setVisibility(View.VISIBLE);
                vSourceIconView.setImageResource(R.drawable.ic_as_bookmarked);
                break;
            case VISITED:
                vSourceView.setText(R.string.activity_stream_highlight_label_visited);
                vSourceView.setVisibility(View.VISIBLE);
                vSourceIconView.setImageResource(R.drawable.ic_as_visited);
                break;
            default:
                vSourceView.setVisibility(View.INVISIBLE);
                vSourceIconView.setImageResource(0);
                break;
        }
    }

    private void updatePage(final JSONObject metadata, final String url) {
        // First try to set the provider name from the page's metadata.

        try {
            if (metadata != null && metadata.has("provider")) {
                vPageView.setText(metadata.getString("provider"));
                return;
            }
        } catch (JSONException e) {
            // Broken JSON? Continue with fallback.
        }

        // If there's no provider name available then let's try to extract one from the URL.

        extractLabel(itemView.getContext(), url, false, new ActivityStream.LabelCallback() {
            @Override
            public void onLabelExtracted(String label) {
                vPageView.setText(TextUtils.isEmpty(label) ? url : label);
            }
        });
    }

    @Override
    public void onIconResponse(IconResponse response) {
        vIconView.updateImage(response);
    }
}
