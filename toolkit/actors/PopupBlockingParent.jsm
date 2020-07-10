/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PopupBlocker", "PopupBlockingParent"];

/**
 * This class manages all popup blocking operations on a <xul:browser>, including
 * notifying the UI about updates to the blocked popups, and allowing popups to
 * be unblocked.
 */
class PopupBlocker {
  constructor(browser) {
    this._browser = browser;
    this._allBlockedPopupCounts = new WeakMap();
    this._shouldShowNotification = false;
  }

  /**
   * Returns whether or not there are new blocked popups for the associated
   * <xul:browser> that the user might need to be notified about.
   */
  get shouldShowNotification() {
    return this._shouldShowNotification;
  }

  /**
   * Should be called by the UI when the user has been notified about blocked
   * popups for the associated <xul:browser>.
   */
  didShowNotification() {
    this._shouldShowNotification = false;
  }

  /**
   * Synchronously returns the most recent count of blocked popups for
   * the associated <xul:browser>.
   *
   * @return {Number}
   *   The total number of blocked popups for this <xul:browser>.
   */
  getBlockedPopupCount() {
    let totalBlockedPopups = 0;

    let contextsToVisit = [this._browser.browsingContext];
    while (contextsToVisit.length) {
      let currentBC = contextsToVisit.pop();
      let windowGlobal = currentBC.currentWindowGlobal;

      if (!windowGlobal) {
        continue;
      }

      let popupCountForGlobal =
        this._allBlockedPopupCounts.get(windowGlobal) || 0;
      totalBlockedPopups += popupCountForGlobal;
      contextsToVisit.push(...currentBC.children);
    }

    return totalBlockedPopups;
  }

  /**
   * Asynchronously retrieve information about the popups that have
   * been blocked for the associated <xul:browser>. This information
   * can be used to unblock those popups.
   *
   * @return {Promise}
   * @resolves {Array}
   *   When the blocked popup information has been gathered,
   *   resolves with an Array of Objects with the following properties:
   *
   *   browsingContext {BrowsingContext}
   *     The BrowsingContext that the popup was blocked for.
   *
   *   innerWindowId {Number}
   *     The inner window ID for the blocked popup. This is used to differentiate
   *     popups that were blocked from one page load to the next.
   *
   *   popupWindowURISpec {String}
   *     A string representing part or all of the URI that tried to be opened in a
   *     popup.
   */
  async getBlockedPopups() {
    let contextsToVisit = [this._browser.browsingContext];
    let result = [];
    while (contextsToVisit.length) {
      let currentBC = contextsToVisit.pop();
      let windowGlobal = currentBC.currentWindowGlobal;

      if (!windowGlobal) {
        continue;
      }

      let popupCountForGlobal =
        this._allBlockedPopupCounts.get(windowGlobal) || 0;
      if (popupCountForGlobal) {
        let actor = windowGlobal.getActor("PopupBlocking");
        let popups = await actor.sendQuery("GetBlockedPopupList");

        for (let popup of popups) {
          if (!popup.popupWindowURISpec) {
            continue;
          }

          result.push({
            browsingContext: currentBC,
            innerWindowId: windowGlobal.innerWindowId,
            popupWindowURISpec: popup.popupWindowURISpec,
          });
        }
      }

      contextsToVisit.push(...currentBC.children);
    }

    return result;
  }

  /**
   * Unblocks a popup that had been blocked. The information passed should
   * come from the list of blocked popups returned via getBlockedPopups().
   *
   * Unblocking a popup causes that popup to open.
   *
   * @param browsingContext {BrowsingContext}
   *   The BrowsingContext that the popup was blocked for.
   *
   * @param innerWindowId {Number}
   *   The inner window ID for the blocked popup. This is used to differentiate popups
   *   that were blocked from one page load to the next.
   *
   * @param popupIndex {Number}
   *   The index of the entry in the Array returned by getBlockedPopups().
   */
  unblockPopup(browsingContext, innerWindowId, popupIndex) {
    let popupFrame = browsingContext.top.embedderElement;
    let popupBrowser = popupFrame.outerBrowser
      ? popupFrame.outerBrowser
      : popupFrame;

    if (this._browser != popupBrowser) {
      throw new Error(
        "Attempting to unblock popup in a BrowsingContext no longer hosted in this browser."
      );
    }

    let windowGlobal = browsingContext.currentWindowGlobal;

    if (!windowGlobal || windowGlobal.innerWindowId != innerWindowId) {
      // The inner window has moved on since the user clicked on
      // the blocked popups dropdown, so we'll just exit silently.
      return;
    }

    let actor = browsingContext.currentWindowGlobal.getActor("PopupBlocking");
    actor.sendAsyncMessage("UnblockPopup", { index: popupIndex });
  }

