/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  class MozDeck extends MozXULElement {
    set selectedIndex(val) {
      if (this.selectedIndex == val) {
        return val;
      }
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
      for (
        var panel = val;
        panel != null;
        panel = panel.previousElementSibling
      ) {
        ++selectedIndex;
      }
      this.selectedIndex = selectedIndex;
      return val;
    }

    get selectedPanel() {
      return this.children[this.selectedIndex];
    }
  }

  customElements.define("deck", MozDeck);

  class MozDropmarker extends MozXULElement {
    constructor() {
      super();
      let shadowRoot = this.attachShadow({ mode: "open" });
      let stylesheet = document.createElement("link");
      stylesheet.rel = "stylesheet";
      stylesheet.href = "chrome://global/skin/dropmarker.css";

      let image = document.createXULElement("image");
      image.part = "icon";
      shadowRoot.append(stylesheet, image);
    }
  }

  customElements.define("dropmarker", MozDropmarker);

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
