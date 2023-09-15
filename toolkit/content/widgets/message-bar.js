/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  class MessageBarElement extends HTMLElement {
    constructor() {
      super();
      const shadowRoot = this.attachShadow({ mode: "open" });
      window.MozXULElement?.insertFTLIfNeeded(
        "toolkit/global/notification.ftl"
      );
      document.l10n.connectRoot(this.shadowRoot);
      const content = this.constructor.template.content.cloneNode(true);
      shadowRoot.append(content);
      this.closeButton.addEventListener("click", () => this.dismiss(), {
        once: true,
      });
    }

    disconnectedCallback() {
      this.dispatchEvent(new CustomEvent("message-bar:close"));
    }

    get closeButton() {
      return this.shadowRoot.querySelector("button.close");
    }

    static get template() {
      const template = document.createElement("template");

      const commonStyles = document.createElement("link");
      commonStyles.rel = "stylesheet";
      commonStyles.href = "chrome://global/skin/in-content/common.css";
      const messageBarStyles = document.createElement("link");
      messageBarStyles.rel = "stylesheet";
      messageBarStyles.href =
        "chrome://global/content/elements/message-bar.css";
      template.content.append(commonStyles, messageBarStyles);

      // A container for the entire message bar content,
      // most of the css rules needed to provide the
      // expected message bar layout is applied on this
      // element.
      const container = document.createElement("div");
      container.part = "container";
      container.classList.add("container");
      template.content.append(container);

      const icon = document.createElement("span");
      icon.classList.add("icon");
      icon.part = "icon";
      container.append(icon);

      const barcontent = document.createElement("span");
      barcontent.classList.add("content");
      barcontent.append(document.createElement("slot"));
      container.append(barcontent);

      const spacer = document.createElement("span");
      spacer.classList.add("spacer");
      container.append(spacer);

      const closeIcon = document.createElement("button");
      closeIcon.classList.add("close", "ghost-button");
      document.l10n.setAttributes(closeIcon, "notification-close-button");
      container.append(closeIcon);

      Object.defineProperty(this, "template", {
        value: template,
      });

      return template;
    }

    dismiss() {
      this.dispatchEvent(new CustomEvent("message-bar:user-dismissed"));
      this.close();
    }

    close() {
      this.remove();
    }
  }

  customElements.define("message-bar", MessageBarElement);
}
