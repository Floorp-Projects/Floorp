/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SelectParentHelper"
];

var currentBrowser = null;

this.SelectParentHelper = {
  populate: function(menulist, items, selectedIndex, zoom) {
    // Clear the current contents of the popup
    menulist.menupopup.textContent = "";
    populateChildren(menulist, items, selectedIndex, zoom);
  },

  open: function(browser, menulist, rect) {
    menulist.hidden = false;
    currentBrowser = browser;
    this._registerListeners(menulist.menupopup);

    menulist.menupopup.openPopupAtScreenRect("after_start", rect.left, rect.top, rect.width, rect.height, false, false);
    menulist.selectedItem.scrollIntoView();
  },

  hide: function(menulist, browser) {
    if (currentBrowser == browser) {
      menulist.menupopup.hidePopup();
    }
  },

  handleEvent: function(event) {
    switch (event.type) {
      case "mouseover":
        currentBrowser.messageManager.sendAsyncMessage("Forms:MouseOver", {});
        break;

      case "mouseout":
        currentBrowser.messageManager.sendAsyncMessage("Forms:MouseOut", {});
        break;

      case "command":
        if (event.target.hasAttribute("value")) {
          currentBrowser.messageManager.sendAsyncMessage("Forms:SelectDropDownItem", {
            value: event.target.value
          });
        }
        break;

      case "popuphidden":
        currentBrowser.messageManager.sendAsyncMessage("Forms:DismissedDropDown", {});
        currentBrowser = null;
        let popup = event.target;
        this._unregisterListeners(popup);
        popup.parentNode.hidden = true;
        break;
    }
  },

  _registerListeners: function(popup) {
    popup.addEventListener("command", this);
    popup.addEventListener("popuphidden", this);
    popup.addEventListener("mouseover", this);
    popup.addEventListener("mouseout", this);
  },

  _unregisterListeners: function(popup) {
    popup.removeEventListener("command", this);
    popup.removeEventListener("popuphidden", this);
    popup.removeEventListener("mouseover", this);
    popup.removeEventListener("mouseout", this);
  },

};

function populateChildren(menulist, options, selectedIndex, zoom,
                          isInGroup = false, isGroupDisabled = false, adjustedTextSize = -1) {
  let element = menulist.menupopup;

  // -1 just means we haven't calculated it yet. When we recurse through this function
  // we will pass in adjustedTextSize to save on recalculations.
  if (adjustedTextSize == -1) {
    let win = element.ownerDocument.defaultView;

    // Grab the computed text size and multiply it by the remote browser's fullZoom to ensure
    // the popup's text size is matched with the content's. We can't just apply a CSS transform
    // here as the popup's preferred size is calculated pre-transform.
    let textSize = win.getComputedStyle(element).getPropertyValue("font-size");
    adjustedTextSize = (zoom * parseFloat(textSize, 10)) + "px";
  }

  for (let option of options) {
    let isOptGroup = (option.tagName == 'OPTGROUP');
    let item = element.ownerDocument.createElement(isOptGroup ? "menucaption" : "menuitem");

    item.setAttribute("label", option.textContent);
    item.style.direction = option.textDirection;
    item.style.fontSize = adjustedTextSize;
    item.style.display = option.display;
    item.setAttribute("tooltiptext", option.tooltip);

    element.appendChild(item);

    // A disabled optgroup disables all of its child options.
    let isDisabled = isGroupDisabled || option.disabled;
    if (isDisabled) {
      item.setAttribute("disabled", "true");
    }

    if (isOptGroup) {
      populateChildren(menulist, option.children, selectedIndex, zoom,
                       true, isDisabled, adjustedTextSize);
    } else {
      if (option.index == selectedIndex) {
        // We expect the parent element of the popup to be a <xul:menulist> that
        // has the popuponly attribute set to "true". This is necessary in order
        // for a <xul:menupopup> to act like a proper <html:select> dropdown, as
        // the <xul:menulist> does things like remember state and set the
        // _moz-menuactive attribute on the selected <xul:menuitem>.
        menulist.selectedItem = item;
      }

      item.setAttribute("value", option.index);

      if (isInGroup) {
        item.classList.add("contentSelectDropdown-ingroup")
      }
    }
  }
}
