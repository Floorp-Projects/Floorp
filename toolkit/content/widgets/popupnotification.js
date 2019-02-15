/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
class MozPopupNotification extends MozXULElement {
  static get observedAttributes() {
    return [
      "buttonaccesskey",
      "buttoncommand",
      "buttonhighlight",
      "buttonlabel",
      "closebuttoncommand",
      "closebuttonhidden",
      "dropmarkerhidden",
      "dropmarkerpopupshown",
      "endlabel",
      "icon",
      "iconclass",
      "label",
      "learnmoreclick",
      "learnmoreurl",
      "mainactiondisabled",
      "menucommand",
      "name",
      "origin",
      "origin",
      "popupid",
      "secondarybuttonaccesskey",
      "secondarybuttoncommand",
      "secondarybuttonhidden",
      "secondarybuttonlabel",
      "secondendlabel",
      "secondname",
      "warninghidden",
      "warninglabel",
    ];
  }

  _updateAttributes() {
    for (let [ el, attrs ] of this._inheritedAttributeMap.entries()) {
      for (let attr of attrs) {
        this.inheritAttribute(el, attr);
      }
    }
  }

  get _inheritedAttributeMap() {
    if (!this.__inheritedAttributeMap) {
      this.__inheritedAttributeMap = new Map();
      for (let el of this.querySelectorAll("[inherits]")) {
        this.__inheritedAttributeMap.set(el, el.getAttribute("inherits").split(","));
      }
    }
    return this.__inheritedAttributeMap;
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (!this._hasSlotted || oldValue === newValue) {
      return;
    }

    this._updateAttributes();
  }

  show() {
    this.slotContents();

    if (this.checkboxState) {
      this.checkbox.checked = this.checkboxState.checked;
      this.checkbox.setAttribute("label", this.checkboxState.label);
      this.checkbox.hidden = false;
    } else {
      this.checkbox.hidden = true;
    }

    this.hidden = false;
  }

  slotContents() {
    if (this._hasSlotted) {
      return;
    }
    this._hasSlotted = true;
    this.appendChild(MozXULElement.parseXULToFragment(`
      <hbox class="popup-notification-header-container"></hbox>
      <hbox align="start" class="popup-notification-body-container">
        <image class="popup-notification-icon"
               inherits="popupid,src=icon,class=iconclass"/>
        <vbox flex="1" pack="start" class="popup-notification-body">
          <hbox align="start">
            <vbox flex="1">
              <label class="popup-notification-origin header" inherits="value=origin,tooltiptext=origin" crop="center"></label>
              <!-- These need to be on the same line to avoid creating
                  whitespace between them (whitespace is added in the
                  localization file, if necessary). -->
              <description class="popup-notification-description" inherits="popupid"><html:span inherits="text=label,popupid"></html:span><html:b inherits="text=name,popupid"></html:b><html:span inherits="text=endlabel,popupid"></html:span><html:b inherits="text=secondname,popupid"></html:b><html:span inherits="text=secondendlabel,popupid"></html:span></description>
            </vbox>
            <toolbarbutton class="messageCloseButton close-icon popup-notification-closebutton tabbable" inherits="oncommand=closebuttoncommand,hidden=closebuttonhidden" tooltiptext="&closeNotification.tooltip;"></toolbarbutton>
          </hbox>
          <label class="text-link popup-notification-learnmore-link" inherits="onclick=learnmoreclick,href=learnmoreurl">&learnMore;</label>
          <checkbox class="popup-notification-checkbox" oncommand="PopupNotifications._onCheckboxCommand(event)"></checkbox>
          <description class="popup-notification-warning" inherits="hidden=warninghidden,text=warninglabel"></description>
        </vbox>
      </hbox>
      <hbox class="popup-notification-footer-container"></hbox>
      <hbox class="popup-notification-button-container panel-footer">
        <button class="popup-notification-button popup-notification-secondary-button" inherits="oncommand=secondarybuttoncommand,label=secondarybuttonlabel,accesskey=secondarybuttonaccesskey,hidden=secondarybuttonhidden"></button>
        <toolbarseparator inherits="hidden=dropmarkerhidden"></toolbarseparator>
        <button type="menu" class="popup-notification-button popup-notification-dropmarker" aria-label="&moreActionsButton.accessibleLabel;" inherits="onpopupshown=dropmarkerpopupshown,hidden=dropmarkerhidden">
          <menupopup position="after_end" aria-label="&moreActionsButton.accessibleLabel;" inherits="oncommand=menucommand">
          </menupopup>
        </button>
        <button class="popup-notification-button popup-notification-primary-button" label="&defaultButton.label;" accesskey="&defaultButton.accesskey;" inherits="oncommand=buttoncommand,label=buttonlabel,accesskey=buttonaccesskey,default=buttonhighlight,disabled=mainactiondisabled"></button>
      </hbox>
    `, ["chrome://global/locale/notification.dtd"]));

    this.button = this.querySelector(".popup-notification-primary-button");
    this.secondaryButton =  this.querySelector(".popup-notification-secondary-button");
    this.checkbox = this.querySelector(".popup-notification-checkbox");
    this.closebutton = this.querySelector(".popup-notification-closebutton");
    this.menubutton = this.querySelector(".popup-notification-dropmarker");
    this.menupopup = this.menubutton.querySelector("menupopup");

    let popupnotificationfooter = this.querySelector("popupnotificationfooter");
    if (popupnotificationfooter) {
      this.querySelector(".popup-notification-footer-container").append(popupnotificationfooter);
    }

    let popupnotificationheader = this.querySelector("popupnotificationheader");
    if (popupnotificationheader) {
      this.querySelector(".popup-notification-header-container").append(popupnotificationheader);
    }

    for (let popupnotificationcontent of this.querySelectorAll("popupnotificationcontent")) {
      this.appendNotificationContent(popupnotificationcontent);
    }

    this._updateAttributes();
  }

  appendNotificationContent(el) {
    let nextSibling = this.querySelector(".popup-notification-body > .popup-notification-learnmore-link");
    nextSibling.before(el);
  }
}

customElements.define("popupnotification", MozPopupNotification);
}
