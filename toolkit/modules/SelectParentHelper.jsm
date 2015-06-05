/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SelectParentHelper"
];

let currentBrowser = null;

this.SelectParentHelper = {
  populate: function(menulist, items, selectedIndex) {
    // Clear the current contents of the popup
    menulist.menupopup.textContent = "";
    populateChildren(menulist, items, selectedIndex);
  },

  open: function(browser, menulist, rect) {
    menulist.hidden = false;
    currentBrowser = browser;
    this._registerListeners(menulist.menupopup);

    menulist.menupopup.openPopupAtScreenRect("after_start", rect.left, rect.top, rect.width, rect.height, false, false);
    menulist.selectedItem.scrollIntoView();
  },

  hide: function(menulist) {
    menulist.menupopup.hidePopup();
  },

  handleEvent: function(event) {
    let popup = event.currentTarget;
    let menulist = popup.parentNode;

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
        menulist.hidden = true;
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

function populateChildren(menulist, options, selectedIndex, startIndex = 0,
                          isInGroup = false, isGroupDisabled = false) {
  let index = startIndex;
  let element = menulist.menupopup;

  for (let option of options) {
    let isOptGroup = (option.tagName == 'OPTGROUP');
    let item = element.ownerDocument.createElement(isOptGroup ? "menucaption" : "menuitem");

    item.setAttribute("label", option.textContent);
    item.style.direction = option.textDirection;

    element.appendChild(item);

    // A disabled optgroup disables all of its child options.
    let isDisabled = isGroupDisabled || option.disabled;
    if (isDisabled) {
      item.setAttribute("disabled", "true");
    }

    if (isOptGroup) {
      index = populateChildren(menulist, option.children, selectedIndex, index, true, isDisabled);
    } else {
      if (index == selectedIndex) {
        // We expect the parent element of the popup to be a <xul:menulist> that
        // has the popuponly attribute set to "true". This is necessary in order
        // for a <xul:menupopup> to act like a proper <html:select> dropdown, as
        // the <xul:menulist> does things like remember state and set the
        // _moz-menuactive attribute on the selected <xul:menuitem>.
        menulist.selectedItem = item;
      }

      item.setAttribute("value", index++);

      if (isInGroup) {
        item.classList.add("contentSelectDropdown-ingroup")
      }
    }
  }

  return index;
}
