/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionCommon",
  "resource://gre/modules/ExtensionCommon.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionActivityLog",
  "resource://gre/modules/ExtensionActivityLog.jsm"
);

this.activityLog = class extends ExtensionAPI {
  getAPI(context) {
    return {
      activityLog: {
        onExtensionActivity: new ExtensionCommon.EventManager({
          context,
          name: "activityLog.onExtensionActivity",
          register: (fire, id) => {
            // A logger cannot log itself.
            if (id === context.extension.id) {
              throw new ExtensionUtils.ExtensionError(
                "Extension cannot monitor itself."
              );
            }
            function handler(details) {
              fire.async(details);
            }

            ExtensionActivityLog.addListener(id, handler);
            return () => {
              ExtensionActivityLog.removeListener(id, handler);
            };
          },
        }).api(),
      },
    };
  }
};
