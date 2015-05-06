/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import java.net.MalformedURLException;
import java.net.URISyntaxException;
import java.net.URL;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.search.providers.SearchEngine;

import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.provider.Settings;
import android.support.v4.app.Fragment;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

public class PostSearchFragment extends Fragment {

    private static final String LOG_TAG = "PostSearchFragment";

    private SearchEngine engine;

    private ProgressBar progressBar;
    private WebView webview;
    private View errorView;

    private String resultsPageHost;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View mainView = inflater.inflate(R.layout.search_fragment_post_search, container, false);

        progressBar = (ProgressBar) mainView.findViewById(R.id.progress_bar);

        webview = (WebView) mainView.findViewById(R.id.webview);
        webview.setWebChromeClient(new ChromeClient());
        webview.setWebViewClient(new ResultsWebViewClient());

        // This is required for our greasemonkey terror script.
        webview.getSettings().setJavaScriptEnabled(true);

        return mainView;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        webview.setWebChromeClient(null);
        webview.setWebViewClient(null);
        webview = null;
        progressBar = null;
    }

    public void startSearch(SearchEngine engine, String query) {
        this.engine = engine;

        final String url = engine.resultsUriForQuery(query);
        // Only load urls if the url is different than the webview's current url.
        if (!TextUtils.equals(webview.getUrl(), url)) {
            resultsPageHost = null;
            webview.loadUrl(Constants.ABOUT_BLANK);
            webview.loadUrl(url);
        }
    }

    /**
     * A custom WebViewClient that intercepts every page load. This allows
     * us to decide whether to load the url here, or send it to Android
     * as an intent. It also handles network errors.
     */
    private class ResultsWebViewClient extends WebViewClient {

        // Whether or not there is a network error.
        private boolean networkError;

        @Override
        public void onPageStarted(WebView view, final String url, Bitmap favicon) {
            // Reset the error state.
            networkError = false;
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
            // Ignore about:blank URL loads and the first results page we try to load.
            if (TextUtils.equals(url, Constants.ABOUT_BLANK) || resultsPageHost == null) {
                return false;
            }

            String host = null;
            try {
                host = new URL(url).getHost();
            } catch (MalformedURLException e) {
                Log.e(LOG_TAG, "Error getting host from URL loading in webview", e);
            }

            // If the host name is the same as the results page, don't override the URL load, but
            // do update the query in the search bar if possible.
            if (TextUtils.equals(resultsPageHost, host)) {
                // This won't work for results pages that redirect (e.g. Google in different country)
                final String query = engine.queryForResultsUrl(url);
                if (!TextUtils.isEmpty(query)) {
                    ((AcceptsSearchQuery) getActivity()).onQueryChange(query);
                }
                return false;
            }

            try {
                // If the url URI does not have an intent scheme, the intent data will be the entire
                // URI and its action will be ACTION_VIEW.
                final Intent i = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);

                // If the intent URI didn't specify a package, open this in Fennec.
                if (i.getPackage() == null) {
                    i.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
                    Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL,
                            TelemetryContract.Method.CONTENT, "search-result");
                } else {
                    Telemetry.sendUIEvent(TelemetryContract.Event.LAUNCH,
                            TelemetryContract.Method.INTENT, "search-result");
                }

                startActivity(i);
                return true;
            } catch (URISyntaxException e) {
                Log.e(LOG_TAG, "Error parsing intent URI", e);
            }

            return false;
        }

        @Override
        public void onReceivedError(WebView view, int errorCode, String description, String failingUrl) {
            Log.e(LOG_TAG, "Error loading search results: " + description);

            networkError = true;

            if (errorView == null) {
                final ViewStub errorViewStub = (ViewStub) getView().findViewById(R.id.error_view_stub);
                errorView = errorViewStub.inflate();

                ((ImageView) errorView.findViewById(R.id.empty_image)).setImageResource(R.drawable.network_error);
                ((TextView) errorView.findViewById(R.id.empty_title)).setText(R.string.network_error_title);

                final TextView message = (TextView) errorView.findViewById(R.id.empty_message);
                message.setText(R.string.network_error_message);
                message.setTextColor(getResources().getColor(R.color.network_error_link));
                message.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        startActivity(new Intent(Settings.ACTION_SETTINGS));
                    }
                });
            }
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            // Make sure the error view is hidden if the network error was fixed.
            if (errorView != null) {
                errorView.setVisibility(networkError ? View.VISIBLE : View.GONE);
                webview.setVisibility(networkError ? View.GONE : View.VISIBLE);
            }

            if (!TextUtils.equals(url, Constants.ABOUT_BLANK) && resultsPageHost == null) {
                try {
                    resultsPageHost = new URL(url).getHost();
                } catch (MalformedURLException e) {
                    Log.e(LOG_TAG, "Error getting host from results page URL", e);
                }
            }
        }
    }

    /**
     * A custom WebChromeClient that allows us to inject CSS into
     * the head of the HTML and to monitor pageload progress.
     *
     * We use the WebChromeClient because it provides a hook to the titleReceived
     * event. Once the title is available, the page will have started parsing the
     * head element. The script injects its CSS into the head element.
     */
    private class ChromeClient extends WebChromeClient {

        @Override
        public void onReceivedTitle(final WebView view, String title) {
            view.loadUrl(engine.getInjectableJs());
        }

        @Override
        public void onProgressChanged(WebView view, int newProgress) {
            if (newProgress < 100) {
                if (progressBar.getVisibility() == View.INVISIBLE) {
                    progressBar.setVisibility(View.VISIBLE);
                }
                progressBar.setProgress(newProgress);
            } else {
                progressBar.setVisibility(View.INVISIBLE);
            }
        }
    }
}
