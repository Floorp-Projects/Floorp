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

  // Start of nsIObserver implementation.

  onEvent: function(event, data, callback) {
    switch (event) {
      case "FindInPage:Opened": {
        this._findOpened();
        break;
      }

      case "FindInPage:Closed": {
        this._uninit();
        this._findClosed();
        break;
      }

      case "Tab:Selected": {
        // Allow for page switching.
        this._uninit();
        break;
      }

      case "FindInPage:Find": {
        this.doFind(data.searchString);
        break;
      }

      case "FindInPage:Next": {
        this.findAgain(data.searchString, false);
        break;
      }

      case "FindInPage:Prev": {
        this.findAgain(data.searchString, true);
        break;
      }
    }
  },

  /**
   * When the FindInPageBar opens/ becomes visible, it's time to:
   * 1. Add listeners for other message types sent from the FindInPageBar
   * 2. initialize the Finder instance, if necessary.
   */
  _findOpened: function() {
    GlobalEventDispatcher.registerListener(this, [
      "FindInPage:Find",
      "FindInPage:Next",
      "FindInPage:Prev",
    ]);

    // Initialize the finder component for the current page by performing a fake find.
    this._init();
    this._finder.requestMatchesCount("");
  },

  /**
   * Fetch the Finder instance from the active tabs' browser and start tracking
   * the active viewport.
   */
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

    WindowEventDispatcher.registerListener(this, [
      "Tab:Selected",
    ]);
  },

  /**
   * Detach from the Finder instance (so stop listening for messages) and stop
   * tracking the active viewport.
   */
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

    WindowEventDispatcher.unregisterListener(this, [
      "Tab:Selected",
    ]);
  },

  /**
   * When the FindInPageBar closes, it's time to stop listening for its messages.
   */
  _findClosed: function() {
    GlobalEventDispatcher.unregisterListener(this, [
      "FindInPage:Find",
      "FindInPage:Next",
      "FindInPage:Prev",
    ]);
  },

  /**
   * Start an asynchronous find-in-page operation, using the current Finder
   * instance and request to count the amount of matches.
   * If no Finder instance is currently active, we'll lazily initialize it here.
   *
   * @param  {String} searchString Word to search for in the current document
   * @return {Object}              Echo of the current find action
   */
  doFind: function(searchString) {
    if (!this._finder) {
      this._init();
    }

    this._finder.fastFind(searchString, false);
    return { searchString, findBackwards: false };
  },

  /**
   * Restart the same find-in-page operation as before via `doFind()`. If we
   * haven't called `doFind()`, we simply kick off a regular find.
   *
   * @param  {String}  searchString  Word to search for in the current document
   * @param  {Boolean} findBackwards Direction to search in
   * @return {Object}                Echo of the current find action
   */
  findAgain: function(searchString, findBackwards) {
    // This always happens if the user taps next/previous after re-opening the
    // search bar, and not only forces _init() but also an initial fastFind(STRING)
    // before any findAgain(DIRECTION).
    if (!this._finder) {
      return this.doFind(searchString);
    }

    this._finder.findAgain(findBackwards, false, false);
    return { searchString, findBackwards };
  },

  // Start of Finder.jsm listener implementation.

  /**
   * Pass along the count results to FindInPageBar for display. The result that
   * is sent to the FindInPageBar is augmented with the current find-in-page count
   * limit.
   *
   * @param {Object} result Result coming from the Finder instance that contains
   *                        the following properties:
   *                        - {Number} total   The total amount of matches found
   *                        - {Number} current The index of current found range
   *                                           in the document
   */
  onMatchesCountResult: function(result) {
    this._result = result;

    GlobalEventDispatcher.sendRequest(Object.assign({
      type: "FindInPage:MatchesCountResult"
    }, this._result));
  },

  /**
   * When a find-in-page action finishes, this method is invoked. This is mainly
   * used at the moment to detect if the current viewport has changed, which might
   * be indicated by not finding a string in the current page.
   *
   * @param {Object} aData A dictionary, representing the find result, which
   *                       contains the following properties:
   *                       - {String}  searchString  Word that was searched for
   *                                                 in the current document
   *                       - {Number}  result        One of the following
   *                                                 Ci.nsITypeAheadFind.* result
   *                                                 indicators: FIND_FOUND,
   *                                                 FIND_NOTFOUND, FIND_WRAPPED,
   *                                                 FIND_PENDING
   *                       - {Boolean} findBackwards Whether the search direction
   *                                                 was backwards
   *                       - {Boolean} findAgain     Whether the previous search
   *                                                 was repeated
   *                       - {Boolean} drawOutline   Whether we may (re-)draw the
   *                                                 outline of a hyperlink
   *                       - {Boolean} linksOnly     Whether links-only mode was
   *                                                 active
   */
  onFindResult: function(aData) {
    if (aData.result == Ci.nsITypeAheadFind.FIND_NOTFOUND) {
      if (this._viewportChanged) {
        if (this._targetTab != BrowserApp.selectedTab) {
          // this should never happen
          Cu.reportError("Warning: selected tab changed during find!");
          // fall through and restore viewport on the initial tab anyway
        }
        this._targetTab.sendViewportUpdate();
      }
    } else {
      // Disabled until bug 1014113 is fixed
      // ZoomHelper.zoomToRect(aData.rect);
      this._viewportChanged = true;
    }
  }
};
