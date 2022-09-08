/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class EnterprisePoliciesManagerContent {
  get status() {
    return (
      Services.cpmm.sharedData.get("EnterprisePolicies:Status") ||
      Ci.nsIEnterprisePolicies.INACTIVE
    );
  }

  isAllowed(feature) {
    let disallowedFeatures = Services.cpmm.sharedData.get(
      "EnterprisePolicies:DisallowedFeatures"
    );
    return !(disallowedFeatures && disallowedFeatures.has(feature));
  }
}

EnterprisePoliciesManagerContent.prototype.QueryInterface = ChromeUtils.generateQI(
  ["nsIEnterprisePolicies"]
);
