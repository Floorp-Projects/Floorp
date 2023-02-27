/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

const FEATURE_ID = "legacyHeartbeat";

/**
 * A bridge between Nimbus and Normandy's Heartbeat implementation.
 */
export const LegacyHeartbeat = {
  getHeartbeatRecipe() {
    const survey = lazy.NimbusFeatures.legacyHeartbeat.getVariable("survey");

    if (typeof survey == "undefined") {
      return null;
    }

    let isRollout = false;
    let enrollmentData = lazy.ExperimentAPI.getExperimentMetaData({
      featureId: FEATURE_ID,
    });

    if (!enrollmentData) {
      enrollmentData = lazy.ExperimentAPI.getRolloutMetaData({
        featureId: FEATURE_ID,
      });
      isRollout = true;
    }

    return {
      id: `nimbus:${enrollmentData.slug}`,
      name: `Nimbus legacyHeartbeat ${isRollout ? "rollout" : "experiment"} ${
        enrollmentData.slug
      }`,
      action: "show-heartbeat",
      arguments: survey,
      capabilities: ["action.show-heartbeat"],
      filter_expression: "true",
      use_only_baseline_capabilities: true,
    };
  },
};
