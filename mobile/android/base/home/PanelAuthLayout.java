/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomeConfig.AuthConfig;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;

import android.content.Context;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.squareup.picasso.Picasso;

class PanelAuthLayout extends LinearLayout {

    public PanelAuthLayout(Context context, PanelConfig panelConfig) {
        super(context);

        final AuthConfig authConfig = panelConfig.getAuthConfig();
        if (authConfig == null) {
            throw new IllegalStateException("Can't create PanelAuthLayout without a valid AuthConfig");
        }

        setOrientation(LinearLayout.VERTICAL);
        LayoutInflater.from(context).inflate(R.layout.panel_auth_layout, this);

        final TextView messageView = (TextView) findViewById(R.id.message);
        messageView.setText(authConfig.getMessageText());

        final Button buttonView = (Button) findViewById(R.id.button);
        buttonView.setText(authConfig.getButtonText());

        final String panelId = panelConfig.getId();
        buttonView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("HomePanels:Authenticate", panelId));
            }
        });

        final ImageView imageView = (ImageView) findViewById(R.id.image);
        final String imageUrl = authConfig.getImageUrl();

        if (TextUtils.isEmpty(imageUrl)) {
            // Use a default image if an image URL isn't specified.
            imageView.setImageResource(R.drawable.icon_home_empty_firefox);
        } else {
            Picasso.with(getContext())
                   .load(imageUrl)
                   .into(imageView);
        }
    }
}
