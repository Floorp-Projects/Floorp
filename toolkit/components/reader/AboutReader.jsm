/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "AboutReader" ];

Cu.import("resource://gre/modules/ReaderMode.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Rect", "resource://gre/modules/Geometry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry", "resource://gre/modules/UITelemetry.jsm");

const READINGLIST_COMMAND_ID = "readingListSidebar";

let gStrings = Services.strings.createBundle("chrome://global/locale/aboutReader.properties");

let AboutReader = function(mm, win, articlePromise) {
  let url = this._getOriginalUrl(win);
  if (!(url.startsWith("http://") || url.startsWith("https://"))) {
    Cu.reportError("Only http:// and https:// URLs can be loaded in about:reader");
    win.location.href = "about:blank";
    return;
  }

  let doc = win.document;

  this._mm = mm;
  this._mm.addMessageListener("Reader:Added", this);
  this._mm.addMessageListener("Reader:Removed", this);
  this._mm.addMessageListener("Sidebar:VisibilityChange", this);
  this._mm.addMessageListener("ReadingList:VisibilityStatus", this);

  this._docRef = Cu.getWeakReference(doc);
  this._winRef = Cu.getWeakReference(win);

  this._article = null;

  if (articlePromise) {
    this._articlePromise = articlePromise;
  }

  this._headerElementRef = Cu.getWeakReference(doc.getElementById("reader-header"));
  this._domainElementRef = Cu.getWeakReference(doc.getElementById("reader-domain"));
  this._titleElementRef = Cu.getWeakReference(doc.getElementById("reader-title"));
  this._creditsElementRef = Cu.getWeakReference(doc.getElementById("reader-credits"));
  this._contentElementRef = Cu.getWeakReference(doc.getElementById("moz-reader-content"));
  this._toolbarElementRef = Cu.getWeakReference(doc.getElementById("reader-toolbar"));
  this._messageElementRef = Cu.getWeakReference(doc.getElementById("reader-message"));

  this._scrollOffset = win.pageYOffset;

  doc.addEventListener("click", this, false);

  win.addEventListener("unload", this, false);
  win.addEventListener("scroll", this, false);
  win.addEventListener("resize", this, false);

  doc.addEventListener("visibilitychange", this, false);

  this._setupStyleDropdown();
  this._setupButton("close-button", this._onReaderClose.bind(this), "aboutReader.toolbar.close");
  this._setupButton("share-button", this._onShare.bind(this), "aboutReader.toolbar.share");

  try {
    if (Services.prefs.getBoolPref("browser.readinglist.enabled")) {
      this._setupButton("toggle-button", this._onReaderToggle.bind(this, "button"), "aboutReader.toolbar.addToReadingList");
      this._setupButton("list-button", this._onList.bind(this), "aboutReader.toolbar.openReadingList");
      this._setupButton("remove-button", this._onReaderToggle.bind(this, "footer"),
        "aboutReader.footer.deleteThisArticle", "aboutReader.footer.deleteThisArticle");
      this._doc.getElementById("reader-footer").setAttribute('readinglist-enabled', "true");
    }
  } catch (e) {
    // Pref doesn't exist.
  }

  let colorSchemeValues = JSON.parse(Services.prefs.getCharPref("reader.color_scheme.values"));
  let colorSchemeOptions = colorSchemeValues.map((value) => {
    return { name: gStrings.GetStringFromName("aboutReader.colorScheme." + value),
             value: value,
             itemClass: value + "-button" };
  });

  let colorScheme = Services.prefs.getCharPref("reader.color_scheme");
  this._setupSegmentedButton("color-scheme-buttons", colorSchemeOptions, colorScheme, this._setColorSchemePref.bind(this));
  this._setColorSchemePref(colorScheme);

  let fontTypeSample = gStrings.GetStringFromName("aboutReader.fontTypeSample");
  let fontTypeOptions = [
    { name: fontTypeSample,
      description: gStrings.GetStringFromName("aboutReader.fontType.sans-serif"),
      value: "sans-serif",
      itemClass: "sans-serif-button"
    },
    { name: fontTypeSample,
      description: gStrings.GetStringFromName("aboutReader.fontType.serif"),
      value: "serif",
      itemClass: "serif-button" },
  ];

  let fontType = Services.prefs.getCharPref("reader.font_type");
  this._setupSegmentedButton("font-type-buttons", fontTypeOptions, fontType, this._setFontType.bind(this));
  this._setFontType(fontType);

  this._setupFontSizeButtons();

  // Track status of reader toolbar add/remove toggle button
  this._isReadingListItem = -1;
  this._updateToggleButton();

  // Setup initial ReadingList button styles.
  this._updateListButton();

  this._loadArticle();
}

