/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorParent } from "resource://gre/modules/GeckoViewActorParent.sys.mjs";

export class GeckoViewExperimentDelegateParent extends GeckoViewActorParent {
  constructor() {
    super();
  }

  /**
   * Gets experiment information on a given feature.
   *
   * @param feature the experiment item to retrieve information on
   * @returns a promise of success with a JSON message or failure
   */
  async getExperimentFeature(feature) {
    return this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:GetExperimentFeature",
      feature,
    });
  }

  /**
   * Records an exposure event, that the experiment area was encountered, on a given feature.
   *
   * @param feature the experiment item to record an exposure event of
   * @returns a promise of success or failure
   */
  async recordExposure(feature) {
    return this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:RecordExposure",
      feature,
    });
  }

  /**
   * Records an exposure event on a specific experiment feature and element.
   *
   * Note: Use recordExposure, if the slug is not known.
   *
   * @param feature the experiment item to record an exposure event of
   * @param slug a specific experiment element
   * @returns a promise of success or failure
   */
  async recordExperimentExposure(feature, slug) {
    return this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:RecordExperimentExposure",
      feature,
      slug,
    });
  }

  /**
   * For recording malformed configuration.
   *
   * @param feature the experiment item to record an exposure event of
   * @param part malformed information to send
   * @returns a promise of success or failure
   */
  async recordExperimentMalformedConfig(feature, part) {
    return this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:RecordMalformedConfig",
      feature,
      part,
    });
  }
}
