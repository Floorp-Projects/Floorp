/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.util.ArrayMap;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.View;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.HtmlLoader;
import org.mozilla.focus.utils.SupportUtils;

import java.util.Map;

/**
 * A generic activity that supports showing additional information in a WebView. This is useful
 * for showing any web based content, including About/Help/Rights, and also SUMO pages.
 */
public class InfoActivity extends AppCompatActivity {
    private static final String EXTRA_URL = "extra_url";
    private static final String EXTRA_TITLE = "extra_title";

    public static final Intent getIntentFor(final Context context, final String url, final String title) {
        final Intent intent = new Intent(context, InfoActivity.class);

        intent.putExtra(EXTRA_URL, url);
        intent.putExtra(EXTRA_TITLE, title);

        return intent;
    }

    public static final Intent getAboutIntent(final Context context) {
        return getIntentFor(context, "about:", context.getResources().getString(R.string.menu_about));
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_info);

        final String url = getIntent().getStringExtra(EXTRA_URL);
        final String title = getIntent().getStringExtra(EXTRA_TITLE);

        final WebView webView = (WebView) findViewById(R.id.webview);
        // By default WebView will try to open links and redirects in an external browser (and sumo
        // links usually use a redirect to get to the desired target page). Somehow, setting a default
        // WebViewClient() is enough to disable that behaviour.
        webView.setWebViewClient(new WebViewClient());

        loadURL(url, webView);

        final Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        toolbar.setTitle(title);

        setSupportActionBar(toolbar);

        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        getSupportActionBar().setDisplayShowHomeEnabled(true);

        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                finish();
            }
        });
    }

    private void loadURL(final String url, final WebView webView) {
        if (url.equals("about:")) {
            final Resources resources = webView.getContext().getResources();

            final Map<String, String> substitutionMap = new ArrayMap<>();
            final String appName = webView.getContext().getResources().getString(R.string.app_name);
            final String learnMoreURL = SupportUtils.getManifestoURL();

            final String aboutContent = resources.getString(R.string.about_content, appName, learnMoreURL);
            substitutionMap.put("%about-content%", aboutContent);

            final String data = HtmlLoader.loadResourceFile(webView.getContext(), R.raw.about, substitutionMap);
            // We use a file:/// base URL so that we have the right origin to load file:/// css and
            // image resources.
            webView.loadDataWithBaseURL("file:///android_res/raw/about.html", data, "text/html", "UTF-8", null);
        } else {
            webView.loadUrl(url);
        }

    }
}
