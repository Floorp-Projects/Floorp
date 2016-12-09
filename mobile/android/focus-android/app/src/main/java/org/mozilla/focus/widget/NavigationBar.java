/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.webkit.WebView;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import org.mozilla.focus.R;

public class NavigationBar extends LinearLayout {
    public interface NavigationListener {
        void onBack();

        void onForward();

        void onRefresh();

        void onOpen();
    }

    private ImageButton backButton;
    private ImageButton forwardButton;
    private ImageButton refreshButton;
    private ImageButton openButton;

    private NavigationListener listener;

    public NavigationBar(Context context) {
        super(context);
        init(context);
    }

    public NavigationBar(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public void setNavigationListener(NavigationListener listener) {
        this.listener = listener;
    }

    private void init(Context context) {
        setOrientation(HORIZONTAL);

        LayoutInflater.from(context).inflate(R.layout.item_navigationbar, this, true);

        backButton = (ImageButton) findViewById(R.id.back);
        forwardButton = (ImageButton) findViewById(R.id.forward);
        refreshButton = (ImageButton) findViewById(R.id.refresh);
        openButton = (ImageButton) findViewById(R.id.open);

        backButton.setEnabled(false);
        forwardButton.setEnabled(false);

        backButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                if (listener != null) {
                    listener.onBack();
                }
            }
        });

        forwardButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                if (listener != null) {
                    listener.onForward();
                }
            }
        });

        refreshButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                if (listener != null) {
                    listener.onRefresh();
                }
            }
        });

        openButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                if (listener != null) {
                    listener.onOpen();
                }
            }
        });
    }

    public void updateState(WebView webView) {
        backButton.setEnabled(webView.canGoBack());
        forwardButton.setEnabled(webView.canGoForward());
    }
}
