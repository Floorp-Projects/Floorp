/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.net.MalformedURLException;
import java.net.URL;

import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.R;

import com.squareup.picasso.Picasso;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.FrameLayout;
import android.widget.TextView;

public class PanelGridItemView extends FrameLayout {
    private static final String LOGTAG = "GeckoPanelGridItemView";

    private final ImageView mThumbnailView;

    public PanelGridItemView(Context context) {
        this(context, null);
    }

    public PanelGridItemView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.panelGridItemViewStyle);
    }

    public PanelGridItemView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        LayoutInflater.from(context).inflate(R.layout.panel_grid_item_view, this);
        mThumbnailView = (ImageView) findViewById(R.id.image);
    }

    public void updateFromCursor(Cursor cursor) {
        int imageIndex = cursor.getColumnIndexOrThrow(HomeItems.IMAGE_URL);
        final String imageUrl = cursor.getString(imageIndex);

        Picasso.with(getContext())
               .load(imageUrl)
               .into(mThumbnailView);
    }
}
