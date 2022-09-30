/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class LogModule extends Module {
  destroy() {}

  static get supportedEvents() {
    return ["log.entryAdded"];
  }
}

export const log = LogModule;
