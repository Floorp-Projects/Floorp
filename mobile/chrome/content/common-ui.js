var AlertsHelper = {
  _timeoutID: -1,
  _listener: null,
  _cookie: "",
  _clickable: false,
  get container() {
    delete this.container;
    let container = document.getElementById("alerts-container");

    // Move the popup on the other side if we are in RTL
    let [leftSidebar, rightSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    if (leftSidebar.left > rightSidebar.left) {
      container.removeAttribute("right");
      container.setAttribute("left", "0");
    }

    let self = this;
    container.addEventListener("transitionend", function() {
      self.alertTransitionOver();
    }, true);

    return this.container = container;
  },

  showAlertNotification: function ah_show(aImageURL, aTitle, aText, aTextClickable, aCookie, aListener) {
    this._clickable = aTextClickable || false;
    this._listener = aListener || null;
    this._cookie = aCookie || "";

    document.getElementById("alerts-image").setAttribute("src", aImageURL);
    document.getElementById("alerts-title").value = aTitle;
    document.getElementById("alerts-text").textContent = aText;

    let container = this.container;
    container.hidden = false;
    container.height = container.getBoundingClientRect().height;
    container.classList.add("showing");

    let timeout = Services.prefs.getIntPref("alerts.totalOpenTime");
    let self = this;
    if (this._timeoutID)
      clearTimeout(this._timeoutID);
    this._timeoutID = setTimeout(function() { self._timeoutAlert(); }, timeout);
  },

  _timeoutAlert: function ah__timeoutAlert() {
    this._timeoutID = -1;

    this.container.classList.remove("showing");
    if (this._listener)
      this._listener.observe(null, "alertfinished", this._cookie);
  },

  alertTransitionOver: function ah_alertTransitionOver() {
    let container = this.container;
    if (!container.classList.contains("showing")) {
      container.height = 0;
      container.hidden = true;
    }
  },

  click: function ah_click(aEvent) {
    if (this._clickable && this._listener)
      this._listener.observe(null, "alertclickcallback", this._cookie);

    if (this._timeoutID != -1) {
      clearTimeout(this._timeoutID);
      this._timeoutAlert();
    }
  }
};

var BrowserSearch = {
  get _popup() {
    delete this._popup;
    return this._popup = document.getElementById("search-engines-popup");
  },

  get _list() {
    delete this._list;
    return this._list = document.getElementById("search-engines-list");
  },

  get _button() {
    delete this._button;
    return this._button = document.getElementById("tool-search");
  },

  toggle: function bs_toggle() {
    if (this._popup.hidden)
      this.show();
    else
      this.hide();
  },

  show: function bs_show() {
    let popup = this._popup;
    let list = this._list;
    while (list.lastChild)
      list.removeChild(list.lastChild);

    this.engines.forEach(function(aEngine) {
      let button = document.createElement("button");
      button.className = "prompt-button";
      button.setAttribute("label", aEngine.name);
      button.setAttribute("crop", "end");
      button.setAttribute("pack", "start");
      button.setAttribute("image", aEngine.iconURI ? aEngine.iconURI.spec : null);
      button.onclick = function() {
        popup.hidden = true;
        BrowserUI.doOpenSearch(aEngine.name);
      }
      list.appendChild(button);
    });

    popup.hidden = false;
    popup.top = BrowserUI.toolbarH - popup.offset;
    popup.anchorTo(document.getElementById("tool-search"));

    document.getElementById("urlbar-icons").setAttribute("open", "true");
    BrowserUI.pushPopup(this, [popup, this._button]);
  },

  hide: function bs_hide() {
    this._popup.hidden = true;
    document.getElementById("urlbar-icons").removeAttribute("open");
    BrowserUI.popPopup(this);
  },

  observe: function bs_observe(aSubject, aTopic, aData) {
    if (aTopic != "browser-search-engine-modified")
      return;

    switch (aData) {
      case "engine-added":
      case "engine-removed":
        // force a rebuild of the prefs list, if needed
        // XXX this is inefficient, shouldn't have to rebuild the entire list
        if (ExtensionsView._list)
          ExtensionsView.getAddonsFromLocal();

        // fall through
      case "engine-changed":
        // XXX we should probably also update the ExtensionsView list here once
        // that's efficient, since the icon can change (happen during an async
        // installs from the web)

        // blow away our cache
        this._engines = null;
        break;
      case "engine-current":
        // Not relevant
        break;
    }
  },

  get engines() {
    if (this._engines)
      return this._engines;

    this._engines = Services.search.getVisibleEngines({ });
    return this._engines;
  },

  updatePageSearchEngines: function updatePageSearchEngines(aNode) {
    let items = Browser.selectedBrowser.searchEngines.filter(this.isPermanentSearchEngine);
    if (!items.length)
      return false;

    // XXX limit to the first search engine for now
    let engine = items[0];
    aNode.setAttribute("description", engine.title);
    aNode.onclick = function(aEvent) {
      BrowserSearch.addPermanentSearchEngine(engine);
      PageActions.hideItem(aNode);
      aEvent.stopPropagation(); // Don't hide the site menu.
    };
    return true;
  },

  addPermanentSearchEngine: function addPermanentSearchEngine(aEngine) {
    let iconURL = BrowserUI._favicon.src;
    Services.search.addEngine(aEngine.href, Ci.nsISearchEngine.DATA_XML, iconURL, false);

    this._engines = null;
  },

  isPermanentSearchEngine: function isPermanentSearchEngine(aEngine) {
    return !BrowserSearch.engines.some(function(item) {
      return aEngine.title == item.name;
    });
  }
};

var PageActions = {
  init: function init() {
    document.getElementById("pageactions-container").addEventListener("click", this, true);

    this.register("pageaction-reset", this.updatePagePermissions, this);
    this.register("pageaction-password", this.updateForgetPassword, this);
#ifdef NS_PRINTING
    this.register("pageaction-saveas", this.updatePageSaveAs, this);
#endif
    this.register("pageaction-share", this.updateShare, this);
    this.register("pageaction-search", BrowserSearch.updatePageSearchEngines, BrowserSearch);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        getIdentityHandler().hide();
        break;
    }
  },

  /**
   * @param aId id of a pageaction element
   * @param aCallback function that takes an element and returns true if it should be visible
   * @param aThisObj (optional) scope object for aCallback
   */
  register: function register(aId, aCallback, aThisObj) {
    this._handlers.push({id: aId, callback: aCallback, obj: aThisObj});
  },

  _handlers: [],

  updateSiteMenu: function updateSiteMenu() {
    this._handlers.forEach(function(action) {
      let node = document.getElementById(action.id);
      node.hidden = !action.callback.call(action.obj, node);
    });
    this._updateAttributes();
  },

  get _loginManager() {
    delete this._loginManager;
    return this._loginManager = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  },

  // This is easy for an addon to add his own perm type here
  _permissions: ["popup", "offline-app", "geo"],

  _forEachPermissions: function _forEachPermissions(aHost, aCallback) {
    let pm = Services.perms;
    for (let i = 0; i < this._permissions.length; i++) {
      let type = this._permissions[i];
      if (!pm.testPermission(aHost, type))
        continue;

      let perms = pm.enumerator;
      while (perms.hasMoreElements()) {
        let permission = perms.getNext().QueryInterface(Ci.nsIPermission);
        if (permission.host == aHost.asciiHost && permission.type == type)
          aCallback(type);
      }
    }
  },

  updatePagePermissions: function updatePagePermissions(aNode) {
    let host = Browser.selectedBrowser.currentURI;
    let permissions = [];

    this._forEachPermissions(host, function(aType) {
      permissions.push("pageactions." + aType);
    });

    if (!this._loginManager.getLoginSavingEnabled(host.prePath)) {
      // If rememberSignons is false, then getLoginSavingEnabled returns false
      // for all pages, so we should just ignore it (Bug 601163).
      if (Services.prefs.getBoolPref("signon.rememberSignons"))
        permissions.push("pageactions.password");
    }

    let descriptions = permissions.map(function(s) Strings.browser.GetStringFromName(s));
    aNode.setAttribute("description", descriptions.join(", "));

    return (permissions.length > 0);
  },

  updateForgetPassword: function updateForgetPassword(aNode) {
    let host = Browser.selectedBrowser.currentURI;
    let logins = this._loginManager.findLogins({}, host.prePath, "", "");

    return logins.some(function(login) login.hostname == host.prePath);
  },

  forgetPassword: function forgetPassword(aEvent) {
    let host = Browser.selectedBrowser.currentURI;
    let lm = this._loginManager;

    lm.findLogins({}, host.prePath, "", "").forEach(function(login) {
      if (login.hostname == host.prePath)
        lm.removeLogin(login);
    });

    this.hideItem(aEvent.target);
    aEvent.stopPropagation(); // Don't hide the site menu.
  },

  clearPagePermissions: function clearPagePermissions(aEvent) {
    let pm = Services.perms;
    let host = Browser.selectedBrowser.currentURI;
    this._forEachPermissions(host, function(aType) {
      pm.remove(host.asciiHost, aType);
    });

    let lm = this._loginManager;
    if (!lm.getLoginSavingEnabled(host.prePath))
      lm.setLoginSavingEnabled(host.prePath, true);

    this.hideItem(aEvent.target);
    aEvent.stopPropagation(); // Don't hide the site menu.
  },

  savePageAsPDF: function saveAsPDF() {
    let browser = Browser.selectedBrowser;
    let fileName = ContentAreaUtils.getDefaultFileName(browser.contentTitle, browser.documentURI, null, null);
    fileName = fileName.trim() + ".pdf";
    let displayName = fileName;
#ifdef MOZ_PLATFORM_MAEMO
    fileName = fileName.replace(/[\*\:\?]+/g, " ");
#endif

    let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    let downloadsDir = dm.defaultDownloadsDirectory;

#ifdef ANDROID
    // Create the final destination file location
    // Try the intended filename, and if that doesn't work, try a safer one
    // (the intended one may have special characters)
    let file = null;
    [fileName, 'download.pdf'].forEach(function(potentialName, i) {
      if (file) return;
      let attemptedFile = downloadsDir.clone();
      attemptedFile.append(potentialName);
      // The filename is used below to save the file to a temp location in 
      // the content process. Make sure it's up to date.
      try {
        attemptedFile.createUnique(attemptedFile.NORMAL_FILE_TYPE, 0666);
      } catch (e) {
        // The first try may fail if the filename has special characters. If so
        // we will try with a safer name.
        if (i != 0)
          throw e;
        return;
      }
      file = attemptedFile;
      fileName = file.leafName;
    });
#else
    let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    picker.init(window, Strings.browser.GetStringFromName("pageactions.saveas.pdf"), Ci.nsIFilePicker.modeSave);
    picker.appendFilter("PDF", "*.pdf");
    picker.defaultExtension = "pdf";

    picker.defaultString = fileName;

    picker.displayDirectory = downloadsDir;
    let rv = picker.show();
    if (rv == Ci.nsIFilePicker.returnCancel)
      return;

    let file = picker.file;
#endif

    // We must manually add this to the download system
    let db = dm.DBConnection;

    let stmt = db.createStatement(
      "INSERT INTO moz_downloads (name, source, target, startTime, endTime, state, referrer) " +
      "VALUES (:name, :source, :target, :startTime, :endTime, :state, :referrer)"
    );

    let current = browser.currentURI.spec;
    stmt.params.name = displayName;
    stmt.params.source = current;
    stmt.params.target = Services.io.newFileURI(file).spec;
    stmt.params.startTime = Date.now() * 1000;
    stmt.params.endTime = Date.now() * 1000;
    stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED;
    stmt.params.referrer = current;
    stmt.execute();
    stmt.finalize();

    let newItemId = db.lastInsertRowID;
    let download = dm.getDownload(newItemId);
    try {
      DownloadsView.downloadStarted(download);
    }
    catch(e) {}
    Services.obs.notifyObservers(download, "dl-start", null);

#ifdef ANDROID
    let tmpDir = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);  
    
    file = tmpDir.clone();
    file.append(fileName);

#endif

    let data = {
      type: Ci.nsIPrintSettings.kOutputFormatPDF,
      id: newItemId,
      referrer: current,
      filePath: file.path
    };

    Browser.selectedBrowser.messageManager.sendAsyncMessage("Browser:SaveAs", data);
  },

  updatePageSaveAs: function updatePageSaveAs(aNode) {
    // Check for local XUL content
    let contentWindow = Browser.selectedBrowser.contentWindow;
    return !(contentWindow && contentWindow.document instanceof XULDocument);
  },

  updateShare: function updateShare(aNode) {
    return Util.isShareableScheme(Browser.selectedBrowser.currentURI.scheme);
  },

  hideItem: function hideItem(aNode) {
    aNode.hidden = true;
    this._updateAttributes();
  },

  _updateAttributes: function _updateAttributes() {
    let container = document.getElementById("pageactions-container");
    let visibleNodes = container.querySelectorAll("pageaction:not([hidden=true])");
    let visibleCount = visibleNodes.length;

    for (let i = 0; i < visibleCount; i++)
      visibleNodes[i].classList.remove("odd-last-child");

    if (visibleCount % 2)
      visibleNodes[visibleCount - 1].classList.add("odd-last-child");
  }
};

