/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseAction } from "resource://normandy/actions/BaseAction.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ActionSchemas: "resource://normandy/actions/schemas/index.sys.mjs",
});

export class ConsoleLogAction extends BaseAction {
  get schema() {
    return lazy.ActionSchemas["console-log"];
  }

  async _run(recipe) {
    this.log.info(recipe.arguments.message);
  }
}
