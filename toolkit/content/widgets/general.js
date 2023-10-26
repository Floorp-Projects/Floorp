/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  class MozCommandSet extends MozXULElement {
    connectedCallback() {
      if (this.getAttribute("commandupdater") === "true") {
        const events = this.getAttribute("events") || "*";
        const targets = this.getAttribute("targets") || "*";
        document.commandDispatcher.addCommandUpdater(this, events, targets);
      }
    }

    disconnectedCallback() {
      if (this.getAttribute("commandupdater") === "true") {
        document.commandDispatcher.removeCommandUpdater(this);
      }
    }
  }

  customElements.define("commandset", MozCommandSet);
}