var NewTabPopup = {
  _timeout: 0,
  _tabs: [],

  init: function init() {
    Elements.tabs.addEventListener("TabOpen", this, true);
  },

  get box() {
    delete this.box;
    let box = document.getElementById("newtab-popup");

    // Move the popup on the other side if we are in RTL
    let [leftSidebar, rightSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    if (leftSidebar.left > rightSidebar.left) {
      let margin = box.getAttribute("left");
      box.removeAttribute("left");
      box.setAttribute("right", margin);
    }

    return this.box = box;
  },

  _updateLabel: function nt_updateLabel() {
    let newtabStrings = Strings.browser.GetStringFromName("newtabpopup.opened");
    let label = PluralForm.get(this._tabs.length, newtabStrings).replace("#1", this._tabs.length);

    this.box.firstChild.setAttribute("value", label);
  },

  hide: function nt_hide() {
    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = 0;
    }

    this._tabs = [];
    this.box.hidden = true;
    BrowserUI.popPopup(this);
  },

  show: function nt_show(aTab) {
    BrowserUI.pushPopup(this, this.box);

    this._tabs.push(aTab);
    this._updateLabel();

    this.box.top = aTab.getBoundingClientRect().top + (aTab.getBoundingClientRect().height / 3);
    this.box.hidden = false;

    if (this._timeout)
      clearTimeout(this._timeout);

    this._timeout = setTimeout(function(self) {
      self.hide();
    }, 2000, this);
  },

  selectTab: function nt_selectTab() {
    BrowserUI.selectTab(this._tabs.pop());
    this.hide();
  },

  handleEvent: function nt_handleEvent(aEvent) {
    // Bail early and fast
    if (!aEvent.detail)
      return;

    let [tabsVisibility,,,] = Browser.computeSidebarVisibility();
    if (tabsVisibility != 1.0)
      this.show(aEvent.originalTarget);
  }
};

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
    command.removeAttribute("checked", "true");
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

