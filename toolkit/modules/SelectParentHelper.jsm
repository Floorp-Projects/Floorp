/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "SelectParentHelper",
];

const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Maximum number of rows to display in the select dropdown.
const MAX_ROWS = 20;

// Minimum elements required to show select search
const SEARCH_MINIMUM_ELEMENTS = 40;

// The properties that we should respect only when the item is not active.
const PROPERTIES_RESET_WHEN_ACTIVE = [
  "color",
  "background-color",
  "text-shadow",
];

// Make sure to clear these objects when the popup closes to avoid leaking.
var currentBrowser = null;
var currentMenulist = null;
var selectRect = null;

var currentZoom = 1;
var closedWithEnter = false;
var customStylingEnabled = Services.prefs.getBoolPref("dom.forms.select.customstyling");

var SelectParentHelper = {
  /**
   * `populate` takes the `menulist` element and a list of `items` and generates
   * a popup list of options.
   *
   * If `customStylingEnabled` is set to `true`, the function will alse
   * style the select and its popup trying to prevent the text
   * and background to end up in the same color.
   *
   * All `ua*` variables represent the color values for the default colors
   * for their respective form elements used by the user agent.
   * The `select*` variables represent the color values defined for the
   * particular <select> element.
   *
   * The `customoptionstyling` attribute controls the application of
   * `-moz-appearance` on the elements and is disabled if the element is
   * defining its own background-color.
   *
   * @param {Element}        menulist
   * @param {Array<Element>} items
   * @param {Array<Object>}  uniqueItemStyles
   * @param {Number}         selectedIndex
   * @param {Number}         zoom
   * @param {Object}         uaStyle
   * @param {Object}         selectStyle
   *
   * FIXME(emilio): injecting a stylesheet is a somewhat inefficient way to do
   * this, can we use more style attributes?
   *
   * FIXME(emilio, bug 1530709): At the very least we should use CSSOM to avoid
   * trusting the IPC message too much.
   */
  populate(menulist, items, uniqueItemStyles, selectedIndex, zoom,
           uaStyle, selectStyle) {
    // Clear the current contents of the popup
    menulist.menupopup.textContent = "";
    let stylesheet = menulist.querySelector("#ContentSelectDropdownStylesheet");
    if (stylesheet) {
      stylesheet.remove();
    }

    let doc = menulist.ownerDocument;
    let sheet;
    if (customStylingEnabled) {
      stylesheet = doc.createElementNS("http://www.w3.org/1999/xhtml", "style");
      stylesheet.setAttribute("id", "ContentSelectDropdownStylesheet");
      stylesheet.hidden = true;
      stylesheet = menulist.appendChild(stylesheet);
      sheet = stylesheet.sheet;
    } else {
      selectStyle = uaStyle;
    }

    let selectBackgroundSet = false;

    if (selectStyle["background-color"] == "rgba(0, 0, 0, 0)") {
      selectStyle["background-color"] = uaStyle["background-color"];
    }

    // Some webpages set the <select> backgroundColor to transparent,
    // but they don't intend to change the popup to transparent.
    if (customStylingEnabled &&
        selectStyle["background-color"] != uaStyle["background-color"]) {
      let color = selectStyle["background-color"];
      selectStyle["background-image"] = `linear-gradient(${color}, ${color});`;
      selectBackgroundSet = true;
    }

    if (selectStyle.color == selectStyle["background-color"]) {
      selectStyle.color = uaStyle.color;
    }

    if (customStylingEnabled) {
      if (selectStyle["text-shadow"] != "none") {
        sheet.insertRule(`#ContentSelectDropdown > menupopup > [_moz-menuactive="true"] {
          text-shadow: none;
        }`, 0);
      }

      let ruleBody = "";
      for (let property in selectStyle) {
        if (property == "background-color" || property == "direction")
          continue; // Handled above, or before.
        if (selectStyle[property] != uaStyle[property]) {
          ruleBody += `${property}: ${selectStyle[property]};`;
        }
      }
      if (ruleBody) {
        sheet.insertRule(`#ContentSelectDropdown > menupopup {
          ${ruleBody}
        }`, 0);
        sheet.insertRule(`#ContentSelectDropdown > menupopup > :not([_moz-menuactive="true"]) {
           color: inherit;
        }`, 0);
      }
    }

    // We only set the `customoptionstyling` if the background has been
    // manually set. This prevents the overlap between moz-appearance and
    // background-color. `color` and `text-shadow` do not interfere with it.
    if (selectBackgroundSet) {
      menulist.menupopup.setAttribute("customoptionstyling", "true");
    } else {
      menulist.menupopup.removeAttribute("customoptionstyling");
    }

    currentZoom = zoom;
    currentMenulist = menulist;
    populateChildren(menulist, items, uniqueItemStyles, selectedIndex, zoom,
                     selectStyle, selectBackgroundSet, sheet);
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
    let firstItem = menupopup.firstElementChild;
    while (firstItem && firstItem.hidden) {
      firstItem = firstItem.nextElementSibling;
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

    if (browser.getAttribute("selectmenuconstrained") != "false") {
      let constraintRect = browser.getBoundingClientRect();
      constraintRect = new win.DOMRect(constraintRect.left + win.mozInnerScreenX,
                                       constraintRect.top + win.mozInnerScreenY,
                                       constraintRect.width, constraintRect.height);
      menupopup.setConstraintRect(constraintRect);
    } else {
      menupopup.setConstraintRect(new win.DOMRect(0, 0, 0, 0));
    }
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
            closedWithEnter,
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
        selectRect = null;
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

      let scrollBox = currentMenulist.menupopup.scrollBox.scrollbox;
      let scrollTop = scrollBox.scrollTop;

      let options = msg.data.options;
      let selectedIndex = msg.data.selectedIndex;
      this.populate(currentMenulist, options.options, options.uniqueStyles,
                    selectedIndex, currentZoom, msg.data.defaultStyle,
                    msg.data.style);

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

/**
 * `populateChildren` creates all <menuitem> elements for the popup menu
 * based on the list of <option> elements from the <select> element.
 *
 * It attempts to intelligently add per-item CSS rules if the single
 * item values differ from the parent menu values and attempting to avoid
 * ending up with the same color of text and background.
 *
 * @param {Element}        menulist
 * @param {Array<Element>} options
 * @param {Array<Object>}  uniqueOptionStyles
 * @param {Number}         selectedIndex
 * @param {Number}         zoom
 * @param {Object}         selectStyle
 * @param {Boolean}        selectBackgroundSet
 * @param {CSSStyleSheet}  sheet
 * @param {Element}        parentElement
 * @param {Boolean}        isGroupDisabled
 * @param {Boolean}        addSearch
 * @param {Number}         nthChildIndex
 * @returns {Number}
 *
 * FIXME(emilio): Again, using a stylesheet + :nth-child is not really efficient.
 */
function populateChildren(menulist, options, uniqueOptionStyles, selectedIndex,
                          zoom, selectStyle, selectBackgroundSet, sheet,
                          parentElement = null, isGroupDisabled = false,
                          addSearch = true, nthChildIndex = 1) {
  let element = menulist.menupopup;

  for (let option of options) {
    let isOptGroup = (option.tagName == "OPTGROUP");
    let item = element.ownerDocument.createXULElement(isOptGroup ? "menucaption" : "menuitem");
    let style = uniqueOptionStyles[option.styleIndex];

    item.setAttribute("label", option.textContent);
    item.style.direction = style.direction;
    item.style.fontSize = (zoom * parseFloat(style["font-size"], 10)) + "px";
    item.hidden = option.display == "none" || (parentElement && parentElement.hidden);
    // Keep track of which options are hidden by page content, so we can avoid showing
    // them on search input
    item.hiddenByContent = item.hidden;
    item.setAttribute("tooltiptext", option.tooltip);

    if (style["background-color"] == "rgba(0, 0, 0, 0)") {
      style["background-color"] = selectStyle["background-color"];
    }

    let optionBackgroundSet = style["background-color"] != selectStyle["background-color"];

    if (style.color == style["background-color"]) {
      style.color = selectStyle.color;
    }

    if (customStylingEnabled) {
      let ruleBody = "";
      for (let property in style) {
        if (property == "direction" || property == "font-size")
          continue; // handled above
        if (style[property] == selectStyle[property])
          continue;
        if (PROPERTIES_RESET_WHEN_ACTIVE.includes(property)) {
          ruleBody += `${property}: ${style[property]};`;
        } else {
          item.style.setProperty(property, style[property]);
        }
      }

      if (ruleBody) {
        sheet.insertRule(`#ContentSelectDropdown > menupopup > :nth-child(${nthChildIndex}):not([_moz-menuactive="true"]) {
          ${ruleBody}
        }`, 0);

        if (style["text-shadow"] != "none" &&
            style["text-shadow"] != selectStyle["text-shadow"]) {
          // Need to explicitly disable the possibly inherited
          // text-shadow rule when _moz-menuactive=true since
          // _moz-menuactive=true disables custom option styling.
          sheet.insertRule(`#ContentSelectDropdown > menupopup > :nth-child(${nthChildIndex})[_moz-menuactive="true"] {
            text-shadow: none;
          }`, 0);
        }
      }
    }

    if (customStylingEnabled && (optionBackgroundSet || selectBackgroundSet)) {
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
        populateChildren(menulist, option.children, uniqueOptionStyles,
                         selectedIndex, zoom, selectStyle,
                         selectBackgroundSet, sheet, item, isDisabled, false,
                         nthChildIndex);
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
        menulist.activeChild = item;
      }

      item.setAttribute("value", option.index);

      if (parentElement) {
        item.classList.add("contentSelectDropdown-ingroup");
      }
    }
  }

  // Check if search pref is enabled, if this is the first time iterating through
  // the dropdown, and if the list is long enough for a search element to be added.
  if (Services.prefs.getBoolPref("dom.forms.selectSearch") && addSearch
      && element.childElementCount > SEARCH_MINIMUM_ELEMENTS) {
    // Add a search text field as the first element of the dropdown
    let searchbox = element.ownerDocument.createXULElement("textbox", {
      is: "search-textbox",
    });
    searchbox.className = "contentSelectDropdown-searchbox";
    searchbox.addEventListener("input", onSearchInput);
    searchbox.inputField.addEventListener("focus", onSearchFocus);
    searchbox.inputField.addEventListener("blur", onSearchBlur);
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
          if (searchbox.nextElementSibling.localName == "menuitem" &&
              !searchbox.nextElementSibling.hidden) {
            menulist.activeChild = searchbox.nextElementSibling;
          } else {
            var currentOption = searchbox.nextElementSibling;
            while (currentOption && (currentOption.localName != "menuitem" ||
                  currentOption.hidden)) {
              currentOption = currentOption.nextElementSibling;
            }
            if (currentOption) {
              menulist.activeChild = currentOption;
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

    element.insertBefore(searchbox, element.children[0]);
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
            currentItem.previousElementSibling.classList.contains("contentSelectDropdown-ingroup")) {
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
  let menupopup = searchObj.closest("menupopup");
  menupopup.parentElement.activeChild = null;
  menupopup.setAttribute("ignorekeys", "true");
  currentBrowser.messageManager.sendAsyncMessage("Forms:SearchFocused", {});
}

function onSearchBlur() {
  let searchObj = this;
  let menupopup = searchObj.closest("menupopup");
  menupopup.setAttribute("ignorekeys", "false");
}
