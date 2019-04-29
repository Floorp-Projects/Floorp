/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Runtime"];

const {ContentProcessDomain} = ChromeUtils.import("chrome://remote/content/domains/ContentProcessDomain.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

class Runtime extends ContentProcessDomain {
  constructor(session) {
    super(session);
    this.enabled = false;
  }

  destructor() {
    this.disable();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
      this.chromeEventHandler.addEventListener("DOMWindowCreated", this,
        {mozSystemGroup: true});

      // Spin the event loop in order to send the `executionContextCreated` event right
      // after we replied to `enable` request.
      Services.tm.dispatchToMainThread(() => {
        const frameId = this.content.windowUtils.outerWindowID;
        const id = this.content.windowUtils.currentInnerWindowID;
        this.emit("Runtime.executionContextCreated", {
          context: {
            id,
            auxData: {
              isDefault: true,
              frameId,
            },
          },
        });
      });
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
      this.chromeEventHandler.removeEventListener("DOMWindowCreated", this,
        {mozSystemGroup: true});
    }
  }

  handleEvent({type, target}) {
    const frameId = target.defaultView.windowUtils.outerWindowID;
    const id = target.defaultView.windowUtils.currentInnerWindowID;
    switch (type) {
    case "DOMWindowCreated":
      this.emit("Runtime.executionContextCreated", {
        context: {
          id,
          auxData: {
            isDefault: target == this.content.document,
            frameId,
          },
        },
      });
      break;
    }
  }
}
