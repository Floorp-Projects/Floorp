/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
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

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.search.providers.SearchEngine;

public class PostSearchFragment extends Fragment {

    private static final String LOG_TAG = "PostSearchFragment";

    private SearchEngine engine;

    private ProgressBar progressBar;
    private WebView webview;
    private View errorView;

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

            // We keep URLs in the webview that are either about:blank or a search engine result page.
            if (TextUtils.equals(url, Constants.ABOUT_BLANK) || engine.isSearchResultsPage(url)) {
                // Keeping the URL in the webview is a noop.
                return;
            }

            webview.stopLoading();

            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL,
                    TelemetryContract.Method.CONTENT, "search-result");

            final Intent i = new Intent(Intent.ACTION_VIEW);

            // This sends the URL directly to fennec, rather than to Android.
            i.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.BROWSER_INTENT_CLASS_NAME);
            i.setData(Uri.parse(url));
            startActivity(i);
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
