/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ZoomChild"];

/**
 * FIXME(emilio): Most of this code is a bit useless and could be changed by an
 * event listener in the parent process I suspect.
 */
class ZoomChild extends JSWindowActorChild {
  constructor() {
    super();
  }

  handleEvent(event) {
    // Send do zoom events to our parent as messages, to be re-dispatched.
    if (event.type == "DoZoomEnlargeBy10") {
      this.sendAsyncMessage("DoZoomEnlargeBy10", {});
      return;
    }

    if (event.type == "DoZoomReduceBy10") {
      this.sendAsyncMessage("DoZoomReduceBy10", {});
    }
  }
}
