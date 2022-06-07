/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BaseStudyAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseStudyAction.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "ExperimentManager",
  "resource://nimbus/lib/ExperimentManager.jsm"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "ActionSchemas",
  "resource://normandy/actions/schemas/index.js"
);

const EXPORTED_SYMBOLS = ["MessagingExperimentAction"];
const RECIPE_SOURCE = "normandy";

class MessagingExperimentAction extends BaseStudyAction {
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
