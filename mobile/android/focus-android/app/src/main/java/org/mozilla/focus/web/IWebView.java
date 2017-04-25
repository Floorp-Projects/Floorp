/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.os.Bundle;

public interface IWebView {
    interface Callback {
        void onPageStarted(String url);

        void onPageFinished(boolean isSecure);
        void onProgress(int progress);

        void onURLChanged(final String url);

        /** Return true if the URL was handled, false if we should continue loading the current URL. */
        boolean handleExternalUrl(String url);
        void onLinkLongPress(String url);

        void onDownloadStart(Download download);
    }

    void setCallback(Callback callback);

    void onPause();

    void onResume();

    void destroy();

    void reload();

    void stopLoading();

    String getUrl();

    void loadUrl(String url);

    void cleanup();

    void goForward();

    void goBack();

    boolean canGoForward();

    boolean canGoBack();

    void restoreWebviewState(Bundle savedInstanceState);

    void onSaveInstanceState(Bundle outState);
}