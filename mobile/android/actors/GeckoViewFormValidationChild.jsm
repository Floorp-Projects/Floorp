/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

const EXPORTED_SYMBOLS = ["GeckoViewFormValidationChild"];

class GeckoViewFormValidationChild extends GeckoViewActorChild {
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozInvalidForm": {
        // Handle invalid form submission. If we don't hook up to this,
        // invalid forms are allowed to be submitted!
        aEvent.preventDefault();
        // We should show the validation message, bug 1510450.
        break;
      }
    }
  }
}
