/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class RemoveLogins extends MozLitElement {
  constructor() {
    super();
    this.data = { total: 0 };
    this.confirmed = false;
  }

  static get properties() {
    return {
      data: { type: Object },
      confirmed: { type: Boolean },
    };
  }

  #handleCheckboxInputChange(event) {
    this.confirmed = event.target.checked;
  }

  render() {
    const args = JSON.stringify({ total: this.data.total });

    return html` <div class="remove-logins-container">
      <link rel="stylesheet" href="chrome://global/skin/in-content/common.css">
      <link rel="stylesheet" href="chrome://browser/content/aboutlogins/common.css">
      <link rel="stylesheet" href="chrome://browser/content/aboutlogins/components/remove-logins-dialog.css">
      <div class="overlay">
        <div class="container" role="dialog" aria-labelledby="title" aria-describedby="message">
          <button class="dismiss-button ghost-button" data-l10n-id="confirmation-dialog-dismiss-button">
            <img class="dismiss-icon" src="chrome://global/skin/icons/close.svg" draggable="false"/>
          </button>
          <div class="content">
            <img class="warning-icon" src="chrome://global/skin/icons/delete.svg" draggable="false"/>
            <h1 class="title" data-l10n-id="passwords-remove-all-title" data-l10n-args=${args}></h1>
            <p class="message" data-l10n-id="passwords-remove-all-message" data-l10n-args=${args} id="message"></p>
            <label class="checkbox-wrapper toggle-container-with-text">
              <input @input=${
                this.#handleCheckboxInputChange
              } id="confirmation-checkbox" type="checkbox" class="checkbox"></input>
              <span data-l10n-id="passwords-remove-all-confirm" data-l10n-args=${args} class="checkbox-text"></span>
            </label>
          </div>
          <moz-button-group class="buttons">
            <button data-l10n-id="passwords-remove-all-confirm-button" data-command="LoginDataSource.confirmRemoveAll" class="confirm-button primary danger-button" ?disabled=${!this
              .confirmed}></button>
            <button data-command="LoginDataSource.cancelDialog" class="cancel-button" data-l10n-id="confirmation-dialog-cancel-button"></button>
          </moz-button-group>
      </div>
      </div>
    </div>`;
  }
}

customElements.define("remove-logins", RemoveLogins);
