/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

const messageTypeToIconData = {
  info: {
    iconSrc: "chrome://global/skin/icons/info-filled.svg",
    l10nId: "moz-message-bar-icon-info",
  },
  warning: {
    iconSrc: "chrome://global/skin/icons/warning.svg",
    l10nId: "moz-message-bar-icon-warning",
  },
  success: {
    iconSrc: "chrome://global/skin/icons/check.svg",
    l10nId: "moz-message-bar-icon-success",
  },
  error: {
    iconSrc: "chrome://global/skin/icons/error.svg",
    l10nId: "moz-message-bar-icon-error",
  },
};

/**
 * A simple message bar element that can be used to display
 * important information to users.
 *
 * @tagname moz-message-bar
 * @property {string} type - The type of the displayed message.
 * @property {string} header - The header of the message.
 * @property {string} message - The message text.
 * @fires message-bar:close
 *  Custom event indicating that message bar was closed.
 *  @fires message-bar:user-dismissed
 *  Custom event indicating that message bar was dismissed by the user.
 */

export default class MozMessageBar extends MozLitElement {
  static properties = {
    type: { type: String },
    header: { type: String },
    message: { type: String },
  };

  // Use a relative URL in storybook to get faster reloads on style changes.
  static stylesheetUrl = window.IS_STORYBOOK
    ? "./moz-message-bar/moz-message-bar.css"
    : "chrome://global/content/elements/moz-message-bar.css";

  constructor() {
    super();
    MozXULElement.insertFTLIfNeeded("toolkit/global/mozMessageBar.ftl");
    this.type = "info";
    this.role = "status";
  }

  disconnectedCallback() {
    this.dispatchEvent(new CustomEvent("message-bar:close"));
  }

  iconTemplate() {
    let iconData = messageTypeToIconData[this.type];
    if (iconData) {
      let { iconSrc, l10nId } = iconData;
      return html`
        <img
          class="icon"
          src=${iconSrc}
          data-l10n-id=${l10nId}
          data-l10n-attrs="alt"
        />
      `;
    }
    return "";
  }

  headerTemplate() {
    if (this.header) {
      return html`<strong class="header">${this.header}</strong>`;
    }
    return "";
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <link rel="stylesheet" href=${this.constructor.stylesheetUrl} />
      <div class="container">
        ${this.iconTemplate()}
        <div class="content">
          ${this.headerTemplate()}
          <span class="message">${ifDefined(this.message)}</span>
          <slot name="support-link"></slot>
        </div>
        <slot name="actions"></slot>
        <span class="spacer"></span>
        <button
          class="close ghost-button"
          data-l10n-id="moz-message-bar-close-button"
          @click=${this.dismiss}
        ></button>
      </div>
    `;
  }

  dismiss() {
    this.dispatchEvent(new CustomEvent("message-bar:user-dismissed"));
    this.close();
  }

  close() {
    this.remove();
  }
}

customElements.define("moz-message-bar", MozMessageBar);
