/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

/**
 * Base class for all WindowGlobal BiDi MessageHandler modules.
 */
export class WindowGlobalBiDiModule extends Module {
  get nodeCache() {
    return this.processActor.getNodeCache();
  }

  get processActor() {
    return ChromeUtils.domProcessChild.getActor("WebDriverProcessData");
  }
}
