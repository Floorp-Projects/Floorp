var AwesomeScreen = {
  offset: 0.25, // distance the popup will be offset from the left side of the window
  get headers() {
    delete this.headers;
    return this.headers = document.getElementById("awesome-header");
  },

  get container() {
    delete this.container;
    return this.container = document.getElementById("awesome-panels");
  },

  _activePanel: null,
  get activePanel() {
    return this._activePanel;
  },

  get _targets() {
    delete this._targets;
    return this._targets = [
      this.container,
#ifdef MOZ_SERVICES_SYNC
      document.getElementById("syncsetup-container"),
#endif
      document.getElementById("urlbar-container"),
      document.getElementById("tool-app-close"),
      document.getElementById("search-engines-popup"),
      document.getElementById("context-popup")
    ]
  },

  _popupShowing: false,
  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "PopupChanged" :
        this._popupShowing = (aEvent.detail != null);
        break;
      case "TapDown" :
        // If a popup has been shown on top of the active
        // awesome panel (e.g. context menu), the panel should
        // not be dismissed on TapDown.
        if (this._popupShowing)
          break;

        let target = aEvent.target;
        while (target && this._targets.indexOf(target) == -1)
          target = target.parentNode;
        if (!target)
          this.activePanel = null;
        break;
    }
  },

  set activePanel(aPanel) {
    if (this._activePanel == aPanel)
      return;

    let willShowPanel = (!this._activePanel && aPanel);
    if (willShowPanel) {
      BrowserUI.pushDialog(aPanel);
      BrowserUI._edit.attachController();
      BrowserUI._editURI();
      this.container.hidden = this.headers.hidden = false;
      window.addEventListener("TapDown", this, false);
      window.addEventListener("PopupChanged", this, false);
    }

    if (aPanel) {
      aPanel.open();
      if (BrowserUI._edit.value == "")
        BrowserUI._showURI();
    }

    let willHidePanel = (this._activePanel && !aPanel);
    if (willHidePanel) {
      this.container.hidden = true;
      this.headers.hidden = false;
      BrowserUI._edit.reset();
      BrowserUI._edit.detachController();
      BrowserUI.popDialog();
      window.removeEventListener("TapDown", this, false);
      window.removeEventListener("PopupChanged", this, false);
    }

    if (this._activePanel)
      this._activePanel.close();

    // If the keyboard will cover the full screen, we do not want to show it right away.
    let isReadOnly = (aPanel != AllPagesList || BrowserUI._isKeyboardFullscreen() || (!willShowPanel && BrowserUI._edit.readOnly));
    BrowserUI._edit.readOnly = isReadOnly;
    if (isReadOnly)
      BrowserUI._edit.blur();

    this._activePanel = aPanel;
    if (willHidePanel || willShowPanel) {
      let event = document.createEvent("UIEvents");
      event.initUIEvent("NavigationPanel" + (willHidePanel ? "Hidden" : "Shown"), true, true, window, false);
      window.dispatchEvent(event);
    }

    // maemo can change what is shown in the urlbar. we need to resize to ensure
    // that the awesomescreen and urlbar are the same width
    this.doResize(ViewableAreaObserver.width, ViewableAreaObserver.height);
  },

  updateHeader: function updateHeader(aString) {
    this.headers.hidden = (aString != "");

    // During an awesome search we always show the popup_autocomplete/AllPagesList
    // panel since this looks in every places and the rationale behind typing
    // is to find something, whereever it is.
    if (this.activePanel != AllPagesList) {
      let inputField = BrowserUI._edit;
      let oldClickSelectsAll = inputField.clickSelectsAll;
      inputField.clickSelectsAll = false;

      this.activePanel = AllPagesList;

      // changing the searchString property call updateAwesomeHeader again
      inputField.controller.searchString = aString;
      inputField.readOnly = false;
      inputField.clickSelectsAll = oldClickSelectsAll;
      return;
    }

    let event = document.createEvent("Events");
    event.initEvent("onsearchbegin", true, true);
    BrowserUI._edit.dispatchEvent(event);
  },

  doResize: function(aWidth, aHeight) {
    // awesomebar and related panels
    let popup = document.getElementById("awesome-panels");
    popup.top = BrowserUI.toolbarH;

    if (Util.isTablet()) {
      let urlbar = document.getElementById("urlbar-container");
      let urlbarRect = urlbar.getBoundingClientRect();
      let toolbar = document.getElementById("toolbar-main");
      let toolbarRect = toolbar.getBoundingClientRect();

      let dpi = Util.displayDPI;
      let minHeight = 500;
      if (dpi > 96)
        minHeight = 3*dpi;

      if (Util.localeDir > 0) {
        popup.left = toolbarRect.left + this.offset*dpi;
        popup.width = urlbarRect.right - toolbarRect.left - this.offset*dpi;
      } else {
        popup.left = urlbarRect.left;
        popup.width = toolbarRect.right - urlbarRect.left - this.offset*dpi;
      }
      popup.height = Math.min(aHeight - BrowserUI.toolbarH, minHeight);
    } else {
      popup.width = aWidth;
      popup.height = aHeight - BrowserUI.toolbarH;
      popup.left = 0;
    }
  }
}

var AwesomePanel = function(aElementId, aCommandId) {
  let command = document.getElementById(aCommandId);

  this.panel = document.getElementById(aElementId),

  this.open = function aw_open() {
    command.setAttribute("checked", "true");
    this.panel.hidden = false;

    if (this.panel.hasAttribute("onshow")) {
      let func = new Function("panel", this.panel.getAttribute("onshow"));
      func.call(this.panel);
    }

    if (this.panel.open)
      this.panel.open();
  },

  this.close = function aw_close() {
    if (this.panel.hasAttribute("onhide")) {
      let func = new Function("panel", this.panel.getAttribute("onhide"));
      func.call(this.panel);
    }

    if (this.panel.close)
      this.panel.close();

    this.panel.hidden = true;
    command.removeAttribute("checked");
  },

  this.doCommand = function aw_doCommand() {
    BrowserUI.doCommand(aCommandId);
  },

  this.openLink = function aw_openLink(aEvent) {
    let item = aEvent.originalTarget;
    let uri = item.getAttribute("url") || item.getAttribute("uri");
    if (uri != "") {
      Browser.selectedBrowser.userTypedValue = uri;
      BrowserUI.goToURI(uri);
    }
  }
};
