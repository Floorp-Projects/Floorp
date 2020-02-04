/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, Services, XPCOMUtils */

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

this.experiments = class extends ExtensionAPI {
  getAPI(context) {
    function promiseActiveExperiments() {
      return EventDispatcher.instance.sendRequestForResult({
        type: "Experiments:GetActive",
      });
    }
    return {
      experiments: {
        async isActive(name) {
          if (!Services.androidBridge || !Services.androidBridge.isFennec) {
            return undefined;
          }
          return promiseActiveExperiments().then(experiments => {
            return experiments.includes(name);
          });
        },
      },
    };
  }
};
