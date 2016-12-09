/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.View;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.mozilla.focus.R;
import org.mozilla.focus.webkit.TrackingProtectionWebViewClient;
import org.mozilla.focus.widget.UrlBar;

public class BrowserActivity extends Activity {
    private WebView webView;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        getWindow().setStatusBarColor(Color.TRANSPARENT);

        setContentView(R.layout.activity_browser);

        final UrlBar urlBar = (UrlBar) findViewById(R.id.urlbar);

        webView = (WebView) findViewById(R.id.webview);
        final TrackingProtectionWebViewClient webViewClient = new TrackingProtectionWebViewClient(getApplicationContext()) {
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                webView.setVisibility(View.VISIBLE);

                urlBar.onPageStarted(url);

                super.onPageStarted(view, url, favicon);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                urlBar.onPageFinished();

                super.onPageFinished(view, url);
            }
        };

        webView.setWebViewClient(webViewClient);

        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onProgressChanged(WebView view, int newProgress) {
                urlBar.onProgressUpdate(newProgress);
            }
        });

        urlBar.setOnUrlActionListener(new UrlBar.OnUrlAction() {
            @Override
            public void onUrlEntered(String url) {
                webViewClient.notifyCurrentURL(url);

                webView.loadUrl(url);
            }

            @Override
            public void onErase() {
                finish();
            }

            @Override
            public void onEnteredEditMode() {
                webView.setVisibility(View.INVISIBLE);
            }

            @Override
            public void onEditCancelled(boolean hasLoadedPage) {
                if (hasLoadedPage) {
                    webView.setVisibility(View.VISIBLE);
                } else {
                    finish();
                }
            }
        });

        final Intent intent = getIntent();
        if (Intent.ACTION_VIEW.equals(intent.getAction())) {
            urlBar.enterUrl(intent.getDataString());
        }
    }

    @Override
    public void onBackPressed() {
        if (webView.canGoBack()) {
            webView.goBack();
            return;
        }

        super.onBackPressed();
    }
}
