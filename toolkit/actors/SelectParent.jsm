/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["SelectParent"];

ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

class SelectParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "Forms:ShowDropDown":
      {
        let topBrowsingContext = this.manager.browsingContext.top;
        let browser = topBrowsingContext.embedderElement;

        if (!browser.hasAttribute("selectmenulist")) {
          return;
        }

        let document = browser.ownerDocument;
        let menulist = document.getElementById(browser.getAttribute("selectmenulist"));

        if (!this._menulist) {
          // Cache the menulist to have access to it
          // when the document is gone (eg: Tab closed)
          this._menulist = menulist;
        }

        let data = message.data;
        menulist.menupopup.style.direction = data.style.direction;

        let useFullZoom = !browser.isRemoteBrowser ||
                          Services.prefs.getBoolPref("browser.zoom.full") ||
                          browser.isSyntheticDocument;
        let zoom = useFullZoom ? browser._fullZoom : browser._textZoom;

        if (!this._selectParentHelper) {
          this._selectParentHelper =
            ChromeUtils.import("resource://gre/modules/SelectParentHelper.jsm", {})
                       .SelectParentHelper;
        }

        this._selectParentHelper.populate(menulist, data.options.options,
          data.options.uniqueStyles, data.selectedIndex, zoom,
          data.defaultStyle, data.style);
        this._selectParentHelper.open(browser, menulist, data.rect, data.isOpenedViaTouch, this);
        break;
      }

      case "Forms:HideDropDown":
        {
          if (this._selectParentHelper) {
            let topBrowsingContext = this.manager.browsingContext.top;
            let browser = topBrowsingContext.embedderElement;
            this._selectParentHelper.hide(this._menulist, browser);
          }
          break;
        }

      default:
        if (this._selectParentHelper) {
          this._selectParentHelper.receiveMessage(message);
        }
    }
  }
}
