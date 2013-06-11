/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.Favicons;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.widget.FaviconView;

import android.content.Context;
import android.content.res.TypedArray;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

public class TwoLinePageRow extends LinearLayout {

    private final TextView mTitle;
    private final TextView mUrl;
    private final FaviconView mFavicon;

    public TwoLinePageRow(Context context) {
        this(context, null);
    }

    public TwoLinePageRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        setGravity(Gravity.CENTER_VERTICAL);

        LayoutInflater.from(context).inflate(R.layout.two_line_page_row, this);
        mTitle = (TextView) findViewById(R.id.title);
        mUrl = (TextView) findViewById(R.id.url);
        mFavicon = (FaviconView) findViewById(R.id.favicon);
    }

    private void setTitle(String title) {
        mTitle.setText(title);
    }

    private void setUrl(String url) {
        mUrl.setText(url);
    }

    private void setUrl(int stringId) {
        mUrl.setText(stringId);
    }

    private void setUrlIcon(int resourceId) {
        mUrl.setCompoundDrawablesWithIntrinsicBounds(resourceId, 0, 0, 0);
    }

    private void setFaviconWithUrl(Bitmap favicon, String url) {
        mFavicon.updateImage(favicon, url);
    }

    public void updateFromCursor(Cursor cursor) {
        if (cursor == null) {
            return;
        }

        int titleIndex = cursor.getColumnIndexOrThrow(URLColumns.TITLE);
        final String title = cursor.getString(titleIndex);

        int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
        final String url = cursor.getString(urlIndex);

        // Use the URL instead of an empty title for consistency with the normal URL
        // bar view - this is the equivalent of getDisplayTitle() in Tab.java
        setTitle(TextUtils.isEmpty(title) ? url : title);

        // Update the url with "Switch to tab" if needed.
        // FIXME: Bug 877469: Add back switch to tab functionality.
        Integer tabId = null;
        if (tabId != null) {
            setUrl(R.string.switch_to_tab);
            setUrlIcon(R.drawable.ic_url_bar_tab);
        } else {
            setUrl(url);
            setUrlIcon(0);
        }

        int faviconIndex = cursor.getColumnIndex(URLColumns.FAVICON);
        if (faviconIndex != -1) {
            byte[] b = cursor.getBlob(faviconIndex);

            Bitmap favicon = null;
            if (b != null) {
                Bitmap bitmap = BitmapUtils.decodeByteArray(b);
                if (bitmap != null) {
                    favicon = Favicons.getInstance().scaleImage(bitmap);
                }
            }

            setFaviconWithUrl(favicon, url);
        } else {
            // If favicons is not on the cursor, try to fetch it from the memory cache
            setFaviconWithUrl(Favicons.getInstance().getFaviconFromMemCache(url), url);
        }
    }
}
