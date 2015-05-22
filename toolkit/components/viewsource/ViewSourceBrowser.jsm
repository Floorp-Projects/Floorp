// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");

this.EXPORTED_SYMBOLS = ["ViewSourceBrowser"];

/**
 * ViewSourceBrowser manages the view source <browser> from the chrome side.
 * It's companion frame script, viewSource-content.js, needs to be loaded as a
 * frame script into the browser being managed.
 *
 * For a view source window using viewSource.xul, the script viewSource.js in
 * the window extends an instance of this with more window specific functions.
 * The page script takes care of loading the companion frame script.
 *
 * For a view source tab (or some other non-window case), an instance of this is
 * created by viewSourceUtils.js to wrap the <browser>.  The caller that manages
 * the <browser> is responsible for ensuring the companion frame script has been
 * loaded.
 */
this.ViewSourceBrowser = function ViewSourceBrowser(aBrowser) {
  this._browser = aBrowser;
  this.init();
}

ViewSourceBrowser.prototype = {
  /**
   * The <browser> that will be displaying the view source content.
   */
  get browser() {
    return this._browser;
  },

  /**
   * These are the messages that ViewSourceBrowser will listen for
   * from the frame script it injects. Any message names added here
   * will automatically have ViewSourceBrowser listen for those messages,
   * and remove the listeners on teardown.
   */
  // TODO: Some messages will appear here in a later patch
  messages: [
  ],

  /**
   * This should be called as soon as the script loads. When this function
   * executes, we can assume the DOM content has not yet loaded.
   */
  init() {
    this.messages.forEach((msgName) => {
      this.mm.addMessageListener(msgName, this);
    });
  },

  /**
   * This should be called when the window is closing. This function should
   * clean up event and message listeners.
   */
  uninit() {
    this.messages.forEach((msgName) => {
      this.mm.removeMessageListener(msgName, this);
    });
  },

  /**
   * Anything added to the messages array will get handled here, and should
   * get dispatched to a specific function for the message name.
   */
  receiveMessage(message) {
    let data = message.data;

    // TODO: Some messages will appear here in a later patch
    switch(message.name) {
    }
  },

  /**
   * Getter for the message manager of the view source browser.
   */
  get mm() {
    return this.browser.messageManager;
  },

  /**
   * Send a message to the view source browser.
   */
  sendAsyncMessage(...args) {
    this.browser.messageManager.sendAsyncMessage(...args);
  },

  /**
   * Loads the source for a URL while applying some optional features if
   * enabled.
   *
   * For the viewSource.xul window, this is called by onXULLoaded above.
   * For view source in a specific browser, this is manually called after
   * this object is constructed.
   *
   * This takes a single object argument containing:
   *
   *   URL (required):
   *     A string URL for the page we'd like to view the source of.
   *   browser:
   *     The browser containing the document that we would like to view the
   *     source of. This argument is optional if outerWindowID is not passed.
   *   outerWindowID (optional):
   *     The outerWindowID of the content window containing the document that
   *     we want to view the source of. This is the only way of attempting to
   *     load the source out of the network cache.
   *   lineNumber (optional):
   *     The line number to focus on once the source is loaded.
   */
  loadViewSource({ URL, browser, outerWindowID, lineNumber }) {
    if (!URL) {
      throw new Error("Must supply a URL when opening view source.");
    }

    if (browser) {
      // If we're dealing with a remote browser, then the browser
      // for view source needs to be remote as well.
      this.updateBrowserRemoteness(browser.isRemoteBrowser);
    } else {
      if (outerWindowID) {
        throw new Error("Must supply the browser if passing the outerWindowID");
      }
    }

    this.sendAsyncMessage("ViewSource:LoadSource",
                          { URL, outerWindowID, lineNumber });
  },

  /**
   * Updates the "remote" attribute of the view source browser. This
   * will remove the browser from the DOM, and then re-add it in the
   * same place it was taken from.
   *
   * @param shouldBeRemote
   *        True if the browser should be made remote. If the browsers
   *        remoteness already matches this value, this function does
   *        nothing.
   */
  updateBrowserRemoteness(shouldBeRemote) {
    if (this.browser.isRemoteBrowser != shouldBeRemote) {
      // In this base case, where we are handed a <browser> someone else is
      // managing, we don't know for sure that it's safe to toggle remoteness.
      // For view source in a window, this is overridden to actually do the
      // flip if needed.
      throw new Error("View source browser's remoteness mismatch");
    }
  },
};
