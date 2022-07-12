/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const EXPORTED_SYMBOLS = ["GeckoViewPromptChild"];

class GeckoViewPromptChild extends GeckoViewActorChild {
  handleEvent(event) {
    const { type } = event;
    debug`handleEvent: ${type}`;

    switch (type) {
      case "MozOpenDateTimePicker":
      case "mozshowdropdown":
      case "mozshowdropdown-sourcetouch":
      case "click":
      case "contextmenu":
      case "DOMPopupBlocked":
        Services.prompt.wrappedJSObject.handleEvent(event);
    }
  }
}

const { debug, warn } = GeckoViewPromptChild.initLogging("GeckoViewPrompt");
