/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

/**
 * The AboutTranslationsChild is responsible for coordinating what privileged APIs
 * are exposed to the un-privileged scope of the about:translations page.
 */
export class AboutTranslationsChild extends JSWindowActorChild {
  handleEvent(event) {
    if (event.type === "DOMDocElementInserted") {
      lazy.console.log("TODO");
    }
  }
}
