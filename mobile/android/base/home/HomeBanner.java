/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

public class HomeBanner extends LinearLayout {

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
    }

    public boolean isDismissed() {
        return (getVisibility() == View.GONE);
    }
}
