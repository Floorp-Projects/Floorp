/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const localization = new Localization(["toolkit/about/aboutWebrtc.ftl"], true);

/*
 * A disclosure area that has localized tooltips for expanding and collapsing
 * the area.
 */
class Disclosure {
  constructor({
    showMsg = "about-webrtc-fold-default-show-msg",
    hideMsg = "about-webrtc-fold-default-hide-msg",
    startsCollapsed = true,
  } = {}) {
    Object.assign(this, { showMsg, hideMsg, startsCollapsed });
    this.target = document.createElement("div");
    this.target.classList.add("fold-target");
    this.trigger = document.createElement("div");
    this.trigger.className = "fold-trigger";
    this.trigger.classList.add(
      "heading-medium",
      "no-print",
      this.showMsg,
      this.hideMsg
    );
    this.message = document.createElement("span");

    if (this.startsCollapsed) {
      this.collapse();
    } else {
      this.expand();
    }
    this.trigger.onclick = () => {
      if (this.target.classList.contains("fold-closed")) {
        this.expand();
      } else {
        this.collapse();
      }
    };
  }

  /** @return {Element} */
  control() {
    return this.trigger;
  }

  /** @return {Element} */
  view() {
    return this.target;
  }

  expand() {
    this.target.classList.remove("fold-closed");
    this.control().textContent = String.fromCodePoint(0x25bc);
    this.control().setAttribute(
      "title",
      localization.formatValueSync(this.hideMsg)
    );
    document.l10n.setAttributes(this.message, this.hideMsg);
  }

  collapse() {
    this.target.classList.add("fold-closed");
    this.trigger.textContent = String.fromCodePoint(0x25b6);
    this.control().setAttribute(
      "title",
      localization.formatValueSync(this.showMsg)
    );
    document.l10n.setAttributes(this.message, this.showMsg);
  }

  static expandAll() {
    for (const target of document.getElementsByClassName("fold-closed")) {
      target.classList.remove("fold-closed");
    }
    for (const trigger of document.getElementsByClassName("fold-trigger")) {
      const hideMsg = trigger.classList[2];
      document.l10n.setAttributes(trigger, hideMsg);
    }
  }

  static collapseAll() {
    for (const target of document.getElementsByClassName("fold-target")) {
      target.classList.add("fold-closed");
    }
    for (const trigger of document.getElementsByClassName("fold-trigger")) {
      const showMsg = trigger.classList[1];
      document.l10n.setAttributes(trigger, showMsg);
    }
  }
}

export { Disclosure };
