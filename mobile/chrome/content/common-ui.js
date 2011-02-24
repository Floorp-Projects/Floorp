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

  // Permissions we track in Page Actions
  _permissions: ["popup", "offline-app", "geolocation", "desktop-notification"],

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

      // reset the 'remember' counter for permissions that support it
      if (["geolocation", "desktop-notification"].indexOf(aType) != -1)
        Services.contentPrefs.setPref(host.asciiHost, aType + ".request.remember", 0);
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
      if (!val)
        this._textbox.removeAttribute("status");
      else
        this._textbox.setAttribute("status", val);
      this.updateCommands(this._textbox.value);
    }
  },

  init: function findHelperInit() {
    this._textbox = document.getElementById("find-helper-textbox");
    this._container = document.getElementById("content-navigator");

    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    // Listen for find assistant messages from content
    messageManager.addMessageListener("FindAssist:Show", this);
    messageManager.addMessageListener("FindAssist:Hide", this);

    // Listen for pan events happening on the browsers
    Elements.browsers.addEventListener("PanBegin", this, false);
    Elements.browsers.addEventListener("PanFinished", this, false);

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

      case "PanBegin":
        this._container.style.visibility = "hidden";
        this._textbox.collapsed = true;
        break;

      case "PanFinished":
        this._container.style.visibility = "visible";
        this._textbox.collapsed = false;
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
    if (aValue == "") {
      this.status = null;
      return;
    }

    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Find", { searchString: aValue });
  },

  updateCommands: function findHelperUpdateCommands(aValue) {
    let disabled = (this._status == Ci.nsITypeAheadFind.FIND_NOTFOUND) || (aValue == "");
    this._cmdPrevious.setAttribute("disabled", disabled);
    this._cmdNext.setAttribute("disabled", disabled);
  },

  _zoom: function _findHelperZoom(aElementRect) {
    let autozoomEnabled = Services.prefs.getBoolPref("findhelper.autozoom");
    if (!aElementRect || !autozoomEnabled)
      return;

    if (Browser.selectedTab.allowZoom) {
      let zoomLevel = Browser._getZoomLevelForRect(aElementRect);
      zoomLevel = Math.min(Math.max(kBrowserFormZoomLevelMin, zoomLevel), kBrowserFormZoomLevelMax);
      zoomLevel = Browser.selectedTab.clampZoomLevel(zoomLevel);

      let zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, zoomLevel);
      AnimatedZoom.animateTo(zoomRect);
    } else {
      // Even if zooming is disabled we could need to reposition the view in
      // order to keep the element on-screen
      let zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, getBrowser().scale);
      AnimatedZoom.animateTo(zoomRect);
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
    this._suggestionsContainer = document.getElementById("form-helper-suggestions-container");
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

    // Listen key events for fields that are non-editable
    window.addEventListener("keydown", this, true);
    window.addEventListener("keyup", this, true);
    window.addEventListener("keypress", this, true);

    // Listen some events to show/hide the autocomplete box
    Elements.browsers.addEventListener("PanBegin", this, false);
    Elements.browsers.addEventListener("PanFinished", this, false);
    window.addEventListener("AnimatedZoomBegin", this, false);
    window.addEventListener("AnimatedZoomEnd", this, false);
  },

  _currentBrowser: null,
  show: function formHelperShow(aElement, aHasPrevious, aHasNext) {
    this._currentBrowser = Browser.selectedBrowser;
    this._currentCaretRect = null;

    // Update the next/previous commands
    this._cmdPrevious.setAttribute("disabled", !aHasPrevious);
    this._cmdNext.setAttribute("disabled", !aHasNext);
    this._hasSuggestions = false;
    this._open = true;

    let lastElement = this._currentElement || null;
    this._currentElement = {
      id: aElement.id,
      name: aElement.name,
      title: aElement.title,
      value: aElement.value,
      maxLength: aElement.maxLength,
      type: aElement.type,
      isAutocomplete: aElement.isAutocomplete,
      list: aElement.choices
    }

    this._updateContainerForSelect(lastElement, this._currentElement);
    this._zoom(Rect.fromRect(aElement.rect), Rect.fromRect(aElement.caretRect));
    this._updateSuggestionsFor(this._currentElement);

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
    this._resetSuggestions();

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

      case "PanBegin":
        // The previous/next buttons should be hidden during a manual panning
        // operation but not doing a zoom operation since this happen on both
        // manual dblClick and navigation between the fields by clicking the
        // buttons
        this._container.style.visibility = "hidden";

      case "AnimatedZoomBegin":
        // Changing the hidden attribute here create bugs with the scrollbox
        // arrows because the binding will miss some underflow events
        if (this._hasSuggestions)
          this._suggestionsContainer.style.visibility = "hidden";
        break;

      case "PanFinished":
        this._container.style.visibility = "visible";

      case "AnimatedZoomEnd":
        if (this._hasSuggestions) {
          this._suggestionsContainer.style.visibility = "visible";
          this._ensureSuggestionsVisible();
        }
        break;

      case "URLChanged":
        if (aEvent.detail && aEvent.target == getBrowser())
          this.hide();
        break;

      case "keydown":
      case "keypress":
      case "keyup":
        Browser.keySender.handleEvent(aEvent);
        break;

      case "SizeChanged":
        setTimeout(function(self) {
          SelectHelperUI.sizeToContent();
          self._zoom(self._currentElementRect, self._currentCaretRect);
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
                     : SelectHelperUI.show(json.current.choices, json.current.title);
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
        this._updateSuggestionsFor(json.current);
        break;

       case "FormAssist:Update":
        Browser.hideSidebars();
        Browser.hideTitlebar();
        this._zoom(null, Rect.fromRect(json.caretRect));
        break;

      case "DOMWillOpenModalDialog":
        if (aMessage.target == Browser.selectedBrowser && this._container.isActive)
          this._container.style.display = "none";
        break;

      case "DOMModalDialogClosed":
        if (aMessage.target == Browser.selectedBrowser && this._container.isActive)
          this._container.style.display = "-moz-box";
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
    if (aVal) {
      this._zoomStart();
      this._container.show(this);
    } else {
      this._zoomFinish();
      this._currentElement = null;
      this._container.hide(this);

      // Since the style is overrided when a popup is shown, it needs to be
      // resetted here to let the default CSS works
      this._container.style.display = "";
    }

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("FormUI", true, true, window, aVal);
    this._container.dispatchEvent(evt);
  },

  _hasSuggestions: false,
  _updateSuggestionsFor: function _formHelperUpdateAutocompleteFor(aElement) {
    let suggestions = this._getAutocompleteSuggestions(aElement);
    if (!suggestions.length) {
      this._resetSuggestions();
      return;
    }
    // Keeps the suggestions element hidden while is it not positionned to the
    // correct place
    this._suggestionsContainer.style.visibility = "hidden";
    this._suggestionsContainer.hidden = false;

    // the scrollX/scrollY position can change because of the animated zoom so
    // delay the suggestions positioning
    if (AnimatedZoom.isZooming()) {
      let self = this;
      window.addEventListener("AnimatedZoomEnd", function() {
        window.removeEventListener("AnimatedZoomEnd", arguments.callee, true);
          self._updateSuggestionsFor(aElement);
      }, true);
      return;
    }

    let container = this._suggestionsContainer.firstChild;
    while (container.hasChildNodes())
      container.removeChild(container.lastChild);

    let fragment = document.createDocumentFragment();
    for (let i = 0; i < suggestions.length; i++) {
      let value = suggestions[i];
      let button = document.createElement("label");
      button.setAttribute("value", value);
      button.className = "form-helper-suggestions-label";
      fragment.appendChild(button);
    }
    container.appendChild(fragment);

    this._hasSuggestions = true;
    this._ensureSuggestionsVisible();
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

  _resetSuggestions: function _formHelperResetAutocomplete() {
    this._suggestionsContainer.hidden = true;
    this._hasSuggestions = false;
  },

  /** 
   * This method positionned the list of suggestions on the screen using
   * a 'virtual' element as referrer that match the real content element
   * This method called element.getBoundingClientRect() many times and can be
   * expensive, do not called it too many times.
   */
  _ensureSuggestionsVisible: function _formHelperEnsureSuggestionsVisible() {
    let container = this._suggestionsContainer;
    container.firstChild.style.maxWidth = (window.innerWidth * 0.75) + "px";

    let browser = getBrowser();
    let rect = this._currentElementRect.clone().scale(browser.scale, browser.scale);
    let scroll = browser.getRootView().getPosition();

    // The sidebars scroll needs to be taken into account, otherwise the arrows
    // can be misplaced if the sidebars are open
    let [leftVis, rightVis, leftW, rightW] = Browser.computeSidebarVisibility();
    let leftOffset = leftVis * leftW;
    let rightOffset = rightVis * rightW;
    let topOffset = (BrowserUI.toolbarH - Browser.getScrollboxPosition(Browser.pageScrollboxScroller).y);
    let virtualContentRect = {
      width: rect.width,
      height: rect.height,
      left: Math.ceil(rect.left - scroll.x + leftOffset - rightOffset),
      right: Math.floor(rect.left + rect.width - scroll.x + leftOffset - rightOffset),
      top: Math.ceil(rect.top - scroll.y + topOffset),
      bottom: Math.floor(rect.top + rect.height - scroll.y + topOffset)
    };

    // If the suggestions are out of view there is no need to display it
    let browserRect = Rect.fromRect(browser.getBoundingClientRect());
    if (BrowserUI.isToolbarLocked()) {
      // If the toolbar is locked, it can appear over the field in such a way
      // that the field is hidden
      browserRect = new Rect(browserRect.left + leftOffset - rightOffset, browserRect.top + BrowserUI.toolbarH,
                             browserRect.width + leftOffset - rightOffset, browserRect.height - BrowserUI.toolbarH);
    }

    if (browserRect.intersect(Rect.fromRect(virtualContentRect)).isEmpty()) {
      container.style.visibility = "hidden";
      return;
    }

    // Adding rect.height to the top moves the arrowbox below the virtual field
    let left = rect.left - scroll.x + leftOffset - rightOffset;
    let top = rect.top - scroll.y + topOffset + (rect.height);

    // Ensure parts of the arrowbox are not outside the window
    // XXX do we want to correct when it is out of view on the left side too?
    let arrowboxRect = Rect.fromRect(container.getBoundingClientRect());
    if (left + arrowboxRect.width > window.innerWidth)
      left -= (left + arrowboxRect.width - window.innerWidth);
    container.left = left;

    // Do not position the suggestions over the navigation buttons
    let buttonsHeight = this._container.getBoundingClientRect().height;
    if (top + arrowboxRect.height >= window.innerHeight - buttonsHeight)
      top -= (rect.height + arrowboxRect.height);
    container.top = top;
  
    // Create a virtual element to point to
    let virtualContentElement = {
      getBoundingClientRect: function() {
        return virtualContentRect;
      }
    };
    container.anchorTo(virtualContentElement);
    container.style.visibility = "visible";
  },

  /** Update the form helper container to reflect new element user is editing. */
  _updateContainer: function _formHelperUpdateContainer(aLastElement, aCurrentElement) {
    this._updateContainerForSelect(aLastElement, aCurrentElement);

    this._container.contentHasChanged();
  },

  /** Helper for _updateContainer that handles the case where the new element is a select. */
  _updateContainerForSelect: function _formHelperUpdateContainerForSelect(aLastElement, aCurrentElement) {
    let lastHasChoices = aLastElement && (aLastElement.list != null);
    let currentHasChoices = aCurrentElement && (aCurrentElement.list != null);

    if (currentHasChoices)
      SelectHelperUI.show(aCurrentElement.list, aCurrentElement.title);
    else if (lastHasChoices)
      SelectHelperUI.hide();
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
      AnimatedZoom.animateTo(zoomRect);
    } else if (aElementRect && !Browser.selectedTab.allowZoom && autozoomEnabled) {
      // Even if zooming is disabled we could need to reposition the view in
      // order to keep the element on-screen
      zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, browser.scale);
      AnimatedZoom.animateTo(zoomRect);
    }

    this._ensureCaretVisible(aCaretRect);
  },

  _ensureCaretVisible: function _ensureCaretVisible(aCaretRect) {
    if (!aCaretRect || !Services.prefs.getBoolPref("formhelper.autozoom.caret"))
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
    let caretRect = aCaretRect.clone().scale(browser.scale, browser.scale);

    let scroll = browser.getRootView().getPosition();
    zoomRect = new Rect(scroll.x, scroll.y, zoomRect.width, zoomRect.height);
    if (zoomRect.contains(caretRect))
      return;

    let [deltaX, deltaY] = this._getOffsetForCaret(caretRect, zoomRect);
    if (deltaX != 0 || deltaY != 0) {
      let view = browser.getRootView();
      view.scrollBy(deltaX, deltaY);
    }
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
      let command = commands.childNodes[i];
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

var CharsetMenu = {
  _strings: null,
  _charsets: null,

  get strings() {
    if (!this._strings)
      this._strings = Services.strings.createBundle("chrome://global/locale/charsetTitles.properties");
    return this._strings;
  },

  init: function() {
    PageActions.register("pageaction-charset", this.updatePageAction, this);
  },

  updatePageAction: function(aNode) {
    let pref = Services.prefs.getComplexValue("browser.menu.showCharacterEncoding", Ci.nsIPrefLocalizedString).data;
    if (pref == "true") {
      let charset = getBrowser().documentCharsetInfo.forcedCharset;
      if (charset) {
        charset = charset.trim().toLowerCase();
        aNode.setAttribute("description", this.strings.GetStringFromName(charset + ".title"));
      } else if (aNode.hasAttribute("description")) {
        aNode.removeAttribute("description");
      }
    }
    return ("true" == pref)
  },

  _toMenuItems: function(aCharsets, aCurrent) {
    let ret = [];
    aCharsets.forEach(function (aSet) {
      try {
        let string = aSet.trim().toLowerCase();
        ret.push({
          label: this.strings.GetStringFromName(string + ".title"),
          value: string,
          selected: (string == aCurrent)
        });
      } catch(ex) { }
    }, this);
    return ret;
  },

  menu : {
    dispatchEvent: function(aEvent) {
      if (aEvent.type == "command")
        CharsetMenu.setCharset(this.menupopup.children[this.selectedIndex].value);
    },
    menupopup: {
      hasAttribute: function(aAttr) { return false; },
    },
    selectedIndex: -1
  },

  get charsets() {
    if (!this._charsets) {
      this._charsets = Services.prefs.getComplexValue("intl.charsetmenu.browser.static", Ci.nsIPrefLocalizedString).data.split(",");
    }
    let charsets = this._charsets;
    let currentCharset = getBrowser().documentCharsetInfo.forcedCharset;
    
    if (currentCharset) {
      currentCharset = currentCharset.trim().toLowerCase();
      if (charsets.indexOf(currentCharset) == -1)
        charsets.splice(0, 0, currentCharset);
    }
    return this._toMenuItems(charsets, currentCharset);
  },

  show: function showCharsetMenu() {
    this.menu.menupopup.children = this.charsets;
    MenuListHelperUI.show(this.menu);
  },

  setCharset: function setCharset(aCharset) {
    let browser = getBrowser();
    browser.messageManager.sendAsyncMessage("Browser:SetCharset", {
      charset: aCharset
    });
    let history = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
    history.setCharsetForURI(browser.documentURI, aCharset);
  }

};
