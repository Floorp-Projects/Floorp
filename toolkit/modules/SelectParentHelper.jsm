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

// Minimum elements required to show select search
const SEARCH_MINIMUM_ELEMENTS = 40;

var currentBrowser = null;
var currentMenulist = null;
var currentZoom = 1;
var closedWithEnter = false;
var selectRect;
var customStylingEnabled = Services.prefs.getBoolPref("dom.forms.select.customstyling");
var usedSelectBackgroundColor;

this.SelectParentHelper = {
  populate(menulist, items, selectedIndex, zoom, uaBackgroundColor, uaColor,
           uaSelectBackgroundColor, uaSelectColor, selectBackgroundColor, selectColor,
           selectTextShadow) {
    // Clear the current contents of the popup
    menulist.menupopup.textContent = "";
    let stylesheet = menulist.querySelector("#ContentSelectDropdownScopedStylesheet");
    if (stylesheet) {
      stylesheet.remove();
    }

    let doc = menulist.ownerDocument;
    let sheet;
    if (customStylingEnabled) {
      stylesheet = doc.createElementNS("http://www.w3.org/1999/xhtml", "style");
      stylesheet.setAttribute("id", "ContentSelectDropdownScopedStylesheet");
      stylesheet.scoped = true;
      stylesheet.hidden = true;
      stylesheet = menulist.appendChild(stylesheet);
      sheet = stylesheet.sheet;
    }

    let ruleBody = "";

    // Some webpages set the <select> backgroundColor to transparent,
    // but they don't intend to change the popup to transparent.
    if (customStylingEnabled &&
        selectBackgroundColor != uaSelectBackgroundColor &&
        selectBackgroundColor != "rgba(0, 0, 0, 0)") {
      ruleBody = `background-image: linear-gradient(${selectBackgroundColor}, ${selectBackgroundColor});`;
      usedSelectBackgroundColor = selectBackgroundColor;
    } else {
      usedSelectBackgroundColor = uaSelectBackgroundColor;
    }

    if (customStylingEnabled &&
        selectColor != uaSelectColor &&
        selectColor != selectBackgroundColor &&
        (selectBackgroundColor != "rgba(0, 0, 0, 0)" ||
         selectColor != uaSelectBackgroundColor)) {
      ruleBody += `color: ${selectColor};`;
    }

    if (customStylingEnabled &&
        selectTextShadow != "none") {
      ruleBody += `text-shadow: ${selectTextShadow};`;
    }

    if (ruleBody) {
      sheet.insertRule(`menupopup {
        ${ruleBody}
      }`, 0);
      menulist.menupopup.setAttribute("customoptionstyling", "true");
    } else {
      menulist.menupopup.removeAttribute("customoptionstyling");
    }

    currentZoom = zoom;
    currentMenulist = menulist;
    populateChildren(menulist, items, selectedIndex, zoom,
                     uaBackgroundColor, uaColor, sheet);
  },

  open(browser, menulist, rect, isOpenedViaTouch) {
    menulist.hidden = false;
    currentBrowser = browser;
    closedWithEnter = false;
    selectRect = rect;
    this._registerListeners(browser, menulist.menupopup);

    let win = browser.ownerGlobal;

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
  },

  hide(menulist, browser) {
    if (currentBrowser == browser) {
      menulist.menupopup.hidePopup();
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "mouseup":
        function inRect(rect, x, y) {
          return x >= rect.left && x <= rect.left + rect.width && y >= rect.top && y <= rect.top + rect.height;
        }

        let x = event.screenX, y = event.screenY;
        let onAnchor = !inRect(currentMenulist.menupopup.getOuterScreenRect(), x, y) &&
                        inRect(selectRect, x, y) && currentMenulist.menupopup.state == "open";
        currentBrowser.messageManager.sendAsyncMessage("Forms:MouseUp", { onAnchor });
        break;

      case "mouseover":
        currentBrowser.messageManager.sendAsyncMessage("Forms:MouseOver", {});
        break;

      case "mouseout":
        currentBrowser.messageManager.sendAsyncMessage("Forms:MouseOut", {});
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
        popup.parentNode.hidden = true;
        currentBrowser = null;
        currentMenulist = null;
        currentZoom = 1;
        break;
    }
  },

  receiveMessage(msg) {
    if (!currentBrowser) {
      return;
    }

    if (msg.name == "Forms:UpdateDropDown") {
      // Sanity check - we'd better know what the currently
      // opened menulist is, and what browser it belongs to...
      if (!currentMenulist) {
        return;
      }

      let scrollBox = currentMenulist.menupopup.scrollBox;
      let scrollTop = scrollBox.scrollTop;

      let options = msg.data.options;
      let selectedIndex = msg.data.selectedIndex;
      let uaBackgroundColor = msg.data.uaBackgroundColor;
      let uaColor = msg.data.uaColor;
      let uaSelectBackgroundColor = msg.data.uaSelectBackgroundColor;
      let uaSelectColor = msg.data.uaSelectColor;
      let selectBackgroundColor = msg.data.selectBackgroundColor;
      let selectColor = msg.data.selectColor;
      let selectTextShadow = msg.data.selectTextShadow;
      this.populate(currentMenulist, options, selectedIndex,
                    currentZoom, uaBackgroundColor, uaColor,
                    uaSelectBackgroundColor, uaSelectColor,
                    selectBackgroundColor, selectColor, selectTextShadow);

      // Restore scroll position to what it was prior to the update.
      scrollBox.scrollTop = scrollTop;
    } else if (msg.name == "Forms:BlurDropDown-Ping") {
      currentBrowser.messageManager.sendAsyncMessage("Forms:BlurDropDown-Pong", {});
    }
  },

  _registerListeners(browser, popup) {
    popup.addEventListener("command", this);
    popup.addEventListener("popuphidden", this);
    popup.addEventListener("mouseover", this);
    popup.addEventListener("mouseout", this);
    browser.ownerGlobal.addEventListener("mouseup", this, true);
    browser.ownerGlobal.addEventListener("keydown", this, true);
    browser.ownerGlobal.addEventListener("fullscreen", this, true);
    browser.messageManager.addMessageListener("Forms:UpdateDropDown", this);
    browser.messageManager.addMessageListener("Forms:BlurDropDown-Ping", this);
  },

  _unregisterListeners(browser, popup) {
    popup.removeEventListener("command", this);
    popup.removeEventListener("popuphidden", this);
    popup.removeEventListener("mouseover", this);
    popup.removeEventListener("mouseout", this);
    browser.ownerGlobal.removeEventListener("mouseup", this, true);
    browser.ownerGlobal.removeEventListener("keydown", this, true);
    browser.ownerGlobal.removeEventListener("fullscreen", this, true);
    browser.messageManager.removeMessageListener("Forms:UpdateDropDown", this);
    browser.messageManager.removeMessageListener("Forms:BlurDropDown-Ping", this);
  },

};

