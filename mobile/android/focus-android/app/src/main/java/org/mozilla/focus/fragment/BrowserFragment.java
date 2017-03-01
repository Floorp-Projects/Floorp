/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.content.Intent;
import android.graphics.drawable.TransitionDrawable;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.design.widget.Snackbar;
import android.support.v4.app.Fragment;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.PopupMenu;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.SettingsActivity;
import org.mozilla.focus.web.IWebView;

/**
 * Fragment for displaying the browser UI.
 */
public class BrowserFragment extends Fragment implements View.OnClickListener, PopupMenu.OnMenuItemClickListener {
    public static final String FRAGMENT_TAG = "browser";

    private static final int ANIMATION_DURATION = 300;

    private static final String ARGUMENT_URL = "url";

    public static BrowserFragment create(String url) {
        Bundle arguments = new Bundle();
        arguments.putString(ARGUMENT_URL, url);

        BrowserFragment fragment = new BrowserFragment();
        fragment.setArguments(arguments);

        return fragment;
    }


    private TransitionDrawable backgroundTransition;
    private TextView urlView;
    private IWebView webView;
    private ProgressBar progressView;
    private View lockView;
    private View menuView;

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

        backgroundTransition = (TransitionDrawable) view.findViewById(R.id.urlbar).getBackground();

        lockView = view.findViewById(R.id.lock);

        progressView = (ProgressBar) view.findViewById(R.id.progress);

        view.findViewById(R.id.erase).setOnClickListener(this);

        menuView = view.findViewById(R.id.menu);
        menuView.setOnClickListener(this);

        webView = (IWebView) view.findViewById(R.id.webview);

        webView.setCallback(new IWebView.Callback() {
            @Override
            public void onPageStarted(final String url) {
                lockView.setVisibility(View.GONE);

                urlView.setText(url);

                backgroundTransition.resetTransition();

                progressView.setVisibility(View.VISIBLE);
            }

            @Override
            public void onPageFinished(boolean isSecure) {
                backgroundTransition.startTransition(ANIMATION_DURATION);

                progressView.setVisibility(View.INVISIBLE);

                if (isSecure) {
                    lockView.setVisibility(View.VISIBLE);
                }
            }

            @Override
            public void onProgress(int progress) {
                progressView.setProgress(progress);
            }
        });

        webView.loadUrl(url);

        return view;
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.menu:
                final PopupMenu popupMenu = new PopupMenu(getActivity(), menuView);
                popupMenu.getMenuInflater().inflate(R.menu.menu_browser, popupMenu.getMenu());
                popupMenu.getMenu().findItem(R.id.forward).setEnabled(webView.canGoForward());
                popupMenu.getMenu().findItem(R.id.back).setEnabled(webView.canGoBack());
                popupMenu.setOnMenuItemClickListener(this);
                popupMenu.setGravity(Gravity.TOP);
                popupMenu.show();
                break;

            case R.id.url:
                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .add(R.id.container, UrlInputFragment.create(urlView.getText().toString()))
                        .addToBackStack("url_entry")
                        .commit();
                break;

            case R.id.erase:
                webView.cleanup();

                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .replace(R.id.container, HomeFragment.create(), HomeFragment.FRAGMENT_TAG)
                        .commit();

                Snackbar.make(getActivity().findViewById(android.R.id.content), R.string.feedback_erase, Snackbar.LENGTH_LONG).show();

                break;
        }
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.forward:
                webView.goForward();
                return true;

            case R.id.back:
                webView.goBack();
                return true;

            case R.id.refresh:
                webView.reload();
                return true;

            case R.id.share:
                final Intent shareIntent = new Intent(Intent.ACTION_SEND);
                shareIntent.setType("text/plain");
                shareIntent.putExtra(Intent.EXTRA_TEXT, webView.getUrl());
                startActivity(shareIntent);
                return true;

            case R.id.open:
                // TODO: Switch to full featured browser (Issue #26)
                return true;

            case R.id.settings:
                final Intent settingsIntent = new Intent(getActivity(), SettingsActivity.class);
                startActivity(settingsIntent);
                return true;
        }

        return false;
    }

    public boolean canGoBack() {
        return webView.canGoBack();
    }

    public void goBack() {
        webView.goBack();
    }
}