AboutReader.prototype = {
  _BLOCK_IMAGES_SELECTOR: ".content p > img:only-child, " +
                          ".content p > a:only-child > img:only-child, " +
                          ".content .wp-caption img, " +
                          ".content figure img",

  get _doc() {
    return this._docRef.get();
  },

  get _win() {
    return this._winRef.get();
  },

  get _headerElement() {
    return this._headerElementRef.get();
  },

  get _domainElement() {
    return this._domainElementRef.get();
  },

  get _titleElement() {
    return this._titleElementRef.get();
  },

  get _creditsElement() {
    return this._creditsElementRef.get();
  },

  get _contentElement() {
    return this._contentElementRef.get();
  },

  get _toolbarElement() {
    return this._toolbarElementRef.get();
  },

  get _messageElement() {
    return this._messageElementRef.get();
  },

  get _isToolbarVertical() {
    if (this._toolbarVertical !== undefined) {
      return this._toolbarVertical;
    }
    return this._toolbarVertical = Services.prefs.getBoolPref("reader.toolbar.vertical");
  },

  receiveMessage: function (message) {
    switch (message.name) {
      case "Reader:Added": {
        // Page can be added by long-press pageAction, or by tap on banner icon.
        if (message.data.url == this._article.url) {
          if (this._isReadingListItem != 1) {
            this._isReadingListItem = 1;
            this._updateToggleButton();
          }
        }
        break;
      }
      case "Reader:Removed": {
        if (message.data.url == this._article.url) {
          if (this._isReadingListItem != 0) {
            this._isReadingListItem = 0;
            this._updateToggleButton();
          }
        }
        break;
      }

      // Notifys us of Sidebar updates, user clicks X to close,
      // checks View -> Sidebar -> (Bookmarks, Histroy, Readinglist, etc).
      case "Sidebar:VisibilityChange": {
        let data = message.data;
        this._updateListButtonStyle(data.isOpen && data.commandID === READINGLIST_COMMAND_ID);
        break;
      }

      // Returns requested status of current ReadingList Sidebar.
      case "ReadingList:VisibilityStatus": {
        this._updateListButtonStyle(message.data.isOpen);
        break;
      }
    }
  },

  handleEvent: function Reader_handleEvent(aEvent) {
    if (!aEvent.isTrusted)
      return;

    switch (aEvent.type) {
      case "click":
        let target = aEvent.target;
        while (target && target.id != "reader-popup")
          target = target.parentNode;
        if (!target)
          this._toggleToolbarVisibility();
        break;
      case "scroll":
        let isScrollingUp = this._scrollOffset > aEvent.pageY;
        this._setToolbarVisibility(isScrollingUp);
        this._scrollOffset = aEvent.pageY;
        break;
      case "resize":
        this._updateImageMargins();
        break;

      case "devicelight":
        this._handleDeviceLight(aEvent.value);
        break;

      case "visibilitychange":
        this._handleVisibilityChange();
        break;

      case "unload":
        this._mm.removeMessageListener("Reader:Added", this);
        this._mm.removeMessageListener("Reader:Removed", this);
        this._mm.removeMessageListener("Sidebar:VisibilityChange", this);
        this._mm.removeMessageListener("ReadingList:VisibilityStatus", this);
        this._windowUnloaded = true;
        break;
    }
  },

  _updateToggleButton: function Reader_updateToggleButton() {
    let button = this._doc.getElementById("toggle-button");

    if (this._isReadingListItem == 1) {
      button.classList.add("on");
      button.setAttribute("title", gStrings.GetStringFromName("aboutReader.toolbar.removeFromReadingList"));
    } else {
      button.classList.remove("on");
      button.setAttribute("title", gStrings.GetStringFromName("aboutReader.toolbar.addToReadingList"));
    }
    this._updateFooter();
  },

  _requestReadingListStatus: function Reader_requestReadingListStatus() {
    let handleListStatusData = (message) => {
      this._mm.removeMessageListener("Reader:ListStatusData", handleListStatusData);

      let args = message.data;
      if (args.url == this._article.url) {
        if (this._isReadingListItem != args.inReadingList) {
          let isInitialStateChange = (this._isReadingListItem == -1);
          this._isReadingListItem = args.inReadingList;
          this._updateToggleButton();

          // Display the toolbar when all its initial component states are known
          if (isInitialStateChange) {
            // Toolbar display is updated here to avoid it appearing in the middle of the screen on page load. See bug 1145567.
            this._win.setTimeout(() => {
              this._toolbarElement.style.display = "block";
              // Delay showing the toolbar to have a nice slide from bottom animation.
              this._win.setTimeout(() => this._setToolbarVisibility(true), 200);
            }, 500);
          }
        }
      }
    };

    this._mm.addMessageListener("Reader:ListStatusData", handleListStatusData);
    this._mm.sendAsyncMessage("Reader:ListStatusRequest", { url: this._article.url });
  },

  _onReaderClose: function Reader_onToggle() {
    this._win.location.href = this._getOriginalUrl();
  },

  _onReaderToggle: function Reader_onToggle(aMethod) {
    if (!this._article)
      return;

    if (this._isReadingListItem == 0) {
      this._mm.sendAsyncMessage("Reader:AddToList", { article: this._article });
      UITelemetry.addEvent("save.1", aMethod, null, "reader");
    } else {
      this._mm.sendAsyncMessage("Reader:RemoveFromList", { url: this._article.url });
      UITelemetry.addEvent("unsave.1", aMethod, null, "reader");
    }
  },

  _onShare: function Reader_onShare() {
    if (!this._article)
      return;

    this._mm.sendAsyncMessage("Reader:Share", {
      url: this._article.url,
      title: this._article.title
    });
    UITelemetry.addEvent("share.1", "list", null);
  },

  /**
   * To help introduce ReadingList, we want to automatically
   * open the Desktop sidebar the first time ReaderMode is used.
   */
  _showListIntro: function() {
    this._mm.sendAsyncMessage("ReadingList:ShowIntro");
  },

  /**
   * Toggle ReadingList Sidebar visibility. SidebarUI will trigger
   * _updateListButtonStyle().
   */
  _onList: function() {
    this._mm.sendAsyncMessage("ReadingList:ToggleVisibility");
  },

  /**
   * Request ReadingList Sidebar-button visibility status update.
   * Only desktop currently responds to this message.
   */
  _updateListButton: function() {
    this._mm.sendAsyncMessage("ReadingList:GetVisibility");
  },

  /**
   * Update ReadingList toggle button styles.
   * @param   isVisible
   *          What Sidebar ReadingList visibility style the List
   *          toggle-button should be set to reflect, and what
   *          button-action the tip will provide.
   */
  _updateListButtonStyle: function(isVisible) {
    let classes = this._doc.getElementById("list-button").classList;
    if (isVisible) {
      classes.add("on");
      // When on, action tip is "close".
      this._setButtonTip("list-button", "aboutReader.toolbar.closeReadingList");
    } else {
      classes.remove("on");
      // When off, action tip is "open".
      this._setButtonTip("list-button", "aboutReader.toolbar.openReadingList");
    }
  },

  _setFontSize: function Reader_setFontSize(newFontSize) {
    let containerClasses = this._doc.getElementById("container").classList;

    if (this._fontSize > 0)
      containerClasses.remove("font-size" + this._fontSize);

    this._fontSize = newFontSize;
    containerClasses.add("font-size" + this._fontSize);

    this._mm.sendAsyncMessage("Reader:SetIntPref", {
      name: "reader.font_size",
      value: this._fontSize
    });
  },

  _setupFontSizeButtons: function() {
    const FONT_SIZE_MIN = 1;
    const FONT_SIZE_MAX = 9;

    // Sample text shown in Android UI.
    let sampleText = this._doc.getElementById("font-size-sample");
    sampleText.textContent = gStrings.GetStringFromName("aboutReader.fontTypeSample");

    let currentSize = Services.prefs.getIntPref("reader.font_size");
    currentSize = Math.max(FONT_SIZE_MIN, Math.min(FONT_SIZE_MAX, currentSize));

    let plusButton = this._doc.getElementById("font-size-plus");
    let minusButton = this._doc.getElementById("font-size-minus");

    function updateControls() {
      if (currentSize === FONT_SIZE_MIN) {
        minusButton.setAttribute("disabled", true);
      } else {
        minusButton.removeAttribute("disabled");
      }
      if (currentSize === FONT_SIZE_MAX) {
        plusButton.setAttribute("disabled", true);
      } else {
        plusButton.removeAttribute("disabled");
      }
    }

    updateControls();
    this._setFontSize(currentSize);

    plusButton.addEventListener("click", (event) => {
      if (!event.isTrusted) {
        return;
      }
      event.stopPropagation();

      if (currentSize >= FONT_SIZE_MAX) {
        return;
      }

      currentSize++;
      updateControls();
      this._setFontSize(currentSize);
    }, true);

    minusButton.addEventListener("click", (event) => {
      if (!event.isTrusted) {
        return;
      }
      event.stopPropagation();

      if (currentSize <= FONT_SIZE_MIN) {
        return;
      }

      currentSize--;
      updateControls();
      this._setFontSize(currentSize);
    }, true);
  },

  _updateFooter: function RupdateFooter() {
    let footer = this._doc.getElementById("reader-footer");
    if (!this._article || this._isReadingListItem == 0 ||
        footer.getAttribute("readinglist-enabled") != "true") {
      footer.style.display = "none";
      return;
    }
    footer.style.display = null;
  },

  _handleDeviceLight: function Reader_handleDeviceLight(newLux) {
    // Desired size of the this._luxValues array.
    let luxValuesSize = 10;
    // Add new lux value at the front of the array.
    this._luxValues.unshift(newLux);
    // Add new lux value to this._totalLux for averaging later.
    this._totalLux += newLux;

    // Don't update when length of array is less than luxValuesSize except when it is 1.
    if (this._luxValues.length < luxValuesSize) {
      // Use the first lux value to set the color scheme until our array equals luxValuesSize.
      if (this._luxValues.length == 1) {
        this._updateColorScheme(newLux);
      }
      return;
    }
    // Holds the average of the lux values collected in this._luxValues.
    let averageLuxValue = this._totalLux/luxValuesSize;

    this._updateColorScheme(averageLuxValue);
    // Pop the oldest value off the array.
    let oldLux = this._luxValues.pop();
    // Subtract oldLux since it has been discarded from the array.
    this._totalLux -= oldLux;
  },

  _handleVisibilityChange: function Reader_handleVisibilityChange() {
    // ReadingList / Sidebar state might change while we're not the selected tab.
    if (this._doc.visibilityState === "visible") {
      this._updateListButton();
    }

    let colorScheme = Services.prefs.getCharPref("reader.color_scheme");
    if (colorScheme != "auto") {
      return;
    }

    // Turn off the ambient light sensor if the page is hidden
    this._enableAmbientLighting(!this._doc.hidden);
  },

  // Setup or teardown the ambient light tracking system.
  _enableAmbientLighting: function Reader_enableAmbientLighting(enable) {
    if (enable) {
      this._win.addEventListener("devicelight", this, false);
      this._luxValues = [];
      this._totalLux = 0;
    } else {
      this._win.removeEventListener("devicelight", this, false);
      delete this._luxValues;
      delete this._totalLux;
    }
  },

  _updateColorScheme: function Reader_updateColorScheme(luxValue) {
    // Upper bound value for "dark" color scheme beyond which it changes to "light".
    let upperBoundDark = 50;
    // Lower bound value for "light" color scheme beyond which it changes to "dark".
    let lowerBoundLight = 10;
    // Threshold for color scheme change.
    let colorChangeThreshold = 20;

    // Ignore changes that are within a certain threshold of previous lux values.
    if ((this._colorScheme === "dark" && luxValue < upperBoundDark) ||
        (this._colorScheme === "light" && luxValue > lowerBoundLight))
      return;

    if (luxValue < colorChangeThreshold)
      this._setColorScheme("dark");
    else
      this._setColorScheme("light");
  },

  _setColorScheme: function Reader_setColorScheme(newColorScheme) {
    // "auto" is not a real color scheme
    if (this._colorScheme === newColorScheme || newColorScheme === "auto")
      return;

    let bodyClasses = this._doc.body.classList;

    if (this._colorScheme)
      bodyClasses.remove(this._colorScheme);

    this._colorScheme = newColorScheme;
    bodyClasses.add(this._colorScheme);
  },

  // Pref values include "dark", "light", and "auto", which automatically switches
  // between light and dark color schemes based on the ambient light level.
  _setColorSchemePref: function Reader_setColorSchemePref(colorSchemePref) {
    this._enableAmbientLighting(colorSchemePref === "auto");
    this._setColorScheme(colorSchemePref);

    this._mm.sendAsyncMessage("Reader:SetCharPref", {
      name: "reader.color_scheme",
      value: colorSchemePref
    });
  },

  _setFontType: function Reader_setFontType(newFontType) {
    if (this._fontType === newFontType)
      return;

    let bodyClasses = this._doc.body.classList;

    if (this._fontType)
      bodyClasses.remove(this._fontType);

    this._fontType = newFontType;
    bodyClasses.add(this._fontType);

    this._mm.sendAsyncMessage("Reader:SetCharPref", {
      name: "reader.font_type",
      value: this._fontType
    });
  },

  _getToolbarVisibility: function Reader_getToolbarVisibility() {
    return this._toolbarElement.hasAttribute("visible");
  },

  _setToolbarVisibility: function Reader_setToolbarVisibility(visible) {
    let dropdown = this._doc.getElementById("style-dropdown");
    dropdown.classList.remove("open");

    if (this._getToolbarVisibility() === visible) {
      return;
    }

    if (visible) {
      this._toolbarElement.setAttribute("visible", true);
    } else {
      this._toolbarElement.removeAttribute("visible");
    }
    this._setSystemUIVisibility(visible);

    if (!visible) {
      this._mm.sendAsyncMessage("Reader:ToolbarHidden");
    }
    this._updateFooter();
  },

  _toggleToolbarVisibility: function Reader_toggleToolbarVisibility() {
    this._setToolbarVisibility(!this._getToolbarVisibility());
  },

  _setSystemUIVisibility: function Reader_setSystemUIVisibility(visible) {
    this._mm.sendAsyncMessage("Reader:SystemUIVisibility", { visible: visible });
  },

  _loadArticle: Task.async(function* () {
    let url = this._getOriginalUrl();
    this._showProgressDelayed();

    let article;
    if (this._articlePromise) {
      article = yield this._articlePromise;
    } else {
      article = yield this._getArticle(url);
    }

    if (this._windowUnloaded) {
      return;
    }

    if (article) {
      this._showContent(article);
    } else if (this._articlePromise) {
      // If we were promised an article, show an error message if there's a failure.
      this._showError();
    } else {
      // Otherwise, just load the original URL. We can encounter this case when
      // loading an about:reader URL directly (e.g. opening a reading list item).
      this._win.location.href = url;
    }
  }),

  _getArticle: function(url) {
    return new Promise((resolve, reject) => {
      let listener = (message) => {
        this._mm.removeMessageListener("Reader:ArticleData", listener);
        resolve(message.data.article);
      };
      this._mm.addMessageListener("Reader:ArticleData", listener);
      this._mm.sendAsyncMessage("Reader:ArticleGet", { url: url });
    });
  },

  _requestFavicon: function Reader_requestFavicon() {
    let handleFaviconReturn = (message) => {
      this._mm.removeMessageListener("Reader:FaviconReturn", handleFaviconReturn);
      this._loadFavicon(message.data.url, message.data.faviconUrl);
    };

    this._mm.addMessageListener("Reader:FaviconReturn", handleFaviconReturn);
    this._mm.sendAsyncMessage("Reader:FaviconRequest", { url: this._article.url });
  },

  _loadFavicon: function Reader_loadFavicon(url, faviconUrl) {
    if (this._article.url !== url)
      return;

    let doc = this._doc;

    let link = doc.createElement('link');
    link.rel = 'shortcut icon';
    link.href = faviconUrl;

    doc.getElementsByTagName('head')[0].appendChild(link);
  },

  _updateImageMargins: function Reader_updateImageMargins() {
    let windowWidth = this._win.innerWidth;
    let bodyWidth = this._doc.body.clientWidth;

    let setImageMargins = function(img) {
      // If the image is at least as wide as the window, make it fill edge-to-edge on mobile.
      if (img.naturalWidth >= windowWidth) {
        img.setAttribute("moz-reader-full-width", true);
      } else {
        img.removeAttribute("moz-reader-full-width");
      }

      // If the image is at least half as wide as the body, center it on desktop.
      if (img.naturalWidth >= bodyWidth/2) {
        img.setAttribute("moz-reader-center", true);
      } else {
        img.removeAttribute("moz-reader-center");
      }
    }

    let imgs = this._doc.querySelectorAll(this._BLOCK_IMAGES_SELECTOR);
    for (let i = imgs.length; --i >= 0;) {
      let img = imgs[i];

      if (img.naturalWidth > 0) {
        setImageMargins(img);
      } else {
        img.onload = function() {
          setImageMargins(img);
        }
      }
    }
  },

  _maybeSetTextDirection: function Read_maybeSetTextDirection(article){
    if(!article.dir)
      return;

    //Set "dir" attribute on content
    this._contentElement.setAttribute("dir", article.dir);
    this._headerElement.setAttribute("dir", article.dir);
  },

  _showError: function() {
    this._headerElement.style.display = "none";
    this._contentElement.style.display = "none";

    let errorMessage = gStrings.GetStringFromName("aboutReader.loadError");
    this._messageElement.textContent = errorMessage;
    this._messageElement.style.display = "block";

    this._doc.title = errorMessage;

    this._error = true;
  },

  // This function is the JS version of Java's StringUtils.stripCommonSubdomains.
  _stripHost: function Reader_stripHost(host) {
    if (!host)
      return host;

    let start = 0;

    if (host.startsWith("www."))
      start = 4;
    else if (host.startsWith("m."))
      start = 2;
    else if (host.startsWith("mobile."))
      start = 7;

    return host.substring(start);
  },

  _showContent: function Reader_showContent(article) {
    this._messageElement.style.display = "none";

    this._article = article;

    this._domainElement.href = article.url;
    let articleUri = Services.io.newURI(article.url, null, null);
    this._domainElement.textContent = this._stripHost(articleUri.host);
    this._creditsElement.textContent = article.byline;

    this._titleElement.textContent = article.title;
    this._doc.title = article.title;

    this._headerElement.style.display = "block";

    let parserUtils = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils);
    let contentFragment = parserUtils.parseFragment(article.content,
      Ci.nsIParserUtils.SanitizerDropForms | Ci.nsIParserUtils.SanitizerAllowStyle,
      false, articleUri, this._contentElement);
    this._contentElement.innerHTML = "";
    this._contentElement.appendChild(contentFragment);
    this._maybeSetTextDirection(article);

    this._contentElement.style.display = "block";
    this._updateImageMargins();
    this._requestReadingListStatus();

    this._showListIntro();
    this._requestFavicon();
    this._doc.body.classList.add("loaded");
  },

  _hideContent: function Reader_hideContent() {
    this._headerElement.style.display = "none";
    this._contentElement.style.display = "none";
  },

  _showProgressDelayed: function Reader_showProgressDelayed() {
    this._win.setTimeout(function() {
      // No need to show progress if the article has been loaded,
      // if the window has been unloaded, or if there was an error
      // trying to load the article.
      if (this._article || this._windowUnloaded || this._error) {
        return;
      }

      this._headerElement.style.display = "none";
      this._contentElement.style.display = "none";

      this._messageElement.textContent = gStrings.GetStringFromName("aboutReader.loading");
      this._messageElement.style.display = "block";
    }.bind(this), 300);
  },

  /**
   * Returns the original article URL for this about:reader view.
   */
  _getOriginalUrl: function(win) {
    let url = win ? win.location.href : this._win.location.href;
    return ReaderMode.getOriginalUrl(url) || url;
  },

  _setupSegmentedButton: function Reader_setupSegmentedButton(id, options, initialValue, callback) {
    let doc = this._doc;
    let segmentedButton = doc.getElementById(id);

    for (let i = 0; i < options.length; i++) {
      let option = options[i];

      let item = doc.createElement("button");

      // Put the name in a div so that Android can hide it.
      let div = doc.createElement("div");
      div.textContent = option.name;
      div.classList.add("name");
      item.appendChild(div);

      if (option.itemClass !== undefined)
        item.classList.add(option.itemClass);

      if (option.description !== undefined) {
        let description = doc.createElement("div");
        description.textContent = option.description;
        description.classList.add("description");
        item.appendChild(description);
      }

      segmentedButton.appendChild(item);

      item.addEventListener("click", function(aEvent) {
        if (!aEvent.isTrusted)
          return;

        aEvent.stopPropagation();

        // Just pass the ID of the button as an extra and hope the ID doesn't change
        // unless the context changes
        UITelemetry.addEvent("action.1", "button", null, id);

        let items = segmentedButton.children;
        for (let j = items.length - 1; j >= 0; j--) {
          items[j].classList.remove("selected");
        }

        item.classList.add("selected");
        callback(option.value);
      }.bind(this), true);

      if (option.value === initialValue)
        item.classList.add("selected");
    }
  },

  _setupButton: function(id, callback, titleEntity, textEntity) {
    this._setButtonTip(id, titleEntity);

    let button = this._doc.getElementById(id);
    if (textEntity)
      button.textContent = gStrings.GetStringFromName(textEntity);
    button.removeAttribute("hidden");
    button.addEventListener("click", function(aEvent) {
      if (!aEvent.isTrusted)
        return;

      aEvent.stopPropagation();
      callback();
    }, true);
  },

  /**
   * Sets a toolTip for a button. Performed at initial button setup
   * and dynamically as button state changes.
   * @param   Localizable string providing UI element usage tip.
   */
  _setButtonTip: function(id, titleEntity) {
    let button = this._doc.getElementById(id);
    button.setAttribute("title", gStrings.GetStringFromName(titleEntity));
  },

  _setupStyleDropdown: function Reader_setupStyleDropdown() {
    let doc = this._doc;
    let win = this._win;

    let dropdown = doc.getElementById("style-dropdown");
    let dropdownToggle = dropdown.querySelector(".dropdown-toggle");
    let dropdownPopup = dropdown.querySelector(".dropdown-popup");

    // Helper function used to position the popup on desktop,
    // where there is a vertical toolbar.
    function updatePopupPosition() {
      let toggleHeight = dropdownToggle.offsetHeight;
      let toggleTop = dropdownToggle.offsetTop;
      let popupTop = toggleTop - toggleHeight / 2;
      dropdownPopup.style.top = popupTop + "px";
    }

    if (this._isToolbarVertical) {
      win.addEventListener("resize", event => {
        if (!event.isTrusted)
          return;

        // Wait for reflow before calculating the new position of the popup.
        win.setTimeout(updatePopupPosition, 0);
      }, true);
    }

    dropdownToggle.setAttribute("title", gStrings.GetStringFromName("aboutReader.toolbar.typeControls"));
    dropdownToggle.addEventListener("click", event => {
      if (!event.isTrusted)
        return;

      event.stopPropagation();

      if (dropdown.classList.contains("open")) {
        dropdown.classList.remove("open");
      } else {
        dropdown.classList.add("open");
        if (this._isToolbarVertical) {
          updatePopupPosition();
        }
      }
    }, true);
  },
};
