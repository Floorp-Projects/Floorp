/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SelectParentHelper"
];

let currentBrowser = null;

this.SelectParentHelper = {
  populate: function(popup, items, selectedIndex) {
    // Clear the current contents of the popup
    popup.textContent = "";
    populateChildren(popup, items, selectedIndex);
  },

  open: function(browser, popup, rect) {
    currentBrowser = browser;
    this._registerListeners(popup);
    popup.hidden = false;

    let {x, y} = browser.mapScreenCoordinatesFromContent(rect.left, rect.top + rect.height);
    popup.openPopupAtScreen(x, y);
  },

  hide: function(popup) {
    popup.hidePopup();
  },

  handleEvent: function(event) {
    let popup = event.currentTarget;

    switch (event.type) {
      case "command":
        if (event.target.hasAttribute("value")) {
          currentBrowser.messageManager.sendAsyncMessage("Forms:SelectDropDownItem", {
            value: event.target.value
          });
        }
        popup.hidePopup();
        break;

      case "popuphidden":
        currentBrowser.messageManager.sendAsyncMessage("Forms:DismissedDropDown", {});
        currentBrowser = null;
        this._unregisterListeners(popup);
        break;
    }
  },

  _registerListeners: function(popup) {
    popup.addEventListener("command", this);
    popup.addEventListener("popuphidden", this);
  },

  _unregisterListeners: function(popup) {
    popup.removeEventListener("command", this);
    popup.removeEventListener("popuphidden", this);
  },

};

function populateChildren(element, options, selectedIndex, startIndex = 0, isGroup = false) {
  let index = startIndex;

  for (let option of options) {
    let item = element.ownerDocument.createElement("menuitem");
    item.setAttribute("label", option.textContent);
    item.setAttribute("type", "radio");

    if (index == selectedIndex) {
      item.setAttribute("checked", "true");
    }

    element.appendChild(item);

    if (option.children.length > 0) {
      item.classList.add("contentSelectDropdown-optgroup");
      item.setAttribute("disabled", "true");
      index = populateChildren(element, option.children, selectedIndex, index, true);
    } else {
      item.setAttribute("value", index++);

      if (isGroup) {
        item.classList.add("contentSelectDropdown-ingroup")
      }
    }
  }

  return index;
}