var BookmarkPopup = {
  get box() {
    delete this.box;
    this.box = document.getElementById("bookmark-popup");

    let [tabsSidebar, controlsSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    this.box.setAttribute(tabsSidebar.left < controlsSidebar.left ? "right" : "left", controlsSidebar.width - this.box.offset);
    this.box.top = BrowserUI.starButton.getBoundingClientRect().top - this.box.offset;

    // Hide the popup if there is any new page loading
    let self = this;
    messageManager.addMessageListener("pagehide", function(aMessage) {
      self.hide();
    });

    return this.box;
  },

  _bookmarkPopupTimeout: -1,

  hide : function hide() {
    if (this._bookmarkPopupTimeout != -1) {
      clearTimeout(this._bookmarkPopupTimeout);
      this._bookmarkPopupTimeout = -1;
    }
    this.box.hidden = true;
    BrowserUI.popPopup(this);
  },

  show : function show() {
    this.box.hidden = false;
    this.box.anchorTo(BrowserUI.starButton);

    // include starButton here, so that click-to-dismiss works as expected
    BrowserUI.pushPopup(this, [this.box, BrowserUI.starButton]);
  },

  autoHide: function autoHide() {
    if (this._bookmarkPopupTimeout != -1 || this.box.hidden)
      return;

    this._bookmarkPopupTimeout = setTimeout(function (self) {
      self._bookmarkPopupTimeout = -1;
      self.hide();
    }, 2000, this);
  },

  toggle : function toggle() {
    if (this.box.hidden)
      this.show();
    else
      this.hide();
  }
};

var BookmarkHelper = {
  _panel: null,
  _editor: null,

  edit: function BH_edit(aURI) {
    if (!aURI)
      aURI = getBrowser().currentURI;

    let itemId = PlacesUtils.getMostRecentBookmarkForURI(aURI);
    if (itemId == -1)
      return;

    let title = PlacesUtils.bookmarks.getItemTitle(itemId);
    let tags = PlacesUtils.tagging.getTagsForURI(aURI, {});

    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    this._editor = document.createElementNS(XULNS, "placeitem");
    this._editor.setAttribute("id", "bookmark-item");
    this._editor.setAttribute("flex", "1");
    this._editor.setAttribute("type", "bookmark");
    this._editor.setAttribute("ui", "manage");
    this._editor.setAttribute("title", title);
    this._editor.setAttribute("uri", aURI.spec);
    this._editor.setAttribute("itemid", itemId);
    this._editor.setAttribute("tags", tags.join(", "));
    this._editor.setAttribute("onclose", "BookmarkHelper.hide()");
    document.getElementById("bookmark-form").appendChild(this._editor);

    let toolbar = document.getElementById("toolbar-main");
    let top = toolbar.top + toolbar.boxObject.height;

    this._panel = document.getElementById("bookmark-container");
    this._panel.top = (top < 0 ? 0 : top);
    this._panel.hidden = false;
    BrowserUI.pushPopup(this, this._panel);

    let self = this;
    BrowserUI.lockToolbar();
    Browser.forceChromeReflow();
    self._editor.startEditing();
  },

  save: function BH_save() {
    this._editor.stopEditing(true);
  },

  hide: function BH_hide() {
    BrowserUI.unlockToolbar();
    BrowserUI.updateStar();

    // Note: the _editor will have already saved the data, if needed, by the time
    // this method is called, since this method is called via the "close" event.
    this._editor.parentNode.removeChild(this._editor);
    this._editor = null;

    this._panel.hidden = true;
    BrowserUI.popPopup(this);
  },

  removeBookmarksForURI: function BH_removeBookmarksForURI(aURI) {
    //XXX blargle xpconnect! might not matter, but a method on
    // nsINavBookmarksService that takes an array of items to
    // delete would be faster. better yet, a method that takes a URI!
    let itemIds = PlacesUtils.getBookmarksForURI(aURI);
    itemIds.forEach(PlacesUtils.bookmarks.removeItem);

    BrowserUI.updateStar();
  }
};

var FindHelperUI = {
  type: "find",
  commands: {
    next: "cmd_findNext",
    previous: "cmd_findPrevious",
    close: "cmd_findClose"
  },

  _open: false,
  _status: null,

  get status() {
    return this._status;
  },

  set status(val) {
    if (val != this._status) {
      this._status = val;
      this._textbox.setAttribute("status", val);
      this.updateCommands(this._textbox.value);
    }
  },

  init: function findHelperInit() {
    this._textbox = document.getElementById("find-helper-textbox");
    this._container = document.getElementById("content-navigator");

    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    // Listen for form assistant messages from content
    messageManager.addMessageListener("FindAssist:Show", this);
    messageManager.addMessageListener("FindAssist:Hide", this);

    // Listen for events where form assistant should be closed
    document.getElementById("tabs").addEventListener("TabSelect", this, true);
    Elements.browsers.addEventListener("URLChanged", this, true);
  },

  receiveMessage: function findHelperReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch(aMessage.name) {
      case "FindAssist:Show":
        this.status = json.result;
        if (json.rect)
          this._zoom(Rect.fromRect(json.rect));
        break;

      case "FindAssist:Hide":
        if (this._container.getAttribute("type") == this.type)
          this.hide();
        break;
    }
  },

  handleEvent: function findHelperHandleEvent(aEvent) {
    switch (aEvent.type) {
      case "TabSelect":
        this.hide();
        break;

      case "URLChanged":
        if (aEvent.detail && aEvent.target == getBrowser())
          this.hide();
        break;
    }
  },

  show: function findHelperShow() {
    this._container.show(this);
    this.search(this._textbox.value);
    this._textbox.select();
    this._textbox.focus();
    this._open = true;

    // Prevent the view to scroll automatically while searching
    Browser.selectedBrowser.scrollSync = false;
  },

  hide: function findHelperHide() {
    if (!this._open)
      return;

    this._textbox.value = "";
    this.status = null;
    this._textbox.blur();
    this._container.hide(this);
    this._open = false;

    // Restore the scroll synchronisation
    Browser.selectedBrowser.scrollSync = true;
  },

  goToPrevious: function findHelperGoToPrevious() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Previous", { });
  },

  goToNext: function findHelperGoToNext() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Next", { });
  },

  search: function findHelperSearch(aValue) {
    this.updateCommands(aValue);

    // Don't bother searching if the value is empty
    if (aValue == "")
      return;

    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Find", { searchString: aValue });
  },

  updateCommands: function findHelperUpdateCommands(aValue) {
    let disabled = (this._status == Ci.nsITypeAheadFind.FIND_NOTFOUND) || (aValue == "");
    this._cmdPrevious.setAttribute("disabled", disabled);
    this._cmdNext.setAttribute("disabled", disabled);
  },

  _zoom: function _findHelperZoom(aElementRect) {
    // Zoom to a specified Rect
    if (aElementRect && Browser.selectedTab.allowZoom && Services.prefs.getBoolPref("findhelper.autozoom")) {
      let zoomLevel = Browser._getZoomLevelForRect(aElementRect);
      zoomLevel = Math.min(Math.max(kBrowserFormZoomLevelMin, zoomLevel), kBrowserFormZoomLevelMax);
      zoomLevel = Browser.selectedTab.clampZoomLevel(zoomLevel);

      let zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, zoomLevel);
      Browser.animatedZoomTo(zoomRect);
    }
  }
};

