/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { ActorManagerChild } = ChromeUtils.import(
  "resource://gre/modules/ActorManagerChild.jsm"
);

ActorManagerChild.attach(this);

ChromeUtils.defineModuleGetter(
  this,
  "AutoScrollController",
  "resource://gre/modules/AutoScrollController.jsm"
);

var global = this;

var AutoScrollListener = {
  handleEvent(event) {
    if (event.isTrusted & !event.defaultPrevented && event.button == 1) {
      if (!this._controller) {
        this._controller = new AutoScrollController(global);
      }
      this._controller.handleEvent(event);
    }
  },
};
Services.els.addSystemEventListener(
  global,
  "mousedown",
  AutoScrollListener,
  true
);
