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

    this.onFrameNavigated = this.onFrameNavigated.bind(this);
  }

  destructor() {
    this.disable();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
      this.contextObserver.on("frame-navigated", this.onFrameNavigated);

      this.chromeEventHandler.addEventListener("DOMContentLoaded", this,
        {mozSystemGroup: true});
      this.chromeEventHandler.addEventListener("pageshow", this,
        {mozSystemGroup: true});
    }
  }

  disable() {
    if (this.enabled) {
      this.contextObserver.off("frame-navigated", this.onFrameNavigated);

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
    this.docShell.loadURI(url, opts);

    return {
      frameId: this.content.windowUtils.outerWindowID,
    };
  }

  async reload() {
    this.docShell.reload(Ci.nsIWebNavigation.LOAD_FLAGS_NONE);
  }

  getFrameTree() {
    const frameId = this.content.windowUtils.outerWindowID;
    return {
      frameTree: {
        frame: {
          id: frameId,
          url: this.content.location.href,
          loaderId: null,
          securityOrigin: null,
          mimeType: null,
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

  onFrameNavigated(name, { frameId, window }) {
    const url = window.location.href;
    this.emit("Page.frameNavigated", {
      frame: {
        id: frameId,
        // frameNavigated is only emitted for the top level document
        // so that it never has a parent.
        parentId: null,
        url,
      },
    });
  }

  handleEvent({type, target}) {
    if (target.defaultView != this.content) {
      // Ignore iframes for now
      return;
    }

    const timestamp = Date.now();
    const frameId = target.defaultView.windowUtils.outerWindowID;
    const url = target.location.href;

    switch (type) {
    case "DOMContentLoaded":
      this.emit("Page.domContentEventFired", {timestamp});
      break;

    case "pageshow":
      this.emit("Page.loadEventFired", {timestamp, frameId});
      // XXX this should most likely be sent differently
      this.emit("Page.navigatedWithinDocument", {timestamp, frameId, url});
      this.emit("Page.frameStoppedLoading", {timestamp, frameId});
      break;
    }
  }
}

function transitionToLoadFlag(transitionType) {
  switch (transitionType) {
  case "reload":
    return Ci.nsIWebNavigation.LOAD_FLAGS_IS_REFRESH;
  case "link":
  default:
    return Ci.nsIWebNavigation.LOAD_FLAGS_IS_LINK;
  }
}
