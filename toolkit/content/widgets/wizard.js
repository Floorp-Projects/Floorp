/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
class MozWizardPage extends MozXULElement {
  connectedCallback() {
    this.pageIndex = -1;
  }
  get pageid() {
    return this.getAttribute("pageid");
  }
  set pageid(val) {
    this.setAttribute("pageid", val);
  }
  get next() {
    return this.getAttribute("next");
  }
  set next(val) {
    this.setAttribute("next", val);
    this.parentNode._accessMethod = "random";
    return val;
  }
}

customElements.define("wizardpage", MozWizardPage);
}
