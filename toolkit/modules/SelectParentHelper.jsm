/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SelectParentHelper"
];

const {utils: Cu} = Components;
const {AppConstants} = Cu.import("resource://gre/modules/AppConstants.jsm", {});
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

// Maximum number of rows to display in the select dropdown.
const MAX_ROWS = 20;

// Interval between autoscrolls
const AUTOSCROLL_INTERVAL = 25;

// Minimum elements required to show select search
const SEARCH_MINIMUM_ELEMENTS = 40;

var currentBrowser = null;
var currentMenulist = null;
var currentZoom = 1;
var closedWithEnter = false;

this.SelectParentHelper = {
  draggedOverPopup: false,
  scrollTimer: 0,

  populate(menulist, items, selectedIndex, zoom) {
    // Clear the current contents of the popup
    menulist.menupopup.textContent = "";
    currentZoom = zoom;
    currentMenulist = menulist;
    populateChildren(menulist, items, selectedIndex, zoom);
  },

  open(browser, menulist, rect, isOpenedViaTouch) {
    menulist.hidden = false;
    currentBrowser = browser;
    closedWithEnter = false;
    this._registerListeners(browser, menulist.menupopup);

    let win = browser.ownerDocument.defaultView;

    // Set the maximum height to show exactly MAX_ROWS items.
    let menupopup = menulist.menupopup;
    let firstItem = menupopup.firstChild;
    while (firstItem && firstItem.hidden) {
      firstItem = firstItem.nextSibling;
    }

    if (firstItem) {
      let itemHeight = firstItem.getBoundingClientRect().height;

      // Include the padding and border on the popup.
      let cs = win.getComputedStyle(menupopup);
      let bpHeight = parseFloat(cs.borderTopWidth) + parseFloat(cs.borderBottomWidth) +
                     parseFloat(cs.paddingTop) + parseFloat(cs.paddingBottom);
      menupopup.style.maxHeight = (itemHeight * MAX_ROWS + bpHeight) + "px";
    }

    menupopup.classList.toggle("isOpenedViaTouch", isOpenedViaTouch);

    let constraintRect = browser.getBoundingClientRect();
    constraintRect = new win.DOMRect(constraintRect.left + win.mozInnerScreenX,
                                     constraintRect.top + win.mozInnerScreenY,
                                     constraintRect.width, constraintRect.height);
    menupopup.setConstraintRect(constraintRect);
    menupopup.openPopupAtScreenRect(AppConstants.platform == "macosx" ? "selection" : "after_start", rect.left, rect.top, rect.width, rect.height, false, false);

    // Set up for dragging
    menupopup.setCaptureAlways();
    this.draggedOverPopup = false;
    menupopup.addEventListener("mousemove", this);
  },

  hide(menulist, browser) {
    if (currentBrowser == browser) {
      menulist.menupopup.hidePopup();
    }
  },

  clearScrollTimer() {
    if (this.scrollTimer) {
      let win = currentBrowser.ownerDocument.defaultView;
      win.clearInterval(this.scrollTimer);
      this.scrollTimer = 0;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "mouseup":
        this.clearScrollTimer();
        currentMenulist.menupopup.removeEventListener("mousemove", this);
        currentBrowser.messageManager.sendAsyncMessage("Forms:MouseUp", {});
        break;

      case "mouseover":
        currentBrowser.messageManager.sendAsyncMessage("Forms:MouseOver", {});
        break;

      case "mouseout":
        currentBrowser.messageManager.sendAsyncMessage("Forms:MouseOut", {});
        break;

      case "mousemove":
        let menupopup = currentMenulist.menupopup;
        let popupRect = menupopup.getOuterScreenRect();

        this.clearScrollTimer();

        // If dragging outside the top or bottom edge of the popup, but within
        // the popup area horizontally, scroll the list in that direction. The
        // draggedOverPopup flag is used to ensure that scrolling does not start
        // until the mouse has moved over the popup first, preventing scrolling
        // while over the dropdown button.
        if (event.screenX >= popupRect.left && event.screenX <= popupRect.right) {
          if (!this.draggedOverPopup) {
            if (event.screenY > popupRect.top && event.screenY < popupRect.bottom) {
              this.draggedOverPopup = true;
            }
          }

          if (this.draggedOverPopup &&
              (event.screenY <= popupRect.top || event.screenY >= popupRect.bottom)) {
            let scrollAmount = event.screenY <= popupRect.top ? -1 : 1;
            menupopup.scrollBox.scrollByIndex(scrollAmount);

            let win = currentBrowser.ownerDocument.defaultView;
            this.scrollTimer = win.setInterval(function() {
              menupopup.scrollBox.scrollByIndex(scrollAmount);
            }, AUTOSCROLL_INTERVAL);
          }
        }

        break;

      case "keydown":
        if (event.keyCode == event.DOM_VK_RETURN) {
          closedWithEnter = true;
        }
        break;

      case "command":
        if (event.target.hasAttribute("value")) {
          currentBrowser.messageManager.sendAsyncMessage("Forms:SelectDropDownItem", {
            value: event.target.value,
            closedWithEnter
          });
        }
        break;

      case "fullscreen":
        if (currentMenulist) {
          currentMenulist.menupopup.hidePopup();
        }
        break;

      case "popuphidden":
        currentBrowser.messageManager.sendAsyncMessage("Forms:DismissedDropDown", {});
        let popup = event.target;
        this._unregisterListeners(currentBrowser, popup);
        this.clearScrollTimer();
        popup.releaseCapture();
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

  _registerListeners(browser, popup) {
    popup.addEventListener("command", this);
    popup.addEventListener("popuphidden", this);
    popup.addEventListener("mouseover", this);
    popup.addEventListener("mouseout", this);
    browser.ownerDocument.defaultView.addEventListener("mouseup", this, true);
    browser.ownerDocument.defaultView.addEventListener("keydown", this, true);
    browser.ownerDocument.defaultView.addEventListener("fullscreen", this, true);
    browser.messageManager.addMessageListener("Forms:UpdateDropDown", this);
  },

  _unregisterListeners(browser, popup) {
    popup.removeEventListener("command", this);
    popup.removeEventListener("popuphidden", this);
    popup.removeEventListener("mouseover", this);
    popup.removeEventListener("mouseout", this);
    browser.ownerDocument.defaultView.removeEventListener("mouseup", this, true);
    browser.ownerDocument.defaultView.removeEventListener("keydown", this, true);
    browser.ownerDocument.defaultView.removeEventListener("fullscreen", this, true);
    browser.messageManager.removeMessageListener("Forms:UpdateDropDown", this);
  },

};

function populateChildren(menulist, options, selectedIndex, zoom,
                          parentElement = null, isGroupDisabled = false,
                          adjustedTextSize = -1, addSearch = true) {
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
    let isOptGroup = (option.tagName == "OPTGROUP");
    let item = element.ownerDocument.createElement(isOptGroup ? "menucaption" : "menuitem");

    item.setAttribute("label", option.textContent);
    item.style.direction = option.textDirection;
    item.style.fontSize = adjustedTextSize;
    item.hidden = option.display == "none" || (parentElement && parentElement.hidden);
    // Keep track of which options are hidden by page content, so we can avoid showing
    // them on search input
    item.hiddenByContent = item.hidden;
    item.setAttribute("tooltiptext", option.tooltip);

    element.appendChild(item);

    // A disabled optgroup disables all of its child options.
    let isDisabled = isGroupDisabled || option.disabled;
    if (isDisabled) {
      item.setAttribute("disabled", "true");
    }

    if (isOptGroup) {
      populateChildren(menulist, option.children, selectedIndex, zoom,
                       item, isDisabled, adjustedTextSize, false);
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

      if (parentElement) {
        item.classList.add("contentSelectDropdown-ingroup")
      }
    }
  }

  // Check if search pref is enabled, if this is the first time iterating through
  // the dropdown, and if the list is long enough for a search element to be added.
  if (Services.prefs.getBoolPref("dom.forms.selectSearch") && addSearch
      && element.childElementCount > SEARCH_MINIMUM_ELEMENTS) {

    // Add a search text field as the first element of the dropdown
    let searchbox = element.ownerDocument.createElement("textbox");
    searchbox.setAttribute("type", "search");
    searchbox.addEventListener("input", onSearchInput);
    searchbox.addEventListener("focus", onSearchFocus);
    searchbox.addEventListener("blur", onSearchBlur);

    // Handle special keys for exiting search
    searchbox.addEventListener("keydown", function(event) {
      if (event.defaultPrevented) {
        return;
      }
      switch (event.key) {
        case "Escape":
          searchbox.parentElement.hidePopup();
          break;
        case "ArrowDown":
        case "Enter":
        case "Tab":
          searchbox.blur();
          if (searchbox.nextSibling.localName == "menuitem" &&
              !searchbox.nextSibling.hidden) {
            menulist.menuBoxObject.activeChild = searchbox.nextSibling;
          } else {
            var currentOption = searchbox.nextSibling;
            while (currentOption && (currentOption.localName != "menuitem" ||
                  currentOption.hidden)) {
              currentOption = currentOption.nextSibling;
            }
            if (currentOption) {
              menulist.menuBoxObject.activeChild = currentOption;
            } else {
              searchbox.focus();
            }
          }
          break;
        default:
          return;
      }
      event.preventDefault();
    }, true);

    element.insertBefore(searchbox, element.childNodes[0]);
  }

}

function onSearchInput() {
  let searchObj = this;

  // Get input from search field, set to all lower case for comparison
  let input = searchObj.value.toLowerCase();
  // Get all items in dropdown (could be options or optgroups)
  let menupopup = searchObj.parentElement;
  let menuItems = menupopup.querySelectorAll("menuitem, menucaption");

  // Flag used to detect any group headers with no visible options.
  // These group headers should be hidden.
  let allHidden = true;
  // Keep a reference to the previous group header (menucaption) to go back
  // and set to hidden if all options within are hidden.
  let prevCaption = null;

  for (let currentItem of menuItems) {
    // Make sure we don't show any options that were hidden by page content
    if (!currentItem.hiddenByContent) {
      // Get label and tooltip (title) from option and change to
      // lower case for comparison
      let itemLabel = currentItem.getAttribute("label").toLowerCase();
      let itemTooltip = currentItem.getAttribute("title").toLowerCase();

      // If search input is empty, all options should be shown
      if (!input) {
        currentItem.hidden = false;
      } else if (currentItem.localName == "menucaption") {
        if (prevCaption != null) {
          prevCaption.hidden = allHidden;
        }
        prevCaption = currentItem;
        allHidden = true;
      } else {
        if (!currentItem.classList.contains("contentSelectDropdown-ingroup") &&
            currentItem.previousSibling.classList.contains("contentSelectDropdown-ingroup")) {
          if (prevCaption != null) {
            prevCaption.hidden = allHidden;
          }
          prevCaption = null;
          allHidden = true;
        }
        if (itemLabel.includes(input) || itemTooltip.includes(input)) {
          currentItem.hidden = false;
          allHidden = false;
        } else {
          currentItem.hidden = true;
        }
      }
      if (prevCaption != null) {
        prevCaption.hidden = allHidden;
      }
    }
  }
}

function onSearchFocus() {
  let searchObj = this;
  let menupopup = searchObj.parentElement;
  menupopup.parentElement.menuBoxObject.activeChild = null;
  menupopup.setAttribute("ignorekeys", "true");
}

function onSearchBlur() {
  let searchObj = this;
  let menupopup = searchObj.parentElement;
  menupopup.setAttribute("ignorekeys", "false");
}
