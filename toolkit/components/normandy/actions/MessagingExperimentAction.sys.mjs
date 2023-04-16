/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseStudyAction } from "resource://normandy/actions/BaseStudyAction.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ActionSchemas: "resource://normandy/actions/schemas/index.sys.mjs",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
});

const RECIPE_SOURCE = "normandy";

export class MessagingExperimentAction extends BaseStudyAction {
  constructor() {
    super();
    this.manager = lazy.ExperimentManager;
  }
  get schema() {
    return lazy.ActionSchemas["messaging-experiment"];
  }

  async _run(recipe) {
    if (recipe.arguments) {
      await this.manager.onRecipe(recipe.arguments, RECIPE_SOURCE);
    }
  }

  async _finalize() {
    this.manager.onFinalize(RECIPE_SOURCE);
  }
}
