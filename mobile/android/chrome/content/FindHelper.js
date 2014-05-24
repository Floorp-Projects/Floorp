/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var FindHelper = {
  _finder: null,
  _targetTab: null,
  _initialViewport: null,
  _viewportChanged: false,

  observe: function(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "FindInPage:Find":
        this.doFind(aData);
        break;

      case "FindInPage:Prev":
        this.findAgain(aData, true);
        break;

      case "FindInPage:Next":
        this.findAgain(aData, false);
        break;

      case "Tab:Selected":
      case "FindInPage:Closed":
        this.findClosed();
        break;
    }
  },

  doFind: function(aSearchString) {
    if (!this._finder) {
      this._targetTab = BrowserApp.selectedTab;
      this._finder = this._targetTab.browser.finder;
      this._finder.addResultListener(this);
      this._initialViewport = JSON.stringify(this._targetTab.getViewport());
      this._viewportChanged = false;
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

  findClosed: function() {
    // If there's no find in progress, there's nothing to clean up
    if (!this._finder)
      return;

    this._finder.removeSelection();
    this._finder.removeResultListener(this);
    this._finder = null;
    this._targetTab = null;
    this._initialViewport = null;
    this._viewportChanged = false;
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
      //ZoomHelper.zoomToRect(aData.rect, -1, false, true);
      this._viewportChanged = true;
    }
  }
};
