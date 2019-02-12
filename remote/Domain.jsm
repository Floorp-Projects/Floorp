/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Domain"];

const {EventEmitter} = ChromeUtils.import("chrome://remote/content/EventEmitter.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

class Domain {
  constructor(session, target) {
    this.session = session;
    this.target = target;
    this.name = this.constructor.name;

    EventEmitter.decorate(this);
  }

  destructor() {}

  get browser() {
    return this.target.browser;
  }

  get mm() {
    return this.browser.mm;
  }
};

XPCOMUtils.defineLazyModuleGetters(Domain, {
  Log: "chrome://remote/content/domain/Log.jsm",
  Network: "chrome://remote/content/domain/Network.jsm",
  Page: "chrome://remote/content/domain/Page.jsm",
  Runtime: "chrome://remote/content/domain/Runtime.jsm",
});
