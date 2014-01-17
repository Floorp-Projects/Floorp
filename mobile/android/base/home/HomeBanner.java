/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.Html;
import android.text.Spanned;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

public class HomeBanner extends LinearLayout
                        implements GeckoEventListener {
    private static final String LOGTAG = "GeckoHomeBanner";

    public HomeBanner(Context context) {
        this(context, null);
    }

    public HomeBanner(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.home_banner, this);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        // Tapping on the close button will ensure that the banner is never
        // showed again on this session.
        final ImageButton closeButton = (ImageButton) findViewById(R.id.close);

        // The drawable should have 50% opacity.
        closeButton.getDrawable().setAlpha(127);

        closeButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                HomeBanner.this.setVisibility(View.GONE);
            }
        });

        setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Send the current message id back to JS.
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("HomeBanner:Click", (String) getTag()));
            }
        });

        GeckoAppShell.getEventDispatcher().registerEventListener("HomeBanner:Data", this);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("HomeBanner:Get", null));
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        GeckoAppShell.getEventDispatcher().unregisterEventListener("HomeBanner:Data", this);
     }

    public boolean isDismissed() {
        return (getVisibility() == View.GONE);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            // Store the current message id to pass back to JS in the view's OnClickListener.
            setTag(message.getString("id"));

            // Display styled text from an HTML string.
            final Spanned text = Html.fromHtml(message.getString("text"));
            final TextView textView = (TextView) findViewById(R.id.text);

            // Update the banner message on the UI thread.
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    textView.setText(text);
                    setVisibility(View.VISIBLE);
                }
            });
        } catch (JSONException e) {
            Log.e(LOGTAG, "Exception handling " + event + " message", e);
            return;
        }

        final String iconURI = message.optString("iconURI");
        final ImageView iconView = (ImageView) findViewById(R.id.icon);

        if (TextUtils.isEmpty(iconURI)) {
            // Hide the image view if we don't have an icon to show.
            iconView.setVisibility(View.GONE);
            return;
        }

        BitmapUtils.getDrawable(getContext(), iconURI, new BitmapUtils.BitmapLoader() {
            @Override
            public void onBitmapFound(final Drawable d) {
                // Bail if getDrawable doesn't find anything.
                if (d == null) {
                    iconView.setVisibility(View.GONE);
                    return;
                }

                // Update the banner icon on the UI thread.
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        iconView.setImageDrawable(d);
                    }
                });
            }
        });
    }
}
