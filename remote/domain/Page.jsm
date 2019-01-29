/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Page"];

const {Domain} = ChromeUtils.import("chrome://remote/content/Domain.jsm");
const {t} = ChromeUtils.import("chrome://remote/content/Protocol.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {UnsupportedError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

this.Page = class extends Domain {
  constructor(session, target) {
    super(session, target);
    this.enabled = false;
  }

  destructor() {
    this.disable();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
      Services.mm.addMessageListener("RemoteAgent:DOM:OnEvent", this);
    }
  }

  disable() {
    if (this.enabled) {
      Services.mm.removeMessageListener("RemoteAgent:DOM:OnEvent", this);
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
      triggeringPrincipal: this.browser.contentPrincipal,
    };
    this.browser.webNavigation.loadURI(url, opts);

    return {frameId: "42"};
  }

  url() {
    return this.browsrer.currentURI.spec;
  }

  onDOMEvent({type}) {
    const timestamp = Date.now();

    switch (type) {
    case "DOMContentLoaded":
      this.emit("Page.domContentEventFired", {timestamp});
      break;

    case "pageshow":
      this.emit("Page.loadEventFired", {timestamp});
      break;
    }
  }

  // nsIMessageListener

  receiveMessage({target, name, data}) {
    if (target !== this.target.browser) {
      return;
    }

    switch (name) {
    case "RemoteAgent:DOM:OnEvent":
      this.onDOMEvent(data);
      break;
    }
  }

  static get schema() {
    return {
      methods: {
        enable: {},
        disable: {},
        navigate: {
          params: {
            url: t.String,
            referrer: t.Optional(t.String),
            transitionType: t.Optional(Page.TransitionType.schema),
            frameId: t.Optional(Page.FrameId.schema),
          },
          returns: {
            frameId: Page.FrameId,
            loaderId: t.Optional(Domain.Network.LoaderId.schema),
            errorText: t.String,
          },
        },
      },

      events: {
        domContentEventFired: {
          timestamp: Domain.Network.MonotonicTime.schema,
        },
        loadEventFired: {
          timestamp: Domain.Network.MonotonicTime.schema,
        },
      },
    };
  }
};

this.Page.FrameId = {schema: t.String};
this.Page.TransitionType = {
  schema: t.Enum([
    "auto_bookmark",
    "auto_subframe",
    "link",
    "manual_subframe",
    "reload",
    "typed",
  ]),
};

function transitionToLoadFlag(transitionType) {
  switch (transitionType) {
  case "reload":
    return Ci.nsIWebNavigation.LOAD_FLAG_IS_REFRESH;
  case "link":
  default:
    return Ci.nsIWebNavigation.LOAD_FLAG_IS_LINK;
  }
}
