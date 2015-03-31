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
    populateChildren(menulist.menupopup, items, selectedIndex);
    // We expect the parent element of the popup to be a <xul:menulist> that
    // has the popuponly attribute set to "true". This is necessary in order
    // for a <xul:menupopup> to act like a proper <html:select> dropdown, as
    // the <xul:menulist> does things like remember state and set the
    // _moz-menuactive attribute on the selected <xul:menuitem>.
    menulist.selectedIndex = selectedIndex;
  },

  open: function(browser, menulist, rect) {
    menulist.hidden = false;
    currentBrowser = browser;
    this._registerListeners(menulist.menupopup);

    let {x, y} = browser.mapScreenCoordinatesFromContent(rect.left, rect.top + rect.height);
    menulist.menupopup.openPopupAtScreen(x, y);
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

function populateChildren(element, options, selectedIndex, startIndex = 0, isGroup = false) {
  let index = startIndex;

  for (let option of options) {
    let item = element.ownerDocument.createElement("menuitem");
    item.setAttribute("label", option.textContent);

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
