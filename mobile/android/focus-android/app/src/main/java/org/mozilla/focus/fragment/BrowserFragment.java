/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.design.widget.Snackbar;
import android.support.v4.app.Fragment;
import android.support.v4.content.ContextCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.github.clans.fab.FloatingActionMenu;

import org.mozilla.focus.R;
import org.mozilla.focus.webkit.TrackingProtectionWebViewClient;

/**
 * Fragment for displaying the browser UI.
 */
public class BrowserFragment extends Fragment implements View.OnClickListener {
    public static final String FRAGMENT_TAG = "browser";

    private static final String ARGUMENT_URL = "url";

    public static BrowserFragment create(String url) {
        Bundle arguments = new Bundle();
        arguments.putString(ARGUMENT_URL, url);

        BrowserFragment fragment = new BrowserFragment();
        fragment.setArguments(arguments);

        return fragment;
    }

    private TextView urlView;
    private View urlBarView;
    private WebView webView;
    private FloatingActionMenu menu;
    private ProgressBar progressView;
    private View lockView;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_browser, container, false);
        final String url = getArguments().getString(ARGUMENT_URL);

        urlView = (TextView) view.findViewById(R.id.url);
        urlView.setText(url);
        urlView.setOnClickListener(this);

        urlBarView = view.findViewById(R.id.urlbar);
        progressView = (ProgressBar) view.findViewById(R.id.progress);
        lockView = view.findViewById(R.id.lock);

        view.findViewById(R.id.erase).setOnClickListener(this);

        webView = (WebView) view.findViewById(R.id.webview);

        webView.getSettings().setJavaScriptEnabled(true);

        // Enabling built in zooming shows the controls by default
        webView.getSettings().setBuiltInZoomControls(true);
        // So we hide the controls after enabling zooming
        webView.getSettings().setDisplayZoomControls(false);

        webView.setVerticalScrollBarEnabled(true);
        webView.setHorizontalScrollBarEnabled(true);

        menu = (FloatingActionMenu) view.findViewById(R.id.menu);

        final TrackingProtectionWebViewClient webViewClient = new TrackingProtectionWebViewClient(getContext().getApplicationContext()) {
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                webView.setVisibility(View.VISIBLE);

                urlView.setText(url);

                urlBarView.setBackgroundColor(ContextCompat.getColor(getActivity(), R.color.colorTitleBar));

                progressView.setVisibility(View.VISIBLE);
                lockView.setVisibility(View.GONE);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                urlBarView.setBackgroundResource(R.drawable.urlbar_gradient);

                progressView.setVisibility(View.INVISIBLE);

                if (view.getCertificate() != null) {
                    lockView.setVisibility(View.VISIBLE);
                }
            }
        };

        webView.setWebViewClient(webViewClient);

        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onProgressChanged(WebView view, int newProgress) {
                progressView.setProgress(newProgress);
            }
        });

        view.findViewById(R.id.refresh).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                webView.reload();
                menu.close(true);
            }
        });

        view.findViewById(R.id.open).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(Intent.ACTION_SEND);
                intent.setType("text/plain");
                intent.putExtra(Intent.EXTRA_TEXT, webView.getUrl());
                startActivity(intent);
                menu.close(true);
            }
        });

        webViewClient.notifyCurrentURL(url);

        webView.loadUrl(url);

        return view;
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.url:
                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .add(R.id.container, UrlInputFragment.create(urlView.getText().toString()))
                        .commit();
                break;

            case R.id.erase:
                cleanup();

                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .replace(R.id.container, HomeFragment.create(), HomeFragment.FRAGMENT_TAG)
                        .commit();

                Snackbar.make(urlView, R.string.feedback_erase, Snackbar.LENGTH_LONG).show();

                break;
        }
    }

    private void cleanup() {
        webView.clearFormData();
        webView.clearHistory();
        webView.clearMatches();
        webView.clearSslPreferences();
        webView.clearCache(true);
    }
}
