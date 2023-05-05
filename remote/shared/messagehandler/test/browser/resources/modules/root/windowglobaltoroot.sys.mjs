/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class WindowGlobalToRootModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  getValueFromRoot() {
    this.#assertParentProcess();
    return "root-value-called-from-windowglobal";
  }

  #assertParentProcess() {
    const isParent =
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT;

    if (!isParent) {
      throw new Error("Can only run in the parent process");
    }
  }
}

export const windowglobaltoroot = WindowGlobalToRootModule;