  /**
   * Goes through the most recent list of blocked popups for the associated
   * <xul:browser> and unblocks all of them. Unblocking a popup causes the popup
   * to open.
   */
  async unblockAllPopups() {
    let popups = await this.getBlockedPopups();
    for (let i = 0; i < popups.length; ++i) {
      let popup = popups[i];
      this.unblockPopup(popup.browsingContext, popup.innerWindowId, i);
    }
  }

  /**
   * Fires a DOMUpdateBlockedPopups chrome-only event so that the UI can
   * update itself to represent the current state of popup blocking for
   * the associated <xul:browser>.
   */
  updateBlockedPopupsUI() {
    let event = this._browser.ownerDocument.createEvent("Events");
    event.initEvent("DOMUpdateBlockedPopups", true, true);
    this._browser.dispatchEvent(event);
  }

  /** Private methods **/

  /**
   * Updates the current popup count for a particular BrowsingContext based
   * on messages from the underlying process.
   *
   * This should only be called by a PopupBlockingParent instance.
   *
   * @param browsingContext {BrowsingContext}
   *   The BrowsingContext to update the internal blocked popup count for.
   *
   * @param blockedPopupData {Object}
   *   An Object representing information about how many popups are blocked
   *   for the BrowsingContext. The Object has the following properties:
   *
   *   count {Number}
   *     The total number of blocked popups for the BrowsingContext.
   *
   *   shouldNotify {Boolean}
   *     Whether or not the list of blocked popups has changed in such a way that
   *     the UI should be updated about it.
   */
  _updateBlockedPopupEntries(browsingContext, blockedPopupData) {
    let windowGlobal = browsingContext.currentWindowGlobal;
    let { count, shouldNotify } = blockedPopupData;

    if (!this.shouldShowNotification && shouldNotify) {
      this._shouldShowNotification = true;
    }

    if (windowGlobal) {
      this._allBlockedPopupCounts.set(windowGlobal, count);
    }

    this.updateBlockedPopupsUI();
  }
}

/**
 * To keep things properly encapsulated, these should only be instantiated via
 * the PopupBlocker class for a particular <xul:browser>.
 *
 * Instantiated for a WindowGlobalParent for a BrowsingContext in one of two cases:
 *
 *   1. One or more popups have been blocked for the underlying frame represented
 *      by the WindowGlobalParent.
 *
 *   2. Something in the parent process is querying a frame for information about
 *      any popups that may have been blocked inside of it.
 */
class PopupBlockingParent extends JSWindowActorParent {
  didDestroy() {
    this.updatePopupCountForBrowser({ count: 0, shouldNotify: false });
  }

  receiveMessage(message) {
    if (message.name == "UpdateBlockedPopups") {
      this.updatePopupCountForBrowser({
        count: message.data.count,
        shouldNotify: message.data.shouldNotify,
      });
    }
  }

  /**
   * Updates the PopupBlocker for the <xul:browser> associated with this
   * PopupBlockingParent with the most recent count of blocked popups.
   *
   * @param data {Object}
   *   An Object with the following properties:
   *
   *     count {Number}:
   *       The number of blocked popups for the underlying document.
   *
   *     shouldNotify {Boolean}:
   *       Whether or not the list of blocked popups has changed in such a way that
   *       the UI should be updated about it.
   */
  updatePopupCountForBrowser(data) {
    let browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }

    browser.popupBlocker._updateBlockedPopupEntries(this.browsingContext, data);
  }
}
