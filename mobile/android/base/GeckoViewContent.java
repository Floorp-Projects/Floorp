/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

public class GeckoViewContent implements GeckoView.ContentDelegate {
    /**
    * A Browser has started loading content from the network.
    * @param view The GeckoView that initiated the callback.
    * @param browser The Browser that is loading the content.
    * @param url The resource being loaded.
    */
    public void onPageStart(GeckoView view, GeckoView.Browser browser, String url) {}

    /**
    * A Browser has finished loading content from the network.
    * @param view The GeckoView that initiated the callback.
    * @param browser The Browser that was loading the content.
    * @param success Whether the page loaded successfully or an error occured.
    */
    public void onPageStop(GeckoView view, GeckoView.Browser browser, boolean success) {}

    /**
    * A Browser is displaying content. This page could have been loaded via
    * network or from the session history.
    * @param view The GeckoView that initiated the callback.
    * @param browser The Browser that is showing the content.
    */
    public void onPageShow(GeckoView view, GeckoView.Browser browser) {}

    /**
    * A page title was discovered in the content or updated after the content
    * loaded.
    * @param view The GeckoView that initiated the callback.
    * @param browser The Browser that is showing the content.
    * @param title The title sent from the content.
    */
    public void onReceivedTitle(GeckoView view, GeckoView.Browser browser, String title) {}

    /**
    * A link element was discovered in the content or updated after the content
    * loaded that specifies a favicon.
    * @param view The GeckoView that initiated the callback.
    * @param browser The Browser that is showing the content.
    * @param url The href of the link element specifying the favicon.
    * @param size The maximum size specified for the favicon, or -1 for any size.
    */
    public void onReceivedFavicon(GeckoView view, GeckoView.Browser browser, String url, int size) {}
}
