/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DOM"];

const {ContentProcessDomain} = ChromeUtils.import("chrome://remote/content/domains/ContentProcessDomain.jsm");

class DOM extends ContentProcessDomain {
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
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
    }
  }

  getContentQuads({ objectId }) {
    const Runtime = this.session.domains.get("Runtime");
    if (!Runtime) {
      throw new Error("Runtime domain is not instantiated");
    }
    const obj = Runtime.getRemoteObject(objectId);
    if (!obj) {
      throw new Error("Cannot find object with id = " + objectId);
    }
    const unsafeObject = obj.unsafeDereference();
    if (!unsafeObject.getBoxQuads) {
      throw new Error("RemoteObject is not a node");
    }
    let quads = unsafeObject.getBoxQuads({ relativeTo: this.content.document });
    quads = quads.map(quad => {
      return [
        quad.p1.x, quad.p1.y,
        quad.p2.x, quad.p2.y,
        quad.p3.x, quad.p3.y,
        quad.p4.x, quad.p4.y,
      ].map(Math.round);
    });
    return { quads };
  }
}