/**
 * Responsible for navigating forms and filling in information.
 *  - Navigating forms is handled by next and previous commands.
 *  - When an element is focused, the browser view zooms in to the control.
 *  - The caret positionning and the view are sync to keep the type
 *    in text into view for input fields (text/textarea).
 *  - Provides autocomplete box for input fields.
 */
var FormHelperUI = {
  type: "form",
  commands: {
    next: "cmd_formNext",
    previous: "cmd_formPrevious",
    close: "cmd_formClose"
  },

  get enabled() {
    return Services.prefs.getBoolPref("formhelper.enabled");
  },

  init: function formHelperInit() {
    this._container = document.getElementById("content-navigator");
    this._autofillContainer = document.getElementById("form-helper-autofill");
    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    // Listen for form assistant messages from content
    messageManager.addMessageListener("FormAssist:Show", this);
    messageManager.addMessageListener("FormAssist:Hide", this);
    messageManager.addMessageListener("FormAssist:Update", this);
    messageManager.addMessageListener("FormAssist:Resize", this);
    messageManager.addMessageListener("FormAssist:AutoComplete", this);

    // Listen for events where form assistant should be closed or updated
    let tabs = document.getElementById("tabs");
    tabs.addEventListener("TabSelect", this, true);
    tabs.addEventListener("TabClose", this, true);
    Elements.browsers.addEventListener("URLChanged", this, true);
    Elements.browsers.addEventListener("SizeChanged", this, true);

    // Listen for modal dialog to show/hide the UI
    messageManager.addMessageListener("DOMWillOpenModalDialog", this);
    messageManager.addMessageListener("DOMModalDialogClosed", this);
  },

  _currentBrowser: null,
  show: function formHelperShow(aElement, aHasPrevious, aHasNext) {
    this._currentBrowser = Browser.selectedBrowser;

    // Update the next/previous commands
    this._cmdPrevious.setAttribute("disabled", !aHasPrevious);
    this._cmdNext.setAttribute("disabled", !aHasNext);
    this._open = true;

    let lastElement = this._currentElement || null;
    this._currentElement = {
      id: aElement.id,
      name: aElement.name,
      value: aElement.value,
      maxLength: aElement.maxLength,
      type: aElement.type,
      isAutocomplete: aElement.isAutocomplete,
      list: aElement.choices
    }

    this._updateContainer(lastElement, this._currentElement);
    this._zoom(Rect.fromRect(aElement.rect), Rect.fromRect(aElement.caretRect));

    // Prevent the view to scroll automatically while typing
    this._currentBrowser.scrollSync = false;
  },

  hide: function formHelperHide() {
    if (!this._open)
      return;

    // Restore the scroll synchonisation
    this._currentBrowser.scrollSync = true;

    // reset current Element and Caret Rect
    this._currentElementRect = null;
    this._currentCaretRect = null;

    this._updateContainerForSelect(this._currentElement, null);

    this._currentBrowser.messageManager.sendAsyncMessage("FormAssist:Closed", { });
    this._open = false;
  },

  // for VKB that does not resize the window
  _currentCaretRect: null,
  _currentElementRect: null,
  handleEvent: function formHelperHandleEvent(aEvent) {
    if (!this._open)
      return;

    switch (aEvent.type) {
      case "TabSelect":
      case "TabClose":
        this.hide();
        break;

      case "URLChanged":
        if (aEvent.detail && aEvent.target == getBrowser())
          this.hide();
        break;

      case "SizeChanged":
        setTimeout(function(self) {
          SelectHelperUI.resize();
          self._container.contentHasChanged();

          self._zoom(this._currentElementRect, this._currentCaretRect);
        }, 0, this);
        break;
    }
  },

  receiveMessage: function formHelperReceiveMessage(aMessage) {
    if (!this._open && aMessage.name != "FormAssist:Show" && aMessage.name != "FormAssist:Hide")
      return;

    let json = aMessage.json;
    switch (aMessage.name) {
      case "FormAssist:Show":
        // if the user has manually disabled the Form Assistant UI we still
        // want to show a UI for <select /> element but not managed by
        // FormHelperUI
        this.enabled ? this.show(json.current, json.hasPrevious, json.hasNext)
                     : SelectHelperUI.show(json.current.choices);
        break;

      case "FormAssist:Hide":
        this.enabled ? this.hide()
                     : SelectHelperUI.hide();
        break;

      case "FormAssist:Resize":
        let element = json.current;
        this._zoom(Rect.fromRect(element.rect), Rect.fromRect(element.caretRect));
        break;

      case "FormAssist:AutoComplete":
        this._updateAutocompleteFor(json.current);
        this._container.contentHasChanged();
        break;

       case "FormAssist:Update":
        Browser.hideSidebars();
        Browser.hideTitlebar();
        this._zoom(null, Rect.fromRect(json.caretRect));
        break;

      case "DOMWillOpenModalDialog":
        if (aMessage.target == Browser.selectedBrowser) {
          this._container.style.display = "none";
          this._container._spacer.hidden = true;
        }
        break;

      case "DOMModalDialogClosed":
        if (aMessage.target == Browser.selectedBrowser) {
          this._container.style.display = "-moz-box";
          this._container._spacer.hidden = false;
        }
        break;
    }
  },

  goToPrevious: function formHelperGoToPrevious() {
    this._currentBrowser.messageManager.sendAsyncMessage("FormAssist:Previous", { });
  },

  goToNext: function formHelperGoToNext() {
    this._currentBrowser.messageManager.sendAsyncMessage("FormAssist:Next", { });
  },

  doAutoComplete: function formHelperDoAutoComplete(aElement) {
    // Suggestions are only in <label>s. Ignore the rest.
    if (aElement instanceof Ci.nsIDOMXULLabelElement)
      this._currentBrowser.messageManager.sendAsyncMessage("FormAssist:AutoComplete", { value: aElement.value });
  },

  get _open() {
    return (this._container.getAttribute("type") == this.type);
  },

  set _open(aVal) {
    if (aVal == this._open)
      return;

    this._container.hidden = !aVal;
    this._container.contentHasChanged();

    if (aVal) {
      this._zoomStart();
      this._container.show(this);
    } else {
      this._zoomFinish();
      this._currentElement = null;
      this._container.hide(this);
    }

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("FormUI", true, true, window, aVal);
    this._container.dispatchEvent(evt);
  },

  _updateAutocompleteFor: function _formHelperUpdateAutocompleteFor(aElement) {
    let suggestions = this._getAutocompleteSuggestions(aElement);
    this._displaySuggestions(suggestions);
  },

  _displaySuggestions: function _formHelperDisplaySuggestions(aSuggestions) {
    let autofill = this._autofillContainer;
    while (autofill.hasChildNodes())
      autofill.removeChild(autofill.lastChild);

    let fragment = document.createDocumentFragment();
    for (let i = 0; i < aSuggestions.length; i++) {
      let value = aSuggestions[i];
      let button = document.createElement("label");
      button.setAttribute("value", value);
      button.className = "form-helper-autofill-label";
      fragment.appendChild(button);
    }
    autofill.appendChild(fragment);
    autofill.collapsed = !aSuggestions.length;
  },

  /** Retrieve the autocomplete list from the autocomplete service for an element */
  _getAutocompleteSuggestions: function _formHelperGetAutocompleteSuggestions(aElement) {
    if (!aElement.isAutocomplete)
      return [];

    let suggestions = [];

    let autocompleteService = Cc["@mozilla.org/satchel/form-autocomplete;1"].getService(Ci.nsIFormAutoComplete);
    let results = autocompleteService.autoCompleteSearch(aElement.name, aElement.value, aElement, null);
    if (results.matchCount > 0) {
      for (let i = 0; i < results.matchCount; i++) {
        let value = results.getValueAt(i);
        suggestions.push(value);
      }
    }

    return suggestions;
  },

  /** Update the form helper container to reflect new element user is editing. */
  _updateContainer: function _formHelperUpdateContainer(aLastElement, aCurrentElement) {
    this._updateContainerForSelect(aLastElement, aCurrentElement);

    // Setup autofill UI
    this._updateAutocompleteFor(aCurrentElement);
    this._container.contentHasChanged();
  },

  /** Helper for _updateContainer that handles the case where the new element is a select. */
  _updateContainerForSelect: function _formHelperUpdateContainerForSelect(aLastElement, aCurrentElement) {
    let lastHasChoices = aLastElement && (aLastElement.list != null);
    let currentHasChoices = aCurrentElement && (aCurrentElement.list != null);

    if (!lastHasChoices && currentHasChoices) {
      SelectHelperUI.dock(this._container);
      SelectHelperUI.show(aCurrentElement.list);
    } else if (lastHasChoices && currentHasChoices) {
      SelectHelperUI.reset();
      SelectHelperUI.show(aCurrentElement.list);
    } else if (lastHasChoices && !currentHasChoices) {
      SelectHelperUI.hide();
    }
  },

  /** Zoom and move viewport so that element is legible and touchable. */
  _zoom: function _formHelperZoom(aElementRect, aCaretRect) {
    let browser = getBrowser();
    let zoomRect = Rect.fromRect(browser.getBoundingClientRect());

    // Zoom to a specified Rect
    let autozoomEnabled = Services.prefs.getBoolPref("formhelper.autozoom");
    if (aElementRect && Browser.selectedTab.allowZoom && autozoomEnabled) {
      this._currentElementRect = aElementRect;
      // Zoom to an element by keeping the caret into view
      let zoomLevel = Browser.selectedTab.clampZoomLevel(this._getZoomLevelForRect(aElementRect));

      zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, zoomLevel);
      Browser.animatedZoomTo(zoomRect);
    } else if (aElementRect && !Browser.selectedTab.allowZoom && autozoomEnabled) {
      // Even if zooming is disabled we could need to reposition the view in
      // order to keep the element on-screen
      Browser.animatedZoomTo(zoomRect);
    }

    this._ensureCaretVisible(aCaretRect);
  },

  _ensureCaretVisible: function _ensureCaretVisible(aCaretRect) {
    if (!aCaretRect)
      return;

    // the scrollX/scrollY position can change because of the animated zoom so
    // delay the caret adjustment
    if (AnimatedZoom.isZooming()) {
      let self = this;
      window.addEventListener("AnimatedZoomEnd", function() {
        window.removeEventListener("AnimatedZoomEnd", arguments.callee, true);
          self._ensureCaretVisible(aCaretRect);
      }, true);
      return;
    }

    let browser = getBrowser();
    let zoomRect = Rect.fromRect(browser.getBoundingClientRect());

    this._currentCaretRect = aCaretRect;
    let caretRect = aCaretRect.scale(browser.scale, browser.scale);

    let scroll = browser.getPosition();
    zoomRect = new Rect(scroll.x, scroll.y, zoomRect.width, zoomRect.height);
    if (zoomRect.contains(caretRect))
      return;

    let [deltaX, deltaY] = this._getOffsetForCaret(caretRect, zoomRect);
    if (deltaX != 0 || deltaY != 0)
      browser.scrollBy(deltaX, deltaY);
  },

  /* Store the current zoom level, and scroll positions to restore them if needed */
  _zoomStart: function _formHelperZoomStart() {
    if (!Services.prefs.getBoolPref("formhelper.restore"))
      return;

    this._restore = {
      scale: getBrowser().scale,
      contentScrollOffset: Browser.getScrollboxPosition(Browser.contentScrollboxScroller),
      pageScrollOffset: Browser.getScrollboxPosition(Browser.pageScrollboxScroller)
    };
  },

  /** Element is no longer selected. Restore zoom level if setting is enabled. */
  _zoomFinish: function _formHelperZoomFinish() {
    if(!Services.prefs.getBoolPref("formhelper.restore"))
      return;

    let restore = this._restore;
    getBrowser().scale = restore.scale;
    Browser.contentScrollboxScroller.scrollTo(restore.contentScrollOffset.x, restore.contentScrollOffset.y);
    Browser.pageScrollboxScroller.scrollTo(restore.pageScrollOffset.x, restore.pageScrollOffset.y);
  },

  _getZoomLevelForRect: function _getZoomLevelForRect(aRect) {
    const margin = 30;
    let zoomLevel = getBrowser().getBoundingClientRect().width / (aRect.width + margin);
    return Util.clamp(zoomLevel, kBrowserFormZoomLevelMin, kBrowserFormZoomLevelMax);
  },

  _getOffsetForCaret: function _formHelperGetOffsetForCaret(aCaretRect, aRect) {
    // Determine if we need to move left or right to bring the caret into view
    let deltaX = 0;
    if (aCaretRect.right > aRect.right)
      deltaX = aCaretRect.right - aRect.right;
    if (aCaretRect.left < aRect.left)
      deltaX = aCaretRect.left - aRect.left;

    // Determine if we need to move up or down to bring the caret into view
    let deltaY = 0;
    if (aCaretRect.bottom > aRect.bottom)
      deltaY = aCaretRect.bottom - aRect.bottom;
    if (aCaretRect.top < aRect.top)
      deltaY = aCaretRect.top - aRect.top;

    return [deltaX, deltaY];
  }
};

