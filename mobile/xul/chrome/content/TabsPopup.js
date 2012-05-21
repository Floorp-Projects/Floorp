/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TabsPopup = {
  init: function() {
    Elements.tabs.addEventListener("TabOpen", this, true);
    Elements.tabs.addEventListener("TabRemove", this, true);

    this._updateTabsCount();

    // Bind resizeHandler so we can pass it to addEventListener/removeEventListener.
    this.resizeHandler = this.resizeHandler.bind(this);
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

  get visible() {
    return !this.box.hidden;
  },

  toggle: function toggle() {
    if (this.visible)
      this.hide();
    else
      this.show();
  },

  show: function show() {
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

    window.addEventListener("resize", this.resizeHandler, false);
  },

  hide: function hide() {
    if (!this.box.hidden) {
      this.box.hidden = true;
      BrowserUI.popPopup(this);
      window.removeEventListener("resize", this.resizeHandler, false);
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

  resizeHandler: function resizeHandler(aEvent) {
    if (aEvent.target != window)
      return;
    if (!Util.isPortrait())
      this.hide();
  },

  handleEvent: function handleEvent(aEvent) {
    this._updateTabsCount();
  }
};
