/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

{

class MozDeck extends MozXULElement {
  set selectedIndex(val) {
    if (this.selectedIndex == val) return val;
    this.setAttribute("selectedIndex", val);
    var event = document.createEvent("Events");
    event.initEvent("select", true, true);
    this.dispatchEvent(event);
    return val;
  }

  get selectedIndex() {
    return this.getAttribute("selectedIndex") || "0";
  }

  set selectedPanel(val) {
    var selectedIndex = -1;
    for (var panel = val; panel != null; panel = panel.previousSibling)
      ++selectedIndex;
    this.selectedIndex = selectedIndex;
    return val;
  }

  get selectedPanel() {
    return this.childNodes[this.selectedIndex];
  }
}

customElements.define("deck", MozDeck);

class MozDropmarker extends MozXULElement {
  connectedCallback() {
    // Only create the image the first time we are connected
    if (!this.firstChild) {
      let image = document.createElement("image");
      image.classList.add("dropmarker-icon");
      this.appendChild(image);
    }
  }
}

customElements.define("dropmarker", MozDropmarker);

}
