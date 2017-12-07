/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;

import org.mozilla.focus.R;
import org.mozilla.focus.menu.context.WebContextMenu;
import org.mozilla.focus.session.Session;
import org.mozilla.focus.utils.SupportUtils;
import org.mozilla.focus.web.Download;
import org.mozilla.focus.web.IWebView;

public class InfoFragment extends WebFragment {
    private ProgressBar progressView;
    private View webView;

    private static final String ARGUMENT_URL = "url";

    public static InfoFragment create(String url) {
        Bundle arguments = new Bundle();
        arguments.putString(ARGUMENT_URL, url);

        InfoFragment fragment = new InfoFragment();
        fragment.setArguments(arguments);

        return fragment;
    }

    @NonNull
    @Override
    public View inflateLayout(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_info, container, false);
        progressView = view.findViewById(R.id.progress);
        webView = view.findViewById(R.id.webview);

        final String url = getInitialUrl();
        if (!(url.startsWith("http://") || url.startsWith("https://"))) {
            // Hide webview until content has loaded, if we're loading built in about/rights/etc
            // pages: this avoid a white flash (on slower devices) between the screen appearing,
            // and the about/right/etc content appearing. We don't do this for SUMO and other
            // external pages, because they are both light-coloured, and significantly slower loading.
            webView.setVisibility(View.INVISIBLE);
        }

        applyLocale();

        return view;
    }

    @Override
    public void onCreateViewCalled() {}

    @Override
    public IWebView.Callback createCallback() {
        return new IWebView.Callback() {
            @Override
            public void onPageStarted(final String url) {
                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading));

                progressView.setVisibility(View.VISIBLE);
            }

            @Override
            public void onPageFinished(boolean isSecure) {
                progressView.announceForAccessibility(getString(R.string.accessibility_announcement_loading_finished));

                progressView.setVisibility(View.INVISIBLE);

                if (webView.getVisibility() != View.VISIBLE) {
                    webView.setVisibility(View.VISIBLE);
                }
            }

            @Override
            public void onSecurityChanged(boolean isSecure, String host, String organization) {}

            @Override
            public void onProgress(int progress) {
                progressView.setProgress(progress);
            }

            @Override
            public void onDownloadStart(Download download) {}

            @Override
            public void onLongPress(IWebView.HitTarget hitTarget) {
                if (getArguments().get(ARGUMENT_URL).equals(SupportUtils.getSumoURLForTopic(getContext(), SupportUtils.SumoTopic.ADD_SEARCH_ENGINE))) {
                    WebContextMenu.show(getActivity(), this, hitTarget);
                }
            }

            @Override
            public void onURLChanged(String url) {}

            @Override
            public void onRequest(boolean isTriggeredByUserGesture) {}

            @Override
            public void onEnterFullScreen(@NonNull IWebView.FullscreenCallback callback, @Nullable View view) {}

            @Override
            public void onExitFullScreen() {}

            @Override
            public void countBlockedTracker() {}

            @Override
            public void resetBlockedTrackers() {}

            @Override
            public void onBlockingStateChanged(boolean isBlockingEnabled) {}
        };
    }

    @Override
    public Session getSession() {
        return null;
    }

    @Nullable
    @Override
    public String getInitialUrl() {
        return getArguments().getString(ARGUMENT_URL);
    }
}
