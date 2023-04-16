/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "ActionSchemas",
  "resource://normandy/actions/schemas/index.js"
);

export class ConsoleLogAction extends BaseAction {
  get schema() {
    return lazy.ActionSchemas["console-log"];
  }

  async _run(recipe) {
    this.log.info(recipe.arguments.message);
  }
}
