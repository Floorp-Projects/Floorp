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
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class PostSearchFragment extends Fragment {

    private static final String LOGTAG = "PostSearchFragment";
    private WebView webview;

    public PostSearchFragment() {}


    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View mainView = inflater.inflate(R.layout.search_activity_detail, container, false);


        webview = (WebView) mainView.findViewById(R.id.webview);
        webview.setWebViewClient(new WebViewClient(){
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                if (isSearchResultsPage(url)) {
                    super.onPageStarted(view, url, favicon);
                } else {
                    webview.stopLoading();
                    Intent i = new Intent(Intent.ACTION_VIEW);
                    i.setData(Uri.parse(url));
                    startActivity(i);
                }
            }
        });
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
}
