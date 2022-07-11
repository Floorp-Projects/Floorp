/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["UnselectedTabHoverChild"];

class UnselectedTabHoverChild extends JSWindowActorChild {
  receiveMessage(message) {
    Services.obs.notifyObservers(
      this.contentWindow,
      "unselected-tab-hover",
      message.data.hovered
    );
  }

  handleEvent(event) {
    this.sendAsyncMessage("UnselectedTabHover:Toggle", {
      enable: event.type == "UnselectedTabHover:Enable",
    });
  }
}
