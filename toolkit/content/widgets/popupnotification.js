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
        ".popup-notification-description": "popupid",
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
        ".popup-notification-button-container > .popup-notification-secondary-button":
          "oncommand=secondarybuttoncommand,label=secondarybuttonlabel,accesskey=secondarybuttonaccesskey,hidden=secondarybuttonhidden",
        ".popup-notification-button-container > toolbarseparator":
          "hidden=dropmarkerhidden",
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
            <toolbarbutton class="messageCloseButton close-icon popup-notification-closebutton tabbable" tooltiptext="&closeNotification.tooltip;"></toolbarbutton>
          </hbox>
          <label class="popup-notification-learnmore-link" is="text-link">&learnMoreNoEllipsis;</label>
          <checkbox class="popup-notification-checkbox" oncommand="PopupNotifications._onCheckboxCommand(event)"></checkbox>
          <description class="popup-notification-warning"></description>
        </vbox>
      </hbox>
      <hbox class="popup-notification-footer-container"></hbox>
      <hbox class="popup-notification-button-container panel-footer">
        <button class="popup-notification-button popup-notification-secondary-button"></button>
        <toolbarseparator></toolbarseparator>
        <button type="menu" class="popup-notification-button popup-notification-dropmarker" aria-label="&moreActionsButton.accessibleLabel;">
          <menupopup position="after_end" aria-label="&moreActionsButton.accessibleLabel;">
          </menupopup>
        </button>
        <button class="popup-notification-button popup-notification-primary-button" label="&defaultButton.label;" accesskey="&defaultButton.accesskey;"></button>
      </hbox>
      `;
    }

    static get entities() {
      return ["chrome://global/locale/notification.dtd"];
    }

    slotContents() {
      if (this._hasSlotted) {
        return;
      }
      this._hasSlotted = true;
      this.appendChild(this.constructor.fragment);

      this.button = this.querySelector(".popup-notification-primary-button");
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
      let nextSibling = this.querySelector(
        ".popup-notification-body > .popup-notification-learnmore-link"
      );
      nextSibling.before(el);
    }
  }

  customElements.define("popupnotification", MozPopupNotification);
}
