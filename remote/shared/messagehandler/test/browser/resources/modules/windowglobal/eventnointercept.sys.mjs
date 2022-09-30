/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class EventNoInterceptModule extends Module {
  destroy() {}

  testEvent() {
    const text = `event no interception`;
    this.emitEvent("eventnointercept.testEvent", { text });
  }
}

export const eventnointercept = EventNoInterceptModule;
