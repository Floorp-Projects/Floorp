/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  class MozPopupNotification extends MozXULElement {
    static get inheritedAttributes() {
      return {
        ".popup-notification-icon": "popupid,src=icon,class=iconclass,hasicon",
        ".popup-notification-origin": "value=origin,tooltiptext=origin",
        ".popup-notification-description": "popupid,id=descriptionid",
        ".popup-notification-description > span:first-of-type":
          "text=label,popupid",
        ".popup-notification-description > b:first-of-type":
          "text=name,popupid",
        ".popup-notification-description > span:nth-of-type(2)":
          "text=endlabel,popupid",
        ".popup-notification-description > b:last-of-type":
          "text=secondname,popupid",
        ".popup-notification-description > span:last-of-type":
          "text=secondendlabel,popupid",
        ".popup-notification-hint-text": "text=hinttext",
        ".popup-notification-closebutton":
          "oncommand=closebuttoncommand,hidden=closebuttonhidden",
        ".popup-notification-learnmore-link":
          "onclick=learnmoreclick,href=learnmoreurl",
        ".popup-notification-warning": "hidden=warninghidden,text=warninglabel",
        ".popup-notification-secondary-button":
          "oncommand=secondarybuttoncommand,label=secondarybuttonlabel,accesskey=secondarybuttonaccesskey,hidden=secondarybuttonhidden,dropmarkerhidden",
        ".popup-notification-dropmarker":
          "onpopupshown=dropmarkerpopupshown,hidden=dropmarkerhidden",
        ".popup-notification-dropmarker > menupopup": "oncommand=menucommand",
        ".popup-notification-primary-button":
          "oncommand=buttoncommand,label=buttonlabel,accesskey=buttonaccesskey,default=buttonhighlight,disabled=mainactiondisabled",
      };
    }

    attributeChangedCallback(name, oldValue, newValue) {
      if (!this._hasSlotted) {
        return;
      }

      // If the label and/or accesskey for the primary button is set by
      // inherited attributes, its data-l10n-id needs to be unset or
      // DOM Localization will overwrite the values.
      if (name === "buttonlabel" || name === "buttonaccesskey") {
        document.l10n.setAttributes(this.button, "");
      }

      super.attributeChangedCallback(name, oldValue, newValue);
    }

    show() {
      this.slotContents();

      if (this.checkboxState) {
        this.checkbox.checked = this.checkboxState.checked;
        this.checkbox.setAttribute("label", this.checkboxState.label);
        this.checkbox.hidden = false;
      } else {
        this.checkbox.hidden = true;
        // Reset checked state to avoid wrong using of previous value.
        this.checkbox.checked = false;
      }

      this.hidden = false;
    }

    static get markup() {
      return `
      <hbox class="popup-notification-header-container"></hbox>
      <hbox align="start" class="popup-notification-body-container">
        <image class="popup-notification-icon"/>
        <vbox flex="1" pack="start" class="popup-notification-body">
          <hbox align="start">
            <vbox flex="1">
              <label class="popup-notification-origin header" crop="center"></label>
              <!-- These need to be on the same line to avoid creating
                  whitespace between them (whitespace is added in the
                  localization file, if necessary). -->
              <description class="popup-notification-description"><html:span></html:span><html:b></html:b><html:span></html:span><html:b></html:b><html:span></html:span></description>
              <description class="popup-notification-hint-text"></description>
            </vbox>
            <toolbarbutton class="messageCloseButton close-icon popup-notification-closebutton tabbable" data-l10n-id="close-notification-message"></toolbarbutton>
          </hbox>
          <vbox class="popup-notification-bottom-content" align="start">
            <label class="popup-notification-learnmore-link" is="text-link" data-l10n-id="popup-notification-learn-more"></label>
            <checkbox class="popup-notification-checkbox" oncommand="PopupNotifications._onCheckboxCommand(event)"/>
            <description class="popup-notification-warning"/>
          </vbox>
        </vbox>
      </hbox>
      <hbox class="popup-notification-footer-container"></hbox>
      <hbox class="popup-notification-button-container panel-footer">
        <button class="popup-notification-secondary-button"/>
        <button type="menu" class="popup-notification-dropmarker" data-l10n-id="popup-notification-more-actions-button">
          <menupopup position="after_end" data-l10n-id="popup-notification-more-actions-button">
          </menupopup>
        </button>
        <button class="popup-notification-primary-button" data-l10n-id="popup-notification-default-button"/>
      </hbox>
      `;
    }

    slotContents() {
      if (this._hasSlotted) {
        return;
      }
      this._hasSlotted = true;
      MozXULElement.insertFTLIfNeeded("toolkit/global/notification.ftl");
      MozXULElement.insertFTLIfNeeded("toolkit/global/popupnotification.ftl");
      this.appendChild(this.constructor.fragment);

      this.button = this.querySelector(".popup-notification-primary-button");
      if (
        this.hasAttribute("buttonlabel") ||
        this.hasAttribute("buttonaccesskey")
      ) {
        this.button.removeAttribute("data-l10n-id");
      }
      this.secondaryButton = this.querySelector(
        ".popup-notification-secondary-button"
      );
      this.checkbox = this.querySelector(".popup-notification-checkbox");
      this.closebutton = this.querySelector(".popup-notification-closebutton");
      this.menubutton = this.querySelector(".popup-notification-dropmarker");
      this.menupopup = this.menubutton.querySelector("menupopup");

      let popupnotificationfooter = this.querySelector(
        "popupnotificationfooter"
      );
      if (popupnotificationfooter) {
        this.querySelector(".popup-notification-footer-container").append(
          popupnotificationfooter
        );
      }

      let popupnotificationheader = this.querySelector(
        "popupnotificationheader"
      );
      if (popupnotificationheader) {
        this.querySelector(".popup-notification-header-container").append(
          popupnotificationheader
        );
      }

      for (let popupnotificationcontent of this.querySelectorAll(
        "popupnotificationcontent"
      )) {
        this.appendNotificationContent(popupnotificationcontent);
      }

      this.initializeAttributeInheritance();
    }

    appendNotificationContent(el) {
      this.querySelector(".popup-notification-bottom-content").before(el);
    }
  }

  customElements.define("popupnotification", MozPopupNotification);
}
