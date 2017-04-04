/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.web.IWebView;

/**
 * Base implementation for fragments that use an IWebView instance. Based on Android's WebViewFragment.
 */
public abstract class WebFragment extends Fragment {
    private IWebView webView;
    private boolean isWebViewAvailable;

    /**
     * Inflate a layout for this fragment. The layout needs to contain a view implementing IWebView
     * with the id set to "webview".
     */
    @NonNull
    public abstract View inflateLayout(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState);

    public abstract IWebView.Callback createCallback();

    /**
     * Get the initial URL to load after the view has been created.
     */
    @Nullable
    public abstract String getInitialUrl();

    @Override
    public final View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflateLayout(inflater, container, savedInstanceState);

        webView = (IWebView) view.findViewById(R.id.webview);
        isWebViewAvailable = true;
        webView.setCallback(createCallback());

        if (savedInstanceState == null) {
            final String url = getInitialUrl();
            if (url != null) {
                webView.loadUrl(url);
            }
        } else {
            webView.restoreWebviewState(savedInstanceState);
        }

        return view;
    }

    @Override
    public void onPause() {
        webView.onPause();

        super.onPause();
    }

    @Override
    public void onResume() {
        webView.onResume();

        super.onResume();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        webView.onSaveInstanceState(outState);

        super.onSaveInstanceState(outState);
    }

    @Override
    public void onDestroy() {
        if (webView != null) {
            webView.setCallback(null);
            webView.destroy();
            webView = null;
        }

        super.onDestroy();
    }

    @Override
    public void onDestroyView() {
        isWebViewAvailable = false;

        super.onDestroyView();
    }

    @Nullable
    protected IWebView getWebView() {
        return isWebViewAvailable ? webView : null;
    }
}
