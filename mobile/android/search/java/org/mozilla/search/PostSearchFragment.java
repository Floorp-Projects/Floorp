/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class PostSearchFragment extends Fragment {

    private static final String LOGTAG = "PostSearchFragment";
    private WebView webview;

    private static String HIDE_BANNER_SCRIPT = "javascript:(function(){var tag=document.createElement('style');" +
            "tag.type='text/css';document.getElementsByTagName('head')[0].appendChild(tag);tag.innerText='#nav,#header{display:none}'})();";

    public PostSearchFragment() {
    }


    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        final View mainView = inflater.inflate(R.layout.search_activity_detail, container, false);

        webview = (WebView) mainView.findViewById(R.id.webview);
        webview.setWebViewClient(new LinkInterceptingClient());
        webview.setWebChromeClient(new StyleInjectingClient());
        webview.getSettings().setJavaScriptEnabled(true);

        return mainView;
    }

    /**
     * Test if a given URL is a page of search results.
     * <p>
     * Search results pages will be shown in the embedded view.  Other pages are
     * opened in external browsers.
     *
     * @param url to test.
     * @return true if <code>url</code> is a page of search results.
     */
    protected boolean isSearchResultsPage(String url) {
        return url.contains(Constants.YAHOO_WEB_SEARCH_RESULTS_FILTER);
    }

    public void startSearch(String query) {
        setUrl(Constants.YAHOO_WEB_SEARCH_BASE_URL + Uri.encode(query));
    }

    public void setUrl(String url) {
        webview.loadUrl(url);
    }

    /**
     * A custom WebViewClient that intercepts every page load. This allows
     * us to decide whether to load the url here, or send it to Android
     * as an intent.
     */
    private class LinkInterceptingClient extends WebViewClient {

        @Override
        public void onPageStarted(WebView view, String url, Bitmap favicon) {
            if (isSearchResultsPage(url)) {
                super.onPageStarted(view, url, favicon);
            } else {
                view.stopLoading();
                Intent i = new Intent(Intent.ACTION_VIEW);
                i.setData(Uri.parse(url));
                startActivity(i);
            }
        }
    }

    /**
     * A custom WebChromeClient that allows us to inject CSS into
     * the head of the HTML.
     *
     * We use the WebChromeClient because it provides a hook to the titleReceived
     * event. Once the title is available, the page will have started parsing the
     * head element. The script injects its CSS into the head element.
     */
    private class StyleInjectingClient extends WebChromeClient {

        @Override
        public void onReceivedTitle(WebView view, String title) {
            super.onReceivedTitle(view, title);
            view.loadUrl(HIDE_BANNER_SCRIPT);
        }
    }
}
