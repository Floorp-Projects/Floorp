/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

/**
 * TODO - The translations actor will eventually be used to handle in-page translations
 * See Bug 971044
 */
export class TranslationsChild extends JSWindowActorChild {
  /**
   * @param {{ type: string }} event
   */
  handleEvent(event) {
    // TODO
    lazy.console.log("TranslationsChild observed a pageshow event.");
  }
}
