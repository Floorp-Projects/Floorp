/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["log"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class LogModule extends Module {
  destroy() {}

  /**
   * Internal commands
   */

  _subscribeEvent(params) {
    return this.addEventSessionData("log", params.event);
  }

  _unsubscribeEvent(params) {
    return this.removeEventSessionData("log", params.event);
  }

  static get supportedEvents() {
    return ["log.entryAdded"];
  }
}

const log = LogModule;