/**
 * SelectHelperUI: Provides an interface for making a choice in a list.
 *   Supports simultaneous selection of choices and group headers.
 */
var SelectHelperUI = {
  _list: null,
  _selectedIndexes: null,

  get _panel() {
    delete this._panel;
    return this._panel = document.getElementById("select-container");
  },

  get _textbox() {
    delete this._textbox;
    return this._textbox = document.getElementById("select-helper-textbox");
  },

  show: function selectHelperShow(aList) {
    this.showFilter = false;
    this._textbox.blur();
    this._list = aList;

    this._container = document.getElementById("select-list");
    this._container.setAttribute("multiple", aList.multiple ? "true" : "false");

    this._selectedIndexes = this._getSelectedIndexes();
    let firstSelected = null;

    // Using a fragment prevent us to hang on huge list
    let fragment = document.createDocumentFragment();
    let choices = aList.choices;
    for (let i = 0; i < choices.length; i++) {
      let choice = choices[i];
      let item = document.createElement("option");
      item.className = "chrome-select-option";
      item.setAttribute("label", choice.text);
      choice.disabled ? item.setAttribute("disabled", choice.disabled)
                      : item.removeAttribute("disabled");
      fragment.appendChild(item);

      if (choice.group) {
        item.classList.add("optgroup");
        continue;
      }

      item.optionIndex = choice.optionIndex;
      item.choiceIndex = i;

      if (choice.inGroup)
        item.classList.add("in-optgroup");

      if (choice.selected) {
        item.setAttribute("selected", "true");
        firstSelected = firstSelected || item;
      }
    }
    this._container.appendChild(fragment);

    this._panel.hidden = false;
    this._panel.height = this._panel.getBoundingClientRect().height;

    if (!this._docked)
      BrowserUI.pushPopup(this, this._panel);

    this._scrollElementIntoView(firstSelected);

    this._container.addEventListener("click", this, false);
    this._panel.addEventListener("overflow", this, true);
  },

  _showFilter: false,
  get showFilter() {
    return this._showFilter;
  },

  set showFilter(val) {
    this._showFilter = val;
    if (!this._panel.hidden)
      this._textbox.hidden = !val;
  },

  dock: function selectHelperDock(aContainer) {
    aContainer.insertBefore(this._panel, aContainer.lastChild);
    this.resize();
    this._docked = true;
  },

  undock: function selectHelperUndock() {
    let rootNode = Elements.stack;
    rootNode.insertBefore(this._panel, rootNode.lastChild);
    this._panel.style.maxHeight = "";
    this._docked = false;
  },

  reset: function selectHelperReset() {
    this._updateControl();
    let empty = this._container.cloneNode(false);
    this._container.parentNode.replaceChild(empty, this._container);
    this._container = empty;
    this._list = null;
    this._selectedIndexes = null;
    this._panel.height = "";
    this._textbox.value = "";
  },

  resize: function selectHelperResize() {
    this._panel.style.maxHeight = (window.innerHeight / 1.8) + "px";
  },

  hide: function selectHelperResize() {
    this.showFilter = false;
    this._container.removeEventListener("click", this, false);
    this._panel.removeEventListener("overflow", this, true);

    this._panel.hidden = true;

    if (this._docked)
      this.undock();
    else
      BrowserUI.popPopup(this);

    this.reset();
  },

  filter: function selectHelperFilter(aValue) {
    let reg = new RegExp(aValue, "gi");
    let options = this._container.childNodes;
    for (let i = 0; i < options.length; i++) {
      let option = options[i];
      option.getAttribute("label").match(reg) ? option.removeAttribute("filtered")
                                              : option.setAttribute("filtered", "true");
    }
  },

  unselectAll: function selectHelperUnselectAll() {
    if (!this._list)
      return;

    let choices = this._list.choices;
    this._forEachOption(function(aItem, aIndex) {
      aItem.selected = false;
      choices[aIndex].selected = false;
    });
  },

  selectByIndex: function selectHelperSelectByIndex(aIndex) {
    if (!this._list)
      return;

    let choices = this._list.choices;
    for (let i = 0; i < this._container.childNodes.length; i++) {
      let option = this._container.childNodes[i];
      if (option.optionIndex == aIndex) {
        option.selected = true;
        this._choices[i].selected = true;
        this._scrollElementIntoView(option);
        break;
      }
    }
  },

  _getSelectedIndexes: function _selectHelperGetSelectedIndexes() {
    let indexes = [];
    if (!this._list)
      return indexes;

    let choices = this._list.choices;
    let choiceLength = choices.length;
    for (let i = 0; i < choiceLength; i++) {
      let choice = choices[i];
      if (choice.selected)
        indexes.push(choice.optionIndex);
    }
    return indexes;
  },

  _scrollElementIntoView: function _selectHelperScrollElementIntoView(aElement) {
    if (!aElement)
      return;

    let index = -1;
    this._forEachOption(
      function(aItem, aIndex) {
        if (aElement.optionIndex == aItem.optionIndex)
          index = aIndex;
      }
    );

    if (index == -1)
      return;

    let scrollBoxObject = this._container.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    let itemHeight = aElement.getBoundingClientRect().height;
    let visibleItemsCount = this._container.boxObject.height / itemHeight;
    if ((index + 1) > visibleItemsCount) {
      let delta = Math.ceil(visibleItemsCount / 2);
      scrollBoxObject.scrollTo(0, ((index + 1) - delta) * itemHeight);
    }
    else {
      scrollBoxObject.scrollTo(0, 0);
    }
  },

  _forEachOption: function _selectHelperForEachOption(aCallback) {
    let children = this._container.children;
    for (let i = 0; i < children.length; i++) {
      let item = children[i];
      if (!item.hasOwnProperty("optionIndex"))
        continue;
      aCallback(item, i);
    }
  },

  _updateControl: function _selectHelperUpdateControl() {
    let currentSelectedIndexes = this._getSelectedIndexes();

    let isIdentical = (this._selectedIndexes && this._selectedIndexes.length == currentSelectedIndexes.length);
    if (isIdentical) {
      for (let i = 0; i < currentSelectedIndexes.length; i++) {
        if (currentSelectedIndexes[i] != this._selectedIndexes[i]) {
          isIdentical = false;
          break;
        }
      }
    }

    if (isIdentical)
      return;

    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceChange", { });
  },

  handleEvent: function selectHelperHandleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        let item = aEvent.target;
        if (item && item.hasOwnProperty("optionIndex")) {
          if (this._list.multiple) {
            // Toggle the item state
            item.selected = !item.selected;
          }
          else {
            this.unselectAll();

            // Select the new one and update the control
            item.selected = true;
          }
          this.onSelect(item.optionIndex, item.selected, !this._list.multiple);
        }
        break;
      case "overflow":
        if (!this._textbox.value)
          this.showFilter = true;
        break;
    }
  },

  onSelect: function selectHelperOnSelect(aIndex, aSelected, aClearAll) {
    let json = {
      index: aIndex,
      selected: aSelected,
      clearAll: aClearAll
    };
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceSelect", json);

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-button-element.html#the-select-element
    if (!this._list.multiple) {
      this._updateControl();
      // Update the selectedIndex so the field will fire a new change event if
      // needed
      this._selectedIndexes = [aIndex];
    }

  }
};

