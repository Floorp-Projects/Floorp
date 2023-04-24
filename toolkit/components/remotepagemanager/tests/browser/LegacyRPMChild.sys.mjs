/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class LegacyRPMChild extends JSWindowActorChild {
  #rpmInitialized = false;
  actorCreated() {
    if (this.#rpmInitialized) {
      return;
    }
    // Strip the hash from the URL, because it's not part of the origin.
    let url = this.document.documentURI.replace(/[\#?].*$/, "");

    let registeredURLs = Services.cpmm.sharedData.get("RemotePageManager:urls");

    if (registeredURLs && registeredURLs.has(url)) {
      let { ChildMessagePort } = ChromeUtils.importESModule(
        "resource://gre/modules/remotepagemanager/RemotePageManagerChild.sys.mjs"
      );
      // Set up the child side of the message port
      new ChildMessagePort(this.contentWindow);
      this.#rpmInitialized = true;
    }
  }

  // DOMDocElementInserted is only used to create the actor.
  handleEvent() {}
}
