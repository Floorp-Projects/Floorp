/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

/**
 * A simple message bar element that can be used to display
 * important information to users.
 *
 * @tagname moz-message-bar
 * @property {string} type - The type of the displayed message.
 * @property {string} message - The message text.
 * @fires message-bar:close
 *  Custom event indicating that message bar was closed.
 *  @fires message-bar:user-dismissed
 *  Custom event indicating that message bar was dismissed by the user.
 */

export default class MozMessageBar extends MozLitElement {
  static properties = {
    type: { type: String },
    message: { type: String },
  };

  // Use a relative URL in storybook to get faster reloads on style changes.
  static stylesheetUrl = window.IS_STORYBOOK
    ? "./moz-message-bar/moz-message-bar.css"
    : "chrome://global/content/elements/moz-message-bar.css";

  constructor() {
    super();
    MozXULElement.insertFTLIfNeeded("toolkit/global/notification.ftl");
    this.type = "info";
    this.role = "status";
  }

  disconnectedCallback() {
    this.dispatchEvent(new CustomEvent("message-bar:close"));
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <link rel="stylesheet" href=${this.constructor.stylesheetUrl} />
      <div class="container">
        <span class="icon"></span>
        <div class="content">
          <span class="message">${this.message}</span>
          <slot name="link"></slot>
        </div>
        <slot name="actions"></slot>
        <span class="spacer"></span>
        <button
          class="close ghost-button"
          data-l10n-id="notification-close-button"
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
