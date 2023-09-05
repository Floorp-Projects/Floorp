/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export const PLATFORM_LINUX = "linux";
export const PLATFORM_MACOS = "macosx";
export const PLATFORM_WINDOWS = "win";

/**
 * A grouping of buttons. Primary button order will be set automatically based
 * on class="primary", type="submit" or autofocus attribute. Set slot="primary"
 * on a primary button that does not have primary styling to set its position.
 *
 * @tagname moz-button-group
 * @property {string} platform - The detected platform, set automatically.
 */
export default class MozButtonGroup extends MozLitElement {
  static queries = {
    defaultSlotEl: "slot:not([name])",
    primarySlotEl: "slot[name=primary]",
  };

  static properties = {
    platform: { state: true },
  };

  constructor() {
    super();
    this.#detectPlatform();
  }

  #detectPlatform() {
    if (typeof AppConstants !== "undefined") {
      this.platform = AppConstants.platform;
    } else if (navigator.platform.includes("Linux")) {
      this.platform = PLATFORM_LINUX;
    } else if (navigator.platform.includes("Mac")) {
      this.platform = PLATFORM_MACOS;
    } else {
      this.platform = PLATFORM_WINDOWS;
    }
  }

  onSlotchange(e) {
    for (let child of this.defaultSlotEl.assignedNodes()) {
      if (!(child instanceof Element)) {
        // Text nodes won't support classList or getAttribute.
        continue;
      }
      // Bug 1791816: These should check moz-button instead of button.
      if (
        child.localName == "button" &&
        (child.classList.contains("primary") ||
          child.getAttribute("type") == "submit" ||
          child.hasAttribute("autofocus") ||
          child.hasAttribute("default"))
      ) {
        child.slot = "primary";
      }
    }
    this.#reorderLightDom();
  }

  #reorderLightDom() {
    let primarySlottedChildren = [...this.primarySlotEl.assignedNodes()];
    if (this.platform == PLATFORM_WINDOWS) {
      primarySlottedChildren.reverse();
      for (let child of primarySlottedChildren) {
        child.parentElement.prepend(child);
      }
    } else {
      for (let child of primarySlottedChildren) {
        // Ensure the primary buttons are at the end of the light DOM.
        child.parentElement.append(child);
      }
    }
  }

  updated(changedProperties) {
    if (changedProperties.has("platform")) {
      this.#reorderLightDom();
    }
  }

  render() {
    let slots = [
      html` <slot @slotchange=${this.onSlotchange}></slot> `,
      html` <slot name="primary"></slot> `,
    ];
    if (this.platform == PLATFORM_WINDOWS) {
      slots = [slots[1], slots[0]];
    }
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-button-group.css"
      />
      ${slots}
    `;
  }
}
customElements.define("moz-button-group", MozButtonGroup);
