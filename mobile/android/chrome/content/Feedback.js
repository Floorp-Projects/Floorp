/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Feedback = {

  get _feedbackURL() {
    delete this._feedbackURL;
    return this._feedbackURL = Services.urlFormatter.formatURLPref("app.feedbackURL");
  },

  observe: function(aMessage, aTopic, aData) {
    if (aTopic !== "Feedback:Show") {
      return;
    }

    // Don't prompt for feedback in distribution builds.
    try {
      Services.prefs.getCharPref("distribution.id");
      return;
    } catch (e) {}

    let url = this._feedbackURL;
    let browser = BrowserApp.selectOrAddTab(url, { parentId: BrowserApp.selectedTab.id }).browser;

    browser.addEventListener("FeedbackClose", this, false, true);
    browser.addEventListener("FeedbackMaybeLater", this, false, true);

    // Dispatch a custom event to the page content when feedback is prompted by the browser.
    // This will be used by the page to determine it's being loaded directly by the browser,
    // instead of by the user visiting the page, e.g. through browser history.
    function loadListener(event) {
      browser.removeEventListener("DOMContentLoaded", loadListener, false);
      browser.contentDocument.dispatchEvent(new CustomEvent("FeedbackPrompted"));
    }
    browser.addEventListener("DOMContentLoaded", loadListener, false);
  },

  handleEvent: function(event) {
    if (!this._isAllowed(event.target)) {
      return;
    }

    switch (event.type) {
      case "FeedbackClose":
        // Do nothing.
        break;

      case "FeedbackMaybeLater":
        GlobalEventDispatcher.sendRequest({ type: "Feedback:MaybeLater" });
        break;
    }

    let win = event.target.ownerDocument.defaultView.top;
    BrowserApp.closeTab(BrowserApp.getTabForWindow(win));
  },

  _isAllowed: function(node) {
    let uri = node.ownerDocument.documentURIObject;
    let feedbackURI = Services.io.newURI(this._feedbackURL);
    return uri.prePath === feedbackURI.prePath;
  }
};
