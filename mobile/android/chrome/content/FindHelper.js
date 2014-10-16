/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var FindHelper = {
  _finder: null,
  _targetTab: null,
  _initialViewport: null,
  _viewportChanged: false,
  _matchesCountResult: null,

  observe: function(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "FindInPage:Opened": {
        this._findOpened();
        this._init();
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
    Messaging.addListener((data) => {
      this.doFind(data);
      return this._getMatchesCountResult(data);
    }, "FindInPage:Find");

    Messaging.addListener((data) => {
      this.findAgain(data, false);
      return this._getMatchesCountResult(data);
    }, "FindInPage:Next");

    Messaging.addListener((data) => {
      this.findAgain(data, true);
      return this._getMatchesCountResult(data);
    }, "FindInPage:Prev");
  },

  _init: function() {
    // If there's no find in progress, start one.
    if (this._finder) {
      return;
    }

    this._targetTab = BrowserApp.selectedTab;
    this._finder = this._targetTab.browser.finder;
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
      // Sync call to Finder, results available immediately.
      this._matchesCountResult = null;
      this._finder.requestMatchesCount(findString);

      return this._matchesCountResult;
  },

  /**
   * Pass along the count results to FindInPageBar for display.
   */
  onMatchesCountResult: function(result) {
    this._matchesCountResult = result;
  },

  doFind: function(aSearchString) {
    if (!this._finder) {
      this._init();
    }

    this._finder.fastFind(aSearchString, false);
  },

  findAgain: function(aString, aFindBackwards) {
    // This can happen if the user taps next/previous after re-opening the search bar
    if (!this._finder) {
      this.doFind(aString);
      return;
    }

    this._finder.findAgain(aFindBackwards, false, false);
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
