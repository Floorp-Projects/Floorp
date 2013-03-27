/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var FindHelper = {
  _fastFind: null,
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
    if (!this._fastFind) {
      this._targetTab = BrowserApp.selectedTab;
      this._fastFind = this._targetTab.browser.fastFind;
      this._initialViewport = JSON.stringify(this._targetTab.getViewport());
      this._viewportChanged = false;
    }

    let result = this._fastFind.find(aSearchString, false);
    this.handleResult(result);
  },

  findAgain: function(aString, aFindBackwards) {
    // This can happen if the user taps next/previous after re-opening the search bar
    if (!this._fastFind) {
      this.doFind(aString);
      return;
    }

    let result = this._fastFind.findAgain(aFindBackwards, false);
    this.handleResult(result);
  },

  findClosed: function() {
    // If there's no find in progress, there's nothing to clean up
    if (!this._fastFind)
      return;

    this._fastFind.collapseSelection();
    this._fastFind = null;
    this._targetTab = null;
    this._initialViewport = null;
    this._viewportChanged = false;
  },

  handleResult: function(aResult) {
    if (aResult == Ci.nsITypeAheadFind.FIND_NOTFOUND) {
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
      this._viewportChanged = true;
    }
  }
};
