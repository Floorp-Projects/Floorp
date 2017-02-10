/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.content.Context;
import android.graphics.Color;
import android.support.annotation.Nullable;
import android.support.v7.widget.AppCompatTextView;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;

/**
 * WebViewProvider implementation for creating a Gecko based implementation of IWebView.
 */
public class WebViewProvider {
    public static View create(Context context, AttributeSet attrs) {
        // TODO: Import GeckoView into project (Issue #13)

        return new DummyGeckoView(context, attrs);
    }

    private static class DummyGeckoView extends AppCompatTextView implements IWebView {
        public DummyGeckoView(Context context, @Nullable AttributeSet attrs) {
            super(context, attrs);

            setBackgroundColor(Color.BLACK);
            setGravity(Gravity.CENTER);
        }

        @Override
        public void setCallback(Callback callback) {

        }

        @Override
        public void reload() {

        }

        @Override
        public String getUrl() {
            return getText().toString();
        }

        @Override
        public void loadUrl(String url) {
            setText(url);
        }

        @Override
        public void cleanup() {

        }
    }
}