var MenuListHelperUI = {
  get _container() {
    delete this._container;
    return this._container = document.getElementById("menulist-container");
  },

  get _popup() {
    delete this._popup;
    return this._popup = document.getElementById("menulist-popup");
  },

  get _title() {
    delete this._title;
    return this._title = document.getElementById("menulist-title");
  },

  _firePopupEvent: function firePopupEvent(aEventName) {
    let menupopup = this._currentList.menupopup;
    if (menupopup.hasAttribute(aEventName)) {
      let func = new Function("event", menupopup.getAttribute(aEventName));
      func.call(this);
    }
  },

  _currentList: null,
  show: function mn_show(aMenulist) {
    this._currentList = aMenulist;
    this._container.setAttribute("for", aMenulist.id);
    this._title.value = aMenulist.title || "";
    this._firePopupEvent("onpopupshowing");

    let container = this._container;
    let listbox = this._popup.lastChild;
    while (listbox.firstChild)
      listbox.removeChild(listbox.firstChild);

    let children = this._currentList.menupopup.children;
    for (let i = 0; i < children.length; i++) {
      let child = children[i];
      let item = document.createElement("richlistitem");
      if (child.disabled)
        item.setAttribute("disabled", "true");
      if (child.hidden)
        item.setAttribute("hidden", "true");

      // Add selected as a class name instead of an attribute to not being overidden
      // by the richlistbox behavior (it sets the "current" and "selected" attribute
      item.setAttribute("class", "menulist-command prompt-button" + (child.selected ? " selected" : ""));

      let image = document.createElement("image");
      image.setAttribute("src", child.image || "");
      item.appendChild(image);

      let label = document.createElement("label");
      label.setAttribute("value", child.label);
      item.appendChild(label);

      listbox.appendChild(item);
    }

    window.addEventListener("resize", this, true);
    container.hidden = false;
    this.sizeToContent();
    BrowserUI.pushPopup(this, [this._popup]);
  },

  hide: function mn_hide() {
    this._currentList = null;
    this._container.removeAttribute("for");
    this._container.hidden = true;
    window.removeEventListener("resize", this, true);
    BrowserUI.popPopup(this);
  },

  selectByIndex: function mn_selectByIndex(aIndex) {
    this._currentList.selectedIndex = aIndex;

    // Dispatch a xul command event to the attached menulist
    if (this._currentList.dispatchEvent) {
      let evt = document.createEvent("XULCommandEvent");
      evt.initCommandEvent("command", true, true, window, 0, false, false, false, false, null);
      this._currentList.dispatchEvent(evt);
    }

    this.hide();
  },

  sizeToContent: function sizeToContent() {
    this._popup.maxWidth = window.innerWidth * 0.75;
  },

  handleEvent: function handleEvent(aEvent) {
    this.sizeToContent();
  }
}

