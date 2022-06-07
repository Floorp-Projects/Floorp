/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "UptakeTelemetry",
  "resource://services-common/uptake-telemetry.js"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["Uptake"];

const COMPONENT = "normandy";

var Uptake = {
  // Action uptake
  ACTION_NETWORK_ERROR: lazy.UptakeTelemetry.STATUS.NETWORK_ERROR,
  ACTION_PRE_EXECUTION_ERROR: lazy.UptakeTelemetry.STATUS.CUSTOM_1_ERROR,
  ACTION_POST_EXECUTION_ERROR: lazy.UptakeTelemetry.STATUS.CUSTOM_2_ERROR,
  ACTION_SERVER_ERROR: lazy.UptakeTelemetry.STATUS.SERVER_ERROR,
  ACTION_SUCCESS: lazy.UptakeTelemetry.STATUS.SUCCESS,

  // Per-recipe uptake
  RECIPE_ACTION_DISABLED: lazy.UptakeTelemetry.STATUS.CUSTOM_1_ERROR,
  RECIPE_DIDNT_MATCH_FILTER: lazy.UptakeTelemetry.STATUS.BACKOFF,
  RECIPE_INCOMPATIBLE_CAPABILITIES: lazy.UptakeTelemetry.STATUS.BACKOFF,
  RECIPE_EXECUTION_ERROR: lazy.UptakeTelemetry.STATUS.APPLY_ERROR,
  RECIPE_FILTER_BROKEN: lazy.UptakeTelemetry.STATUS.CONTENT_ERROR,
  RECIPE_ARGUMENTS_INVALID: lazy.UptakeTelemetry.STATUS.CONTENT_ERROR,
  RECIPE_INVALID_ACTION: lazy.UptakeTelemetry.STATUS.DOWNLOAD_ERROR,
  RECIPE_SUCCESS: lazy.UptakeTelemetry.STATUS.SUCCESS,
  RECIPE_INVALID_SIGNATURE: lazy.UptakeTelemetry.STATUS.SIGNATURE_ERROR,

  // Uptake for the runner as a whole
  RUNNER_NETWORK_ERROR: lazy.UptakeTelemetry.STATUS.NETWORK_ERROR,
  RUNNER_SERVER_ERROR: lazy.UptakeTelemetry.STATUS.SERVER_ERROR,
  RUNNER_SUCCESS: lazy.UptakeTelemetry.STATUS.SUCCESS,

  async _report(status, source) {
    // Telemetry doesn't help us much with error detection, so do some here.
    if (!status) {
      throw new Error(
        `Uptake status is required (got "${JSON.stringify(status)}"`
      );
    }
    if (!source) {
      throw new Error(
        `Uptake source is required (got "${JSON.stringify(status)}`
      );
    }
    await lazy.UptakeTelemetry.report(COMPONENT, status, {
      source: `${COMPONENT}/${source}`,
    });
  },

  async reportRunner(status) {
    await Uptake._report(status, "runner");
  },

  async reportRecipe(recipe, status) {
    await Uptake._report(status, `recipe/${recipe.id}`);
    const revisionId = parseInt(recipe.revision_id, 10);
    Services.telemetry.keyedScalarSet(
      "normandy.recipe_freshness",
      recipe.id,
      revisionId
    );
  },

  async reportAction(actionName, status) {
    await Uptake._report(status, `action/${actionName}`);
  },
};
