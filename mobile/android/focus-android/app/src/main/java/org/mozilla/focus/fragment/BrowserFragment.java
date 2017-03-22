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
import android.webkit.WebView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.SettingsActivity;
import org.mozilla.focus.menu.BrowserMenu;
import org.mozilla.focus.open.OpenWithFragment;
import org.mozilla.focus.utils.Browsers;
import org.mozilla.focus.utils.ViewUtils;
import org.mozilla.focus.utils.IntentUtils;
import org.mozilla.focus.web.IWebView;

/**
 * Fragment for displaying the browser UI.
 */
public class BrowserFragment extends WebFragment implements View.OnClickListener {
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
    private ProgressBar progressView;
    private View lockView;
    private View menuView;

    private View forwardButton;
    private View backButton;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public String getInitialUrl() {
        return getArguments().getString(ARGUMENT_URL);
    }

    @Override
    public View inflateLayout(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_browser, container, false);

        urlView = (TextView) view.findViewById(R.id.url);
        urlView.setText(getInitialUrl());
        urlView.setOnClickListener(this);

        backgroundTransition = (TransitionDrawable) view.findViewById(R.id.background).getBackground();

        final View refreshButton = view.findViewById(R.id.refresh);
        if (refreshButton != null) {
            refreshButton.setOnClickListener(this);
        }

        if ((forwardButton = view.findViewById(R.id.forward)) != null) {
            forwardButton.setOnClickListener(this);
        }

        if ((backButton = view.findViewById(R.id.back)) != null) {
            backButton.setOnClickListener(this);
        }

        lockView = view.findViewById(R.id.lock);

        progressView = (ProgressBar) view.findViewById(R.id.progress);

        view.findViewById(R.id.erase).setOnClickListener(this);

        menuView = view.findViewById(R.id.menu);
        menuView.setOnClickListener(this);

        return view;
    }

    @Override
    public IWebView.Callback createCallback() {
        return new IWebView.Callback() {
            @Override
            public void onPageStarted(final String url) {
                lockView.setVisibility(View.GONE);

                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading));

                urlView.setText(url);

                backgroundTransition.resetTransition();

                progressView.setVisibility(View.VISIBLE);

                updateToolbarButtonStates();
            }

            @Override
            public void onPageFinished(boolean isSecure) {
                backgroundTransition.startTransition(ANIMATION_DURATION);

                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading_finished));

                progressView.setVisibility(View.INVISIBLE);

                if (isSecure) {
                    lockView.setVisibility(View.VISIBLE);
                }

                updateToolbarButtonStates();
            }

            @Override
            public void onProgress(int progress) {
                progressView.setProgress(progress);
            }

            @Override
            public boolean handleExternalUrl(final String url) {
                final IWebView webView = getWebView();

                return webView != null && IntentUtils.handleExternalUri(getContext(), webView, url);
            }
        };
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

            case R.id.erase: {
                final IWebView webView = getWebView();
                if (webView != null) {
                    webView.cleanup();
                }

                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .setCustomAnimations(0, R.anim.erase_animation)
                        .replace(R.id.container, HomeFragment.create(), HomeFragment.FRAGMENT_TAG)
                        .commit();

                ViewUtils.showBrandedSnackbar(getActivity().findViewById(android.R.id.content),
                        R.string.feedback_erase,
                        getResources().getInteger(R.integer.erase_snackbar_delay));

                break;
            }


            case R.id.back: {
                final IWebView webView = getWebView();
                if (webView != null) {
                    webView.goBack();
                }
                break;
            }

            case R.id.forward: {
                final IWebView webView = getWebView();
                if (webView != null) {
                    webView.goForward();
                }
                break;
            }

            case R.id.refresh: {
                final IWebView webView = getWebView();
                if (webView != null) {
                    webView.reload();
                }
                break;
            }

            case R.id.share: {
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

                final Intent shareIntent = new Intent(Intent.ACTION_SEND);
                shareIntent.setType("text/plain");
                shareIntent.putExtra(Intent.EXTRA_TEXT, webView.getUrl());
                startActivity(shareIntent);
                break;
            }

            case R.id.settings:
                final Intent settingsIntent = new Intent(getActivity(), SettingsActivity.class);
                startActivity(settingsIntent);
                break;

            case R.id.open_default: {
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

                final Browsers browsers = new Browsers(getContext(), webView.getUrl());

                final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(webView.getUrl()));
                intent.setPackage(browsers.getDefaultBrowser().packageName);
                startActivity(intent);
                break;
            }

            case R.id.open_firefox: {
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

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
                final IWebView webView = getWebView();
                if (webView == null) {
                    return;
                }

                final Browsers browsers = new Browsers(getContext(), webView.getUrl());

                final OpenWithFragment fragment = OpenWithFragment.newInstance(
                        browsers.getInstalledBrowsers(), webView.getUrl());
                fragment.show(getFragmentManager(),OpenWithFragment.FRAGMENT_TAG);

                break;
            }
        }
    }

    private void updateToolbarButtonStates() {
        if (forwardButton == null || backButton == null) {
            return;
        }

        final IWebView webView = getWebView();
        if (webView == null) {
            return;
        }

        final boolean canGoForward = webView.canGoForward();
        final boolean canGoBack = webView.canGoBack();

        forwardButton.setEnabled(canGoForward);
        forwardButton.setAlpha(canGoForward ? 1.0f : 0.5f);

        backButton.setEnabled(canGoBack);
        backButton.setAlpha(canGoBack ? 1.0f : 0.5f);
    }

    @Nullable
    public String getUrl() {
        final IWebView webView = getWebView();
        return webView != null ? webView.getUrl() : null;
    }

    public boolean canGoForward() {
        final IWebView webView = getWebView();
        return webView != null && webView.canGoForward();
    }

    public boolean canGoBack() {
        final IWebView webView = getWebView();
        return webView != null && webView.canGoBack();
    }

    public void goBack() {
        final IWebView webView = getWebView();
        if (webView != null) {
            webView.goBack();
        }
    }
}