var ContextHelper = {
  popupState: null,

  get _panel() {
    delete this._panel;
    return this._panel = document.getElementById("context-container");
  },

  get _popup() {
    delete this._popup;
    return this._popup = document.getElementById("context-popup");
  },

  showPopup: function ch_showPopup(aMessage) {
    this.popupState = aMessage.json;
    this.popupState.target = aMessage.target;

    let first = null;
    let last = null;
    let commands = document.getElementById("context-commands");
    for (let i=0; i<commands.childElementCount; i++) {
      let command = commands.children[i];
      command.removeAttribute("selector");
      command.hidden = true;

      let types = command.getAttribute("type").split(/\s+/);
      for (let i=0; i<types.length; i++) {
        if (this.popupState.types.indexOf(types[i]) != -1) {
          first = first || command;
          last = command;
          command.hidden = false;
          break;
        }
      }
    }

    if (!first) {
      this.popupState = null;
      return false;
    }

    // Allow the first and last *non-hidden* elements to be selected in CSS.
    first.setAttribute("selector", "first-child");
    last.setAttribute("selector", "last-child");

    let label = document.getElementById("context-hint");
    label.value = this.popupState.label || "";

    this._panel.hidden = false;
    window.addEventListener("resize", this, true);
    window.addEventListener("keypress", this, true);

    this.sizeToContent();
    BrowserUI.pushPopup(this, [this._popup]);
    return true;
  },

  hide: function ch_hide() {
    if (this._panel.hidden)
      return;
    this.popupState = null;
    this._panel.hidden = true;
    window.removeEventListener("resize", this, true);
    window.removeEventListener("keypress", this, true);

    BrowserUI.popPopup(this);
  },

  sizeToContent: function sizeToContent() {
    this._popup.maxWidth = window.innerWidth * 0.75;
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "resize":
        this.sizeToContent();
        break;
      case "keypress":
        // Hide the context menu so you can't type behind it.
        aEvent.stopPropagation();
        aEvent.preventDefault();
        if (aEvent.keyCode != aEvent.DOM_VK_ESCAPE)
          this.hide();
        break;
    }
  }
};

var ContextCommands = {
  copy: function cc_copy() {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(ContextHelper.popupState.string);

    let target = ContextHelper.popupState.target;
    if (target)
      target.focus();
  },

#ifdef ANDROID
  selectInput: function cc_selectInput() {
    let imePicker = Cc["@mozilla.org/imepicker;1"].getService(Ci.nsIIMEPicker);
    imePicker.show();
  },
#endif

  paste: function cc_paste() {
    let data = ContextHelper.popupState.data;
    let target = ContextHelper.popupState.target;
    target.editor.paste(Ci.nsIClipboard.kGlobalClipboard);
    target.focus();
  },

  selectAll: function cc_selectAll() {
    let target = ContextHelper.popupState.target;
    target.editor.selectAll();
    target.focus();
  },

  openInNewTab: function cc_openInNewTab() {
    Browser.addTab(ContextHelper.popupState.linkURL, false, Browser.selectedTab);
  },

  saveLink: function cc_saveLink() {
    let browser = ContextHelper.popupState.target;
    saveURL(ContextHelper.popupState.linkURL, null, "SaveLinkTitle", false, true, browser.documentURI);
  },

  saveImage: function cc_saveImage() {
    let browser = ContextHelper.popupState.target;
    saveImageURL(ContextHelper.popupState.mediaURL, null, "SaveImageTitle", false, true, browser.documentURI);
  },

  shareLink: function cc_shareLink() {
    let state = ContextHelper.popupState;
    SharingUI.show(state.linkURL, state.linkTitle);
  },

  shareMedia: function cc_shareMedia() {
    SharingUI.show(ContextHelper.popupState.mediaURL, null);
  },

  sendCommand: function cc_playVideo(aCommand) {
    let browser = ContextHelper.popupState.target;
    browser.messageManager.sendAsyncMessage("Browser:ContextCommand", { command: aCommand });
  },

  editBookmark: function cc_editBookmark() {
    let target = ContextHelper.popupState.target;
    target.startEditing();
  },

  removeBookmark: function cc_removeBookmark() {
    let target = ContextHelper.popupState.target;
    target.remove();
  }
}

var SharingUI = {
  _dialog: null,

  show: function show(aURL, aTitle) {
    try {
      this.showSharingUI(aURL, aTitle);
    } catch (ex) {
      this.showFallback(aURL, aTitle);
    }
  },

  showSharingUI: function showSharingUI(aURL, aTitle) {
    let sharingSvc = Cc["@mozilla.org/uriloader/external-sharing-app-service;1"].getService(Ci.nsIExternalSharingAppService);
    sharingSvc.shareWithDefault(aURL, "text/plain", aTitle);
  },

  showFallback: function showFallback(aURL, aTitle) {
    this._dialog = importDialog(window, "chrome://browser/content/share.xul", null);
    document.getElementById("share-title").value = aTitle || aURL;

    BrowserUI.pushPopup(this, this._dialog);

    let bbox = document.getElementById("share-buttons-box");
    this._handlers.forEach(function(handler) {
      let button = document.createElement("button");
      button.className = "prompt-button";
      button.setAttribute("label", handler.name);
      button.addEventListener("command", function() {
        SharingUI.hide();
        handler.callback(aURL || "", aTitle || "");
      }, false);
      bbox.appendChild(button);
    });
    this._dialog.waitForClose();
    BrowserUI.popPopup(this);
  },

  hide: function hide() {
    this._dialog.close();
    this._dialog = null;
  },

  _handlers: [
    {
      name: "Email",
      callback: function callback(aURL, aTitle) {
        let url = "mailto:?subject=" + encodeURIComponent(aTitle) +
                  "&body=" + encodeURIComponent(aURL);
        let uri = Services.io.newURI(url, null, null);
        let extProtocolSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
                             .getService(Ci.nsIExternalProtocolService);
        extProtocolSvc.loadUrl(uri);
      }
    },
    {
      name: "Twitter",
      callback: function callback(aURL, aTitle) {
        let url = "http://twitter.com/home?status=" + encodeURIComponent((aTitle ? aTitle+": " : "")+aURL);
        BrowserUI.newTab(url, Browser.selectedTab);
      }
    },
    {
      name: "Google Reader",
      callback: function callback(aURL, aTitle) {
        let url = "http://www.google.com/reader/link?url=" + encodeURIComponent(aURL) +
                  "&title=" + encodeURIComponent(aTitle);
        BrowserUI.addTab(url, Browser.selectedTab);
      }
    },
    {
      name: "Facebook",
      callback: function callback(aURL, aTitle) {
        let url = "http://www.facebook.com/share.php?u=" + encodeURIComponent(aURL);
        BrowserUI.newTab(url, Browser.selectedTab);
      }
    }
  ]
};

