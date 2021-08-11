/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["commandwindowglobalonly"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class CommandWindowGlobalOnly extends Module {
  destroy() {}

  /**
   * Commands
   */

  testOnlyInWindowGlobal() {
    return "only-in-windowglobal";
  }

  testBroadcast() {
    return `broadcast-${this.messageHandler.contextId}`;
  }

  testBroadcastWithParameter(params) {
    return `broadcast-${this.messageHandler.contextId}-${params.value}`;
  }

  testError() {
    throw new Error("error-from-module");
  }
}

const commandwindowglobalonly = CommandWindowGlobalOnly;
