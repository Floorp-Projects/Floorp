/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["rootOnly"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class RootOnly extends Module {
  destroy() {}

  /**
   * Commands
   */

  testCommand(params = {}) {
    return params;
  }
}

const rootOnly = RootOnly;