function populateChildren(menulist, options, selectedIndex, zoom,
                          uaBackgroundColor, uaColor, sheet,
                          parentElement = null, isGroupDisabled = false,
                          adjustedTextSize = -1, addSearch = true, nthChildIndex = 1) {
  let element = menulist.menupopup;
  let win = element.ownerGlobal;

  // -1 just means we haven't calculated it yet. When we recurse through this function
  // we will pass in adjustedTextSize to save on recalculations.
  if (adjustedTextSize == -1) {
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

    let ruleBody = "";
    if (customStylingEnabled &&
        option.backgroundColor &&
        option.backgroundColor != "rgba(0, 0, 0, 0)" &&
        option.backgroundColor != usedSelectBackgroundColor) {
      ruleBody = `background-color: ${option.backgroundColor};`;
    }

    if (customStylingEnabled &&
        option.color &&
        option.color != uaColor) {
      ruleBody += `color: ${option.color};`;
    }

    if (customStylingEnabled &&
        option.textShadow) {
      ruleBody += `text-shadow: ${option.textShadow};`;
    }

    if (ruleBody) {
      sheet.insertRule(`menupopup > :nth-child(${nthChildIndex}):not([_moz-menuactive="true"]) {
        ${ruleBody}
      }`, 0);

      if (option.textShadow) {
        // Need to explicitly disable the possibly inherited
        // text-shadow rule when _moz-menuactive=true since
        // _moz-menuactive=true disables custom option styling.
        sheet.insertRule(`menupopup > :nth-child(${nthChildIndex})[_moz-menuactive="true"] {
          text-shadow: none;
        }`, 0);
      }

      item.setAttribute("customoptionstyling", "true");
    } else {
      item.removeAttribute("customoptionstyling");
    }

    element.appendChild(item);
    nthChildIndex++;

    // A disabled optgroup disables all of its child options.
    let isDisabled = isGroupDisabled || option.disabled;
    if (isDisabled) {
      item.setAttribute("disabled", "true");
    }

    if (isOptGroup) {
      nthChildIndex =
        populateChildren(menulist, option.children, selectedIndex, zoom,
                         uaBackgroundColor, uaColor, sheet,
                         item, isDisabled, adjustedTextSize, false, nthChildIndex);
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
    searchbox.addEventListener("command", onSearchInput);

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

  return nthChildIndex;
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
  currentBrowser.messageManager.sendAsyncMessage("Forms:SearchFocused", {});
}

function onSearchBlur() {
  let searchObj = this;
  let menupopup = searchObj.parentElement;
  menupopup.setAttribute("ignorekeys", "false");
}
