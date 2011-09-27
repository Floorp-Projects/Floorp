/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var TabsPopup = {
  init: function() {
    Elements.tabs.addEventListener("TabOpen", this, true);
    Elements.tabs.addEventListener("TabRemove", this, true);

    this._updateTabsCount();
  },

  get box() {
    delete this.box;
    return this.box = document.getElementById("tabs-popup-container");
  },

  get list() {
    delete this.list;
    return this.list = document.getElementById("tabs-popup-list");
  },

  get button() {
    delete this.button;
    return this.button = document.getElementById("tool-tabs");
  },

  hide: function hide() {
    this._hidePortraitMenu();

    if (!Util.isPortrait()) {
      Elements.urlbarState.removeAttribute("tablet_sidebar");
      ViewableAreaObserver.update();
    }
  },

  show: function show() {
    if (!Util.isPortrait()) {
      Elements.urlbarState.setAttribute("tablet_sidebar", "true");
      ViewableAreaObserver.update();
      return;
    }

    while(this.list.firstChild)
      this.list.removeChild(this.list.firstChild);

    let tabs = Browser.tabs;
    tabs.forEach(function(aTab) {
      let item = document.createElement("richlistitem");
      item.className = "tab-popup-item";
      if (aTab.active)
        item.classList.add("selected");

      let browser = aTab.browser;
      let icon = browser.mIconURL;
      let caption = browser.contentTitle || browser.currentURI.spec;
      if (browser.__SS_restore) {
        /* if this is a zombie tab, pull the tab title/url out of session store data */
        let entry = browser.__SS_data.entries[0];
        caption = entry.title;

        let pageURI = Services.io.newURI(entry.url, null, null);
        try {
          let iconURI = gFaviconService.getFaviconImageForPage(pageURI);
          icon = iconURI.spec;
        } catch(ex) { }
      } else {
        if (caption == "about:blank")
          caption = browser.currentURI.spec;
        if (!icon) {
          let iconURI = gFaviconService.getFaviconImageForPage(browser.currentURI);
          icon = iconURI.spec;
        }
      }
      item.setAttribute("img", icon);
      item.setAttribute("label", caption);

      this.list.appendChild(item);
      item.tab = aTab;
    }, this)

    // Set the box position.
    this.box.hidden = false;
    this.box.anchorTo(this.button, "after_end");
    BrowserUI.pushPopup(this, [this.box, this.button]);

    window.addEventListener("resize", function resizeHandler(aEvent) {
      if (aEvent.target != window)
        return;
      if (!Util.isPortrait())
        TabsPopup._hidePortraitMenu();
    }, false);
  },

  toggle: function toggle() {
    if (this.visible)
      this.hide();
    else
      this.show();
  },

  get visible() {
    return Util.isPortrait() ? !this.box.hidden : Elements.urlbarState.hasAttribute("tablet_sidebar");
  },

  _hidePortraitMenu: function _hidePortraitMenu() {
    if (!this.box.hidden) {
      this.box.hidden = true;
      BrowserUI.popPopup(this);
      window.removeEventListener("resize", resizeHandler, false);
    }
  },

  closeTab: function(aTab) {
    messageManager.addMessageListener("Browser:CanUnload:Return", this.closeTabReturn.bind(this));
  },

  closeTabReturn: function(aMessage) {
    messageManager.removeMessageListener("Browser:CanUnload:Return", this.closeTabReturn.bind(this));

    if (!aMessage.json.permit)
      return;

    let removedTab = Browser.getTabForBrowser(aMessage.target);
    setTimeout(function(self) {
      let items = self.list.childNodes;
      let selected = Browser.selectedTab;
      for (let i = 0; i < items.length; i++) {
        if (items[i].tab == selected)
          items[i].classList.add("selected");
        else if (items[i].tab == removedTab)
          self.list.removeChild(items[i]);
      }
    }, 0, this);
  },

  _updateTabsCount: function() {
    let cmd = document.getElementById("cmd_showTabs");
    cmd.setAttribute("label", Browser.tabs.length);
  },

  handleEvent: function handleEvent(aEvent) {
    this._updateTabsCount();
  }
};
