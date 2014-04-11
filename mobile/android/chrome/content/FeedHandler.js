/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var FeedHandler = {
  PREF_CONTENTHANDLERS_BRANCH: "browser.contentHandlers.types.",
  TYPE_MAYBE_FEED: "application/vnd.mozilla.maybe.feed",

  _contentTypes: null,

  getContentHandlers: function fh_getContentHandlers(contentType) {
    if (!this._contentTypes)
      this.loadContentHandlers();

    if (!(contentType in this._contentTypes))
      return [];

    return this._contentTypes[contentType];
  },
  
  loadContentHandlers: function fh_loadContentHandlers() {
    this._contentTypes = {};

    let kids = Services.prefs.getBranch(this.PREF_CONTENTHANDLERS_BRANCH).getChildList("");

    // First get the numbers of the providers by getting all ###.uri prefs
    let nums = [];
    for (let i = 0; i < kids.length; i++) {
      let match = /^(\d+)\.uri$/.exec(kids[i]);
      if (!match)
        continue;
      else
        nums.push(match[1]);
    }

    // Sort them, to get them back in order
    nums.sort(function(a, b) { return a - b; });

    // Now register them
    for (let i = 0; i < nums.length; i++) {
      let branch = Services.prefs.getBranch(this.PREF_CONTENTHANDLERS_BRANCH + nums[i] + ".");
      let vals = branch.getChildList("");
      if (vals.length == 0)
        return;

      try {
        let type = branch.getCharPref("type");
        let uri = branch.getComplexValue("uri", Ci.nsIPrefLocalizedString).data;
        let title = branch.getComplexValue("title", Ci.nsIPrefLocalizedString).data;

        if (!(type in this._contentTypes))
          this._contentTypes[type] = [];
        this._contentTypes[type].push({ contentType: type, uri: uri, name: title });
      }
      catch(ex) {}
    }
  },

  observe: function fh_observe(aSubject, aTopic, aData) {
    if (aTopic === "Feeds:Subscribe") {
      let args = JSON.parse(aData);
      let tab = BrowserApp.getTabForId(args.tabId);
      if (!tab)
        return;

      let browser = tab.browser;
      let feeds = browser.feeds;
      if (feeds == null)
        return;

      // First, let's decide on which feed to subscribe
      let feedIndex = -1;
      if (feeds.length > 1) {
        let p = new Prompt({
          window: browser.contentWindow,
          title: Strings.browser.GetStringFromName("feedHandler.chooseFeed")
        }).setSingleChoiceItems(feeds.map(function(feed) {
          return { label: feed.title || feed.href }
        })).show((function(data) {
          feedIndex = data.button;
          if (feedIndex == -1)
            return;

          this.loadFeed(feeds[feedIndex], browser);
        }).bind(this));
        return;
      }

      this.loadFeed(feeds[0], browser);
    }
  },

  loadFeed: function fh_loadFeed(aFeed, aBrowser) {
    let feedURL = aFeed.href;

    // Next, we decide on which service to send the feed
    let handlers = this.getContentHandlers(this.TYPE_MAYBE_FEED);
    if (handlers.length == 0)
      return;

    // JSON for Prompt
    let p = new Prompt({
      window: aBrowser.contentWindow,
      title: Strings.browser.GetStringFromName("feedHandler.subscribeWith")
    }).setSingleChoiceItems(handlers.map(function(handler) {
      return { label: handler.name };
    })).show(function(data) {
      if (data.button == -1)
        return;

      // Merge the handler URL and the feed URL
      let readerURL = handlers[data.button].uri;
      readerURL = readerURL.replace(/%s/gi, encodeURIComponent(feedURL));

      // Open the resultant URL in a new tab
      BrowserApp.addTab(readerURL, { parentId: BrowserApp.selectedTab.id });
    });
  }
};
