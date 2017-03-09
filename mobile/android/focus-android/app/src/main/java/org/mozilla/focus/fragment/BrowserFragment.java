/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.content.Intent;
import android.graphics.drawable.TransitionDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.SettingsActivity;
import org.mozilla.focus.menu.BrowserMenu;
import org.mozilla.focus.open.OpenWithFragment;
import org.mozilla.focus.utils.Browsers;
import org.mozilla.focus.utils.ViewUtils;
import org.mozilla.focus.web.IWebView;

/**
 * Fragment for displaying the browser UI.
 */
public class BrowserFragment extends Fragment implements View.OnClickListener {
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

                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading));

                urlView.setText(url);

                backgroundTransition.resetTransition();

                progressView.setVisibility(View.VISIBLE);
            }

            @Override
            public void onPageFinished(boolean isSecure) {
                backgroundTransition.startTransition(ANIMATION_DURATION);

                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading_finished));

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
                BrowserMenu menu = new BrowserMenu(getActivity(), this);
                menu.show(menuView);
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

                ViewUtils.showBrandedSnackbar(getActivity().findViewById(android.R.id.content),
                        R.string.feedback_erase);

                break;

            case R.id.forward:
                webView.goForward();
                break;

            case R.id.refresh:
                webView.reload();
                break;

            case R.id.share:
                final Intent shareIntent = new Intent(Intent.ACTION_SEND);
                shareIntent.setType("text/plain");
                shareIntent.putExtra(Intent.EXTRA_TEXT, webView.getUrl());
                startActivity(shareIntent);
                break;

            case R.id.settings:
                final Intent settingsIntent = new Intent(getActivity(), SettingsActivity.class);
                startActivity(settingsIntent);
                break;

            case R.id.open_default: {
                final Browsers browsers = new Browsers(getContext(), webView.getUrl());

                final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(webView.getUrl()));
                intent.setPackage(browsers.getDefaultBrowser().packageName);
                startActivity(intent);
                break;
            }

            case R.id.open_firefox: {
                final Browsers browsers = new Browsers(getContext(), webView.getUrl());

                if (browsers.isInstalled(Browsers.KnownBrowser.FIREFOX)) {
                    final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(webView.getUrl()));
                    intent.setPackage(Browsers.KnownBrowser.FIREFOX.packageName);
                    startActivity(intent);
                } else {
                    final Intent intent = new Intent(Intent.ACTION_VIEW,
                            Uri.parse("market://details?id=" + Browsers.KnownBrowser.FIREFOX));
                    startActivity(intent);
                }

                break;
            }

            case R.id.open_select_browser: {
                final Browsers browsers = new Browsers(getContext(), webView.getUrl());

                final OpenWithFragment fragment = OpenWithFragment.newInstance(
                        browsers.getInstalledBrowsers(), webView.getUrl());
                fragment.show(getFragmentManager(),OpenWithFragment.FRAGMENT_TAG);

                break;
            }
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        webView.setCallback(null);
    }

    public String getUrl() {
        return webView.getUrl();
    }

    public boolean canGoForward() {
        return webView.canGoForward();
    }

    public boolean canGoBack() {
        return webView.canGoBack();
    }

    public void goBack() {
        webView.goBack();
    }
}
