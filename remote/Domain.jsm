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

  get content() {
    return this.session.content;
  }

  get docShell() {
    return this.session.docShell;
  }

  get chromeEventHandler() {
    return this.docShell.chromeEventHandler;
  }
}

XPCOMUtils.defineLazyModuleGetters(Domain, {
  Log: "chrome://remote/content/domain/Log.jsm",
  Page: "chrome://remote/content/domain/Page.jsm",
});
