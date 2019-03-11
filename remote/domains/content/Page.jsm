/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Page"];

const {ContentProcessDomain} = ChromeUtils.import("chrome://remote/content/domains/ContentProcessDomain.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {UnsupportedError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

class Page extends ContentProcessDomain {
  constructor(session) {
    super(session);
    this.enabled = false;
  }

  destructor() {
    this.disable();
  }

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIWebProgressListener) ||
      iid.equals(Ci.nsISupportsWeakReference) ||
      iid.equals(Ci.nsIObserver)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
      this.chromeEventHandler.addEventListener("DOMContentLoaded", this,
        {mozSystemGroup: true});
      this.chromeEventHandler.addEventListener("pageshow", this,
        {mozSystemGroup: true});
    }
  }

  disable() {
    if (this.enabled) {
      this.chromeEventHandler.removeEventListener("DOMContentLoaded", this,
        {mozSystemGroup: true});
      this.chromeEventHandler.removeEventListener("pageshow", this,
        {mozSystemGroup: true});
      this.enabled = false;
    }
  }

  async navigate({url, referrer, transitionType, frameId} = {}) {
    if (frameId) {
      throw new UnsupportedError("frameId not supported");
    }

    const opts = {
      loadFlags: transitionToLoadFlag(transitionType),
      referrerURI: referrer,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    };
    this.docShell.QueryInterface(Ci.nsIWebNavigation);
    this.docShell.loadURI(url, opts);

    return {frameId: "42"};
  }

  getFrameTree() {
    return {
      frameTree: {
        frame: {
          // id, parentId
        },
        childFrames: [],
      },
    };
  }

  setLifecycleEventsEnabled() {}
  addScriptToEvaluateOnNewDocument() {}
  createIsolatedWorld() {}

  url() {
    return this.content.location.href;
  }

  handleEvent({type}) {
    const timestamp = Date.now();

    switch (type) {
    case "DOMContentLoaded":
      this.emit("Page.domContentEventFired", {timestamp});
      break;

    case "pageshow":
      this.emit("Page.loadEventFired", {timestamp});
      // XXX this should most likely be sent differently
      this.emit("Page.navigatedWithinDocument", {timestamp});
      this.emit("Page.frameStoppedLoading", {timestamp});
      break;
    }
  }
}

function transitionToLoadFlag(transitionType) {
  switch (transitionType) {
  case "reload":
    return Ci.nsIWebNavigation.LOAD_FLAG_IS_REFRESH;
  case "link":
  default:
    return Ci.nsIWebNavigation.LOAD_FLAG_IS_LINK;
  }
}