var BadgeHandlers = {
  _handlers: [
    {
      _lastUpdate: 0,
      _lastCount: 0,
      url: "https://mail.google.com/mail",
      updateBadge: function(aBadge) {
        // Use the cache if possible
        let now = Date.now();
        if (this._lastCount && this._lastUpdate > now - 1000) {
          aBadge.set(this._lastCount);
          return;
        }

        this._lastUpdate = now;

        // Use any saved username and password. If we don't have any login and we are not
        // currently logged into Gmail, we won't get any count.
        let login = BadgeHandlers.getLogin("https://www.google.com");

        // Get the feed and read the count, passing any saved username and password
        // but do not show any security dialogs if we fail
        let req = new XMLHttpRequest();
        req.mozBackgroundRequest = true;
        req.open("GET", "https://mail.google.com/mail/feed/atom", true, login.username, login.password);
        req.onreadystatechange = function(aEvent) {
          if (req.readyState == 4) {
            if (req.status == 200 && req.responseXML) {
              let count = req.responseXML.getElementsByTagName("fullcount");
              this._lastCount = count ? count[0].childNodes[0].nodeValue : 0;
            } else {
              this._lastCount = 0;
            }
            this._lastCount = BadgeHandlers.setNumberBadge(aBadge, this._lastCount);
          }
        };
        req.send(null);
      }
    }
  ],

  register: function(aPopup) {
    let handlers = this._handlers;
    for (let i = 0; i < handlers.length; i++)
      aPopup.registerBadgeHandler(handlers[i].url, handlers[i]);
  },

  getLogin: function(aURL) {
    let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
    let logins = lm.findLogins({}, aURL, aURL, null);
    let username = logins.length > 0 ? logins[0].username : "";
    let password = logins.length > 0 ? logins[0].password : "";
    return { username: username, password: password };
  },

  clampBadge: function(aValue) {
    if (aValue > 100)
      aValue = "99+";
    return aValue;
  },

  setNumberBadge: function(aBadge, aValue) {
    if (parseInt(aValue) != 0) {
      aValue = this.clampBadge(aValue);
      aBadge.set(aValue);
    } else {
      aBadge.set("");
    }
    return aValue;
  }
};

var FullScreenVideo = {
  browser: null,

  init: function fsv_init() {
    messageManager.addMessageListener("Browser:FullScreenVideo:Start", this.show.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Close", this.hide.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Play", this.play.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Pause", this.pause.bind(this));

    // If the screen supports brightness locks, we will utilize that, see checkBrightnessLocking()
    try {
      this.screen = null;
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"].getService(Ci.nsIScreenManager);
      let screen = screenManager.primaryScreen.QueryInterface(Ci.nsIScreen_MOZILLA_2_0_BRANCH);
      this.screen = screen;
    }
    catch (e) {} // The screen does not support brightness locks
  },

  play: function() {
    this.playing = true;
    this.checkBrightnessLocking();
  },

  pause: function() {
    this.playing = false;
    this.checkBrightnessLocking();
  },

  // We lock the screen brightness - prevent it from dimming or turning off - when
  // we are fullscreen, and are playing (and the screen supports that feature).
  checkBrightnessLocking: function() {
    var shouldLock = !!this.screen && !!window.fullScreen && !!this.playing;
    var locking = !!this.brightnessLocked;
    if (shouldLock == locking)
      return;

    if (shouldLock)
      this.screen.lockMinimumBrightness(this.screen.BRIGHTNESS_FULL);
    else
      this.screen.unlockMinimumBrightness(this.screen.BRIGHTNESS_FULL);
    this.brightnessLocked = shouldLock;
  },

  show: function fsv_show() {
    this.createBrowser();
    window.fullScreen = true;
    BrowserUI.pushPopup(this, this.browser);
    this.checkBrightnessLocking();
  },

  hide: function fsv_hide() {
    this.destroyBrowser();
    window.fullScreen = false;
    BrowserUI.popPopup(this);
    this.checkBrightnessLocking();
  },

  createBrowser: function fsv_createBrowser() {
    let browser = this.browser = document.createElement("browser");
    browser.className = "window-width window-height full-screen";
    browser.setAttribute("type", "content");
    browser.setAttribute("remote", "true");
    browser.setAttribute("src", "chrome://browser/content/fullscreen-video.xhtml");
    document.getElementById("main-window").appendChild(browser);

    let mm = browser.messageManager;
    mm.loadFrameScript("chrome://browser/content/fullscreen-video.js", true);

    browser.addEventListener("TapDown", this, true);
    browser.addEventListener("TapSingle", this, false);

    return browser;
  },

  destroyBrowser: function fsv_destroyBrowser() {
    let browser = this.browser;
    browser.removeEventListener("TapDown", this, false);
    browser.removeEventListener("TapSingle", this, false);
    browser.parentNode.removeChild(browser);
    this.browser = null;
  },

  handleEvent: function fsv_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "TapDown":
        this._dispatchMouseEvent("Browser:MouseDown", aEvent.clientX, aEvent.clientY);
        break;
      case "TapSingle":
        this._dispatchMouseEvent("Browser:MouseUp", aEvent.clientX, aEvent.clientY);
        break;
    }
  },

  _dispatchMouseEvent: function fsv_dispatchMouseEvent(aName, aX, aY) {
    let pos = this.browser.transformClientToBrowser(aX, aY);
    this.browser.messageManager.sendAsyncMessage(aName, {
      x: pos.x,
      y: pos.y,
      messageId: null
    });
  }
};

var AppMenu = {
  get panel() {
    delete this.panel;
    return this.panel = document.getElementById("appmenu");
  },

  show: function show() {
    if (BrowserUI.activePanel || BrowserUI.isPanelVisible())
      return;
    this.panel.setAttribute("count", this.panel.childNodes.length);
    this.panel.collapsed = false;

    addEventListener("keypress", this, true);

    BrowserUI.lockToolbar();
    BrowserUI.pushPopup(this, [this.panel, Elements.toolbarContainer]);
  },

  hide: function hide() {
    this.panel.collapsed = true;

    removeEventListener("keypress", this, true);

    BrowserUI.unlockToolbar();
    BrowserUI.popPopup(this);
  },

  toggle: function toggle() {
    this.panel.collapsed ? this.show() : this.hide();
  },

  handleEvent: function handleEvent(aEvent) {
    this.hide();
  }
};
