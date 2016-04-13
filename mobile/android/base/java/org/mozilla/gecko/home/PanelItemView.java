/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.home.HomeConfig.ItemType;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Color;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

class PanelItemView extends LinearLayout {
    private final TextView titleView;
    private final TextView descriptionView;
    private final ImageView imageView;
    private final LinearLayout titleDescContainerView;
    private final ImageView backgroundView;

    private PanelItemView(Context context, int layoutId) {
        super(context);

        LayoutInflater.from(context).inflate(layoutId, this);
        titleView = (TextView) findViewById(R.id.title);
        descriptionView = (TextView) findViewById(R.id.description);
        imageView = (ImageView) findViewById(R.id.image);
        backgroundView = (ImageView) findViewById(R.id.background);
        titleDescContainerView = (LinearLayout) findViewById(R.id.title_desc_container);
    }

    public void updateFromCursor(Cursor cursor) {
        int titleIndex = cursor.getColumnIndexOrThrow(HomeItems.TITLE);
        final String titleText = cursor.getString(titleIndex);

        // Only show title if the item has one
        final boolean hasTitle = !TextUtils.isEmpty(titleText);
        titleView.setVisibility(hasTitle ? View.VISIBLE : View.GONE);
        if (hasTitle) {
            titleView.setText(titleText);
        }

        int descriptionIndex = cursor.getColumnIndexOrThrow(HomeItems.DESCRIPTION);
        final String descriptionText = cursor.getString(descriptionIndex);

        // Only show description if the item has one
        // Descriptions are not supported for IconItemView objects (Bug 1157539)
        final boolean hasDescription = !TextUtils.isEmpty(descriptionText);
        if (descriptionView != null) {
            descriptionView.setVisibility(hasDescription ? View.VISIBLE : View.GONE);
            if (hasDescription) {
                descriptionView.setText(descriptionText);
            }
        }
        if (titleDescContainerView != null) {
            titleDescContainerView.setVisibility(hasTitle || hasDescription ? View.VISIBLE : View.GONE);
        }

        int imageIndex = cursor.getColumnIndexOrThrow(HomeItems.IMAGE_URL);
        final String imageUrl = cursor.getString(imageIndex);

        // Only try to load the image if the item has define image URL
        final boolean hasImageUrl = !TextUtils.isEmpty(imageUrl);
        imageView.setVisibility(hasImageUrl ? View.VISIBLE : View.GONE);

        if (hasImageUrl) {
            ImageLoader.with(getContext())
                       .load(imageUrl)
                       .into(imageView);
        }

        final int columnIndexBackgroundColor = cursor.getColumnIndex(HomeItems.BACKGROUND_COLOR);
        if (columnIndexBackgroundColor != -1) {
            final String color = cursor.getString(columnIndexBackgroundColor);
            if (!TextUtils.isEmpty(color)) {
                setBackgroundColor(Color.parseColor(color));
            }
        }

        // Backgrounds are only supported for IconItemView objects (Bug 1157539)
        final int columnIndexBackgroundUrl = cursor.getColumnIndex(HomeItems.BACKGROUND_URL);
        if (columnIndexBackgroundUrl != -1) {
            final String backgroundUrl = cursor.getString(columnIndexBackgroundUrl);
            if (backgroundView != null && !TextUtils.isEmpty(backgroundUrl)) {
                ImageLoader.with(getContext())
                        .load(backgroundUrl)
                        .fit()
                        .into(backgroundView);
            }
        }
    }

    private static class ArticleItemView extends PanelItemView {
        private ArticleItemView(Context context) {
            super(context, R.layout.panel_article_item);
            setOrientation(LinearLayout.HORIZONTAL);
        }
    }

    private static class ImageItemView extends PanelItemView {
        private ImageItemView(Context context) {
            super(context, R.layout.panel_image_item);
            setOrientation(LinearLayout.VERTICAL);
        }
    }

    private static class IconItemView extends PanelItemView {
        private IconItemView(Context context) {
            super(context, R.layout.panel_icon_item);
        }
    }

    public static PanelItemView create(Context context, ItemType itemType) {
        switch (itemType) {
            case ARTICLE:
                return new ArticleItemView(context);

            case IMAGE:
                return new ImageItemView(context);

            case ICON:
                return new IconItemView(context);

            default:
                throw new IllegalArgumentException("Could not create panel item view from " + itemType);
        }
    }
}
