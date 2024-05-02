/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";

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
    iconSrc: "chrome://global/skin/icons/check-filled.svg",
    l10nId: "moz-message-bar-icon-success",
  },
  error: {
    iconSrc: "chrome://global/skin/icons/error.svg",
    l10nId: "moz-message-bar-icon-error",
  },
  critical: {
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
 * @property {string} heading - The heading of the message.
 * @property {string} message - The message text.
 * @property {boolean} dismissable - Whether or not the element is dismissable.
 * @property {string} messageL10nId - l10n ID for the message.
 * @property {string} messageL10nArgs - Any args needed for the message l10n ID.
 * @fires message-bar:close
 *  Custom event indicating that message bar was closed.
 * @fires message-bar:user-dismissed
 *  Custom event indicating that message bar was dismissed by the user.
 */

export default class MozMessageBar extends MozLitElement {
  static queries = {
    actionsSlot: "slot[name=actions]",
    actionsEl: ".actions",
    closeButton: "moz-button.close",
    supportLinkSlot: "slot[name=support-link]",
  };

  static properties = {
    type: { type: String },
    heading: { type: String },
    message: { type: String },
    dismissable: { type: Boolean },
    messageL10nId: { type: String },
    messageL10nArgs: { type: String },
  };

  constructor() {
    super();
    window.MozXULElement?.insertFTLIfNeeded("toolkit/global/mozMessageBar.ftl");
    this.type = "info";
    this.dismissable = false;
  }

  onSlotchange() {
    let actions = this.actionsSlot.assignedNodes();
    this.actionsEl.classList.toggle("active", actions.length);
  }

  connectedCallback() {
    super.connectedCallback();
    this.setAttribute("role", "alert");
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.dispatchEvent(new CustomEvent("message-bar:close"));
  }

  get supportLinkEls() {
    return this.supportLinkSlot.assignedElements();
  }

  iconTemplate() {
    let iconData = messageTypeToIconData[this.type];
    if (iconData) {
      let { iconSrc, l10nId } = iconData;
      return html`
        <div class="icon-container">
          <img
            class="icon"
            src=${iconSrc}
            data-l10n-id=${l10nId}
            data-l10n-attrs="alt"
          />
        </div>
      `;
    }
    return "";
  }

  headingTemplate() {
    if (this.heading) {
      return html`<strong class="heading">${this.heading}</strong>`;
    }
    return "";
  }

  closeButtonTemplate({ size } = {}) {
    if (this.dismissable) {
      return html`
        <moz-button
          type="icon ghost"
          class="close"
          size=${ifDefined(size)}
          data-l10n-id="moz-message-bar-close-button"
          @click=${this.dismiss}
        ></moz-button>
      `;
    }
    return "";
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-message-bar.css"
      />
      <div class="container">
        <div class="content">
          <div class="text-container">
            ${this.iconTemplate()}
            <div class="text-content">
              ${this.headingTemplate()}
              <div>
                <span
                  class="message"
                  data-l10n-id=${ifDefined(this.messageL10nId)}
                  data-l10n-args=${ifDefined(
                    JSON.stringify(this.messageL10nArgs)
                  )}
                >
                  ${this.message}
                </span>
                <span class="link">
                  <slot name="support-link"></slot>
                </span>
              </div>
            </div>
          </div>
          <span class="actions">
            <slot name="actions" @slotchange=${this.onSlotchange}></slot>
          </span>
        </div>
        ${this.closeButtonTemplate()}
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
