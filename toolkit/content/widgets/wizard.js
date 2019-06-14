/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const kDTDs = [ "chrome://global/locale/wizard.dtd" ];

class MozWizardPage extends MozXULElement {
  constructor() {
    super();
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

class MozWizardButtons extends MozXULElement {
  connectedCallback() {
    this.textContent = "";
    this.appendChild(MozXULElement.parseXULToFragment(this._markup, kDTDs));

    this._wizardButtonDeck = this.querySelector(".wizard-next-deck");

    this.initializeAttributeInheritance();

    const listeners = [
      ["back", () => document.documentElement.rewind()],
      ["next", () => document.documentElement.advance()],
      ["finish", () => document.documentElement.advance()],
      ["cancel", () => document.documentElement.cancel()],
      ["extra1", () => document.documentElement.extra1()],
      ["extra2", () => document.documentElement.extra2()],
    ];
    for (let [name, listener] of listeners) {
      let btn = this.getButton(name);
      if (btn) {
        btn.addEventListener("command", listener);
      }
    }
  }

  static get inheritedAttributes() {
    return AppConstants.platform == "macosx" ? {
      "[dlgtype='next']": "hidden=lastpage",
    } : null;
  }

  get _markup() {
    if (AppConstants.platform == "macosx") {
      return `
        <vbox flex="1">
          <hbox class="wizard-buttons-btm">
            <button class="wizard-button" dlgtype="extra1" hidden="true"/>
            <button class="wizard-button" dlgtype="extra2" hidden="true"/>
            <button label="&button-cancel-mac.label;"
                    class="wizard-button" dlgtype="cancel"/>
            <spacer flex="1"/>
            <button label="&button-back-mac.label;"
                    accesskey="&button-back-mac.accesskey;"
                    class="wizard-button wizard-nav-button" dlgtype="back"/>
            <button label="&button-next-mac.label;"
                    accesskey="&button-next-mac.accesskey;"
                    class="wizard-button wizard-nav-button" dlgtype="next"
                    default="true" />
            <button label="&button-finish-mac.label;" class="wizard-button"
                    dlgtype="finish" default="true" />
          </hbox>
        </vbox>`;
    }

    let buttons = AppConstants.platform == "linux" ? `
      <button label="&button-cancel-unix.label;"
              class="wizard-button"
              dlgtype="cancel"/>
      <spacer style="width: 24px;"/>
      <button label="&button-back-unix.label;"
              accesskey="&button-back-unix.accesskey;"
              class="wizard-button" dlgtype="back"/>
      <deck class="wizard-next-deck">
        <hbox>
          <button label="&button-finish-unix.label;"
                  class="wizard-button"
                  dlgtype="finish" default="true" flex="1"/>
        </hbox>
        <hbox>
          <button label="&button-next-unix.label;"
                  accesskey="&button-next-unix.accesskey;"
                  class="wizard-button" dlgtype="next"
                  default="true" flex="1"/>
        </hbox>
      </deck>` : `
      <button label="&button-back-win.label;"
              accesskey="&button-back-win.accesskey;"
              class="wizard-button" dlgtype="back"/>
      <deck class="wizard-next-deck">
        <hbox>
          <button label="&button-finish-win.label;"
                  class="wizard-button"
                  dlgtype="finish" default="true" flex="1"/>
        </hbox>
        <hbox>
          <button label="&button-next-win.label;"
                  accesskey="&button-next-win.accesskey;"
                  class="wizard-button" dlgtype="next"
                  default="true" flex="1"/>
        </hbox>
      </deck>
      <button label="&button-cancel-win.label;"
              class="wizard-button"
              dlgtype="cancel"/>`;

    return `
      <vbox class="wizard-buttons-box-1" flex="1">
        <separator class="wizard-buttons-separator groove"/>
        <hbox class="wizard-buttons-box-2">
          <button class="wizard-button" dlgtype="extra1" hidden="true"/>
          <button class="wizard-button" dlgtype="extra2" hidden="true"/>
          <spacer flex="1" anonid="spacer"/>
          ${buttons}
        </hbox>
      </vbox>`;
  }

  onPageChange() {
    if (AppConstants.platform == "macosx") {
      this.getButton("finish").hidden = !(this.getAttribute("lastpage") == "true");
    } else if (this.getAttribute("lastpage") == "true") {
      this._wizardButtonDeck.setAttribute("selectedIndex", 0);
    } else {
      this._wizardButtonDeck.setAttribute("selectedIndex", 1);
    }
  }

  getButton(type) {
    return this.querySelector(`[dlgtype="${type}"]`);
  }

  get defaultButton() {
    const kXULNS =
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let buttons = this._wizardButtonDeck.selectedPanel.
      getElementsByTagNameNS(kXULNS, "button");
    for (let i = 0; i < buttons.length; i++) {
      if (buttons[i].getAttribute("default") == "true" &&
          !buttons[i].hidden && !buttons[i].disabled) {
        return buttons[i];
      }
    }
    return null;
  }
}

customElements.define("wizard-buttons", MozWizardButtons);
}
