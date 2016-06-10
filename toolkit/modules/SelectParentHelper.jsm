/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SelectParentHelper"
];

var currentBrowser = null;
var currentMenulist = null;
var currentZoom = 1;

this.SelectParentHelper = {
  populate: function(menulist, items, selectedIndex, zoom) {
    // Clear the current contents of the popup
    menulist.menupopup.textContent = "";
    currentZoom = zoom;
    currentMenulist = menulist;
    populateChildren(menulist, items, selectedIndex, zoom);
  },

  open: function(browser, menulist, rect) {
    menulist.hidden = false;
    currentBrowser = browser;
    this._registerListeners(browser, menulist.menupopup);

    let win = browser.ownerDocument.defaultView;
    let constraintRect = browser.getBoundingClientRect();
    constraintRect = new win.DOMRect(constraintRect.left + win.mozInnerScreenX,
                                     constraintRect.top + win.mozInnerScreenY,
                                     constraintRect.width, constraintRect.height);
    menulist.menupopup.setConstraintRect(constraintRect);
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
        let popup = event.target;
        this._unregisterListeners(currentBrowser, popup);
        popup.parentNode.hidden = true;
        currentBrowser = null;
        currentMenulist = null;
        currentZoom = 1;
        break;
    }
  },

  receiveMessage(msg) {
    if (msg.name == "Forms:UpdateDropDown") {
      // Sanity check - we'd better know what the currently
      // opened menulist is, and what browser it belongs to...
      if (!currentMenulist || !currentBrowser) {
        return;
      }

      let options = msg.data.options;
      let selectedIndex = msg.data.selectedIndex;
      this.populate(currentMenulist, options, selectedIndex, currentZoom);
    }
  },

  _registerListeners: function(browser, popup) {
    popup.addEventListener("command", this);
    popup.addEventListener("popuphidden", this);
    popup.addEventListener("mouseover", this);
    popup.addEventListener("mouseout", this);
    browser.messageManager.addMessageListener("Forms:UpdateDropDown", this);
  },

  _unregisterListeners: function(browser, popup) {
    popup.removeEventListener("command", this);
    popup.removeEventListener("popuphidden", this);
    popup.removeEventListener("mouseover", this);
    popup.removeEventListener("mouseout", this);
    browser.messageManager.removeMessageListener("Forms:UpdateDropDown", this);
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

        // It's hack time. In the event that we've re-populated the menulist due
        // to a mutation in the <select> in content, that means that the -moz_activemenu
        // may have been removed from the selected item. Since that's normally only
        // set for the initially selected on popupshowing for the menulist, and we
        // don't want to close and re-open the popup, we manually set it here.
        menulist.menuBoxObject.activeChild = item;
      }

      item.setAttribute("value", option.index);

      if (isInGroup) {
        item.classList.add("contentSelectDropdown-ingroup")
      }
    }
  }
}
