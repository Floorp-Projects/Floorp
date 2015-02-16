/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var FindHelper = {
  _finder: null,
  _targetTab: null,
  _initialViewport: null,
  _viewportChanged: false,
  _result: null,
  _limit: 0,

  observe: function(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "FindInPage:Opened": {
        this._findOpened();
        break;
      }

      case "Tab:Selected": {
        // Allow for page switching.
        this._uninit();
        break;
      }

      case "FindInPage:Closed":
        this._uninit();
        this._findClosed();
        break;
    }
  },

  _findOpened: function() {
    try {
      this._limit = Services.prefs.getIntPref("accessibility.typeaheadfind.matchesCountLimit");
    } catch (e) {
      // Pref not available, assume 0, no match counting.
      this._limit = 0;
    }

    Messaging.addListener((data) => {
      this.doFind(data.searchString, data.matchCase);
      return this._getMatchesCountResult(data.searchString);
    }, "FindInPage:Find");

    Messaging.addListener((data) => {
      this.findAgain(data.searchString, false, data.matchCase);
      return this._getMatchesCountResult(data.searchString);
    }, "FindInPage:Next");

    Messaging.addListener((data) => {
      this.findAgain(data.searchString, true, data.matchCase);
      return this._getMatchesCountResult(data.searchString);
    }, "FindInPage:Prev");
  },

  _init: function() {
    // If there's no find in progress, start one.
    if (this._finder) {
      return;
    }

    this._targetTab = BrowserApp.selectedTab;
    try {
      this._finder = this._targetTab.browser.finder;
    } catch (e) {
      throw new Error("FindHelper: " + e + "\n" +
        "JS stack: \n" + (e.stack || Components.stack.formattedStack));
    }

    this._finder.addResultListener(this);
    this._initialViewport = JSON.stringify(this._targetTab.getViewport());
    this._viewportChanged = false;
  },

  _uninit: function() {
    // If there's no find in progress, there's nothing to clean up.
    if (!this._finder) {
      return;
    }

    this._finder.removeSelection();
    this._finder.removeResultListener(this);
    this._finder = null;
    this._targetTab = null;
    this._initialViewport = null;
    this._viewportChanged = false;
  },

  _findClosed: function() {
    Messaging.removeListener("FindInPage:Find");
    Messaging.removeListener("FindInPage:Next");
    Messaging.removeListener("FindInPage:Prev");
  },

  /**
   * Request, wait for, and return the current matchesCount results for a string.
   */
  _getMatchesCountResult: function(findString) {
    // Count matches up to any provided limit.
    if (this._limit <= 0) {
      return { total: 0, current: 0, limit: 0 };
    }

    // Sync call to Finder, results available immediately.
    this._finder.requestMatchesCount(findString, this._limit);
    return this._result;
  },

  /**
   * Pass along the count results to FindInPageBar for display.
   */
  onMatchesCountResult: function(result) {
    this._result = result;
    this._result.limit = this._limit;
  },

  doFind: function(searchString, matchCase) {
    if (!this._finder) {
      this._init();
    }

    this._finder.caseSensitive = matchCase;
    this._finder.fastFind(searchString, false);
  },

  findAgain: function(searchString, findBackwards, matchCase) {
    // This always happens if the user taps next/previous after re-opening the
    // search bar, and not only forces _init() but also an initial fastFind(STRING)
    // before any findAgain(DIRECTION).
    if (!this._finder) {
      this.doFind(searchString, matchCase);
      return;
    }

    this._finder.caseSensitive = matchCase;
    this._finder.findAgain(findBackwards, false, false);
  },

  onFindResult: function(aData) {
    if (aData.result == Ci.nsITypeAheadFind.FIND_NOTFOUND) {
      if (this._viewportChanged) {
        if (this._targetTab != BrowserApp.selectedTab) {
          // this should never happen
          Cu.reportError("Warning: selected tab changed during find!");
          // fall through and restore viewport on the initial tab anyway
        }
        this._targetTab.setViewport(JSON.parse(this._initialViewport));
        this._targetTab.sendViewportUpdate();
      }
    } else {
      // Disabled until bug 1014113 is fixed
      // ZoomHelper.zoomToRect(aData.rect);
      this._viewportChanged = true;
    }
  }
};
