/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "AboutReader" ];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Rect", "resource://gre/modules/Geometry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry", "resource://gre/modules/UITelemetry.jsm");

function dump(s) {
  Services.console.logStringMessage("AboutReader: " + s);
}

let gStrings = Services.strings.createBundle("chrome://global/locale/aboutReader.properties");

let AboutReader = function(mm, win) {
  let doc = win.document;

  this._mm = mm;
  this._mm.addMessageListener("Reader:Added", this);
  this._mm.addMessageListener("Reader:Removed", this);

  this._docRef = Cu.getWeakReference(doc);
  this._winRef = Cu.getWeakReference(win);

  this._article = null;

  this._headerElementRef = Cu.getWeakReference(doc.getElementById("reader-header"));
  this._domainElementRef = Cu.getWeakReference(doc.getElementById("reader-domain"));
  this._titleElementRef = Cu.getWeakReference(doc.getElementById("reader-title"));
  this._creditsElementRef = Cu.getWeakReference(doc.getElementById("reader-credits"));
  this._contentElementRef = Cu.getWeakReference(doc.getElementById("reader-content"));
  this._toolbarElementRef = Cu.getWeakReference(doc.getElementById("reader-toolbar"));
  this._messageElementRef = Cu.getWeakReference(doc.getElementById("reader-message"));

  this._toolbarEnabled = false;

  this._scrollOffset = win.pageYOffset;

  let body = doc.body;
  body.addEventListener("touchstart", this, false);
  body.addEventListener("click", this, false);

  win.addEventListener("unload", this, false);
  win.addEventListener("scroll", this, false);
  win.addEventListener("resize", this, false);

  doc.addEventListener("visibilitychange", this, false);

  this._setupStyleDropdown();
  this._setupButton("close-button", this._onReaderClose.bind(this), "aboutReader.toolbar.close");
  this._setupButton("toggle-button", this._onReaderToggle.bind(this), "aboutReader.toolbar.addToReadingList");
  this._setupButton("share-button", this._onShare.bind(this), "aboutReader.toolbar.share");
  this._setupButton("list-button", this._onList.bind(this), "aboutReader.toolbar.openReadingList");

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
      description: gStrings.GetStringFromName("aboutReader.fontType.serif"),
      value: "serif",
      linkClass: "serif" },
    { name: fontTypeSample,
      description: gStrings.GetStringFromName("aboutReader.fontType.sans-serif"),
      value: "sans-serif",
      linkClass: "sans-serif"
    },
  ];

  let fontType = Services.prefs.getCharPref("reader.font_type");
  this._setupSegmentedButton("font-type-buttons", fontTypeOptions, fontType, this._setFontType.bind(this));
  this._setFontType(fontType);

  let fontSizeSample = gStrings.GetStringFromName("aboutReader.fontSizeSample");
  let fontSizeOptions = [
    { name: fontSizeSample,
      value: 1,
      linkClass: "font-size1-sample" },
    { name: fontSizeSample,
      value: 2,
      linkClass: "font-size2-sample" },
    { name: fontSizeSample,
      value: 3,
      linkClass: "font-size3-sample" },
    { name: fontSizeSample,
      value: 4,
      linkClass: "font-size4-sample" },
    { name: fontSizeSample,
      value: 5,
      linkClass: "font-size5-sample" }
  ];

  let fontSize = Services.prefs.getIntPref("reader.font_size");
  this._setupSegmentedButton("font-size-buttons", fontSizeOptions, fontSize, this._setFontSize.bind(this));
  this._setFontSize(fontSize);

  // Track status of reader toolbar add/remove toggle button
  this._isReadingListItem = -1;
  this._updateToggleButton();

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
    }
  },

  handleEvent: function Reader_handleEvent(aEvent) {
    if (!aEvent.isTrusted)
      return;

    switch (aEvent.type) {
      case "click":
        // XXX: Don't toggle the toolbar on double click. (See the "Gesture:DoubleTap" handler in Reader.js)
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
            this._setToolbarVisibility(true);
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

  _onReaderToggle: function Reader_onToggle() {
    if (!this._article)
      return;

    if (this._isReadingListItem == 0) {
      this._mm.sendAsyncMessage("Reader:AddToList", { article: this._article });
      UITelemetry.addEvent("save.1", "button", null, "reader");
    } else {
      this._mm.sendAsyncMessage("Reader:RemoveFromList", { url: this._article.url });
      UITelemetry.addEvent("unsave.1", "button", null, "reader");
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

  _onList: function() {
    // To be implemented (bug 1132665)
  },

  _setFontSize: function Reader_setFontSize(newFontSize) {
    let htmlClasses = this._doc.documentElement.classList;

    if (this._fontSize > 0)
      htmlClasses.remove("font-size" + this._fontSize);

    this._fontSize = newFontSize;
    htmlClasses.add("font-size" + this._fontSize);

    this._mm.sendAsyncMessage("Reader:SetIntPref", {
      name: "reader.font_size",
      value: this._fontSize
    });
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
    return !this._toolbarElement.classList.contains("toolbar-hidden");
  },

  _setToolbarVisibility: function Reader_setToolbarVisibility(visible) {
    let dropdown = this._doc.getElementById("style-dropdown");
    dropdown.classList.remove("open");

    if (!this._toolbarEnabled)
      return;

    // Don't allow visible toolbar until banner state is known
    if (this._isReadingListItem == -1)
      return;

    if (this._getToolbarVisibility() === visible)
      return;

    this._toolbarElement.classList.toggle("toolbar-hidden");
    this._setSystemUIVisibility(visible);

    if (!visible) {
      this._mm.sendAsyncMessage("Reader:ToolbarHidden");
    }
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

    let article = yield this._getArticle(url);
    if (article && article.url == url) {
      this._showContent(article);
    } else {
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
    let contentWidth = this._contentElement.offsetWidth;
    let maxWidthStyle = windowWidth + "px !important";

    let setImageMargins = function(img) {
      if (!img._originalWidth)
        img._originalWidth = img.offsetWidth;

      let imgWidth = img._originalWidth;

      // If the image is taking more than half of the screen, just make
      // it fill edge-to-edge.
      if (imgWidth < contentWidth && imgWidth > windowWidth * 0.55)
        imgWidth = windowWidth;

      let sideMargin = Math.max((contentWidth - windowWidth) / 2,
                                (contentWidth - imgWidth) / 2);

      let imageStyle = sideMargin + "px !important";
      let widthStyle = imgWidth + "px !important";

      let cssText = "max-width: " + maxWidthStyle + ";" +
                    "width: " + widthStyle + ";" +
                    "margin-left: " + imageStyle + ";" +
                    "margin-right: " + imageStyle + ";";

      img.style.cssText = cssText;
    }

    let imgs = this._doc.querySelectorAll(this._BLOCK_IMAGES_SELECTOR);
    for (let i = imgs.length; --i >= 0;) {
      let img = imgs[i];

      if (img.width > 0) {
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

  _showError: function Reader_showError(error) {
    this._headerElement.style.display = "none";
    this._contentElement.style.display = "none";

    this._messageElement.innerHTML = error;
    this._messageElement.style.display = "block";

    this._doc.title = error;
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
    this._domainElement.innerHTML = this._stripHost(articleUri.host);

    this._creditsElement.innerHTML = article.byline;

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

    this._toolbarEnabled = true;
    this._setToolbarVisibility(true);

    this._requestFavicon();
  },

  _hideContent: function Reader_hideContent() {
    this._headerElement.style.display = "none";
    this._contentElement.style.display = "none";
  },

  _showProgressDelayed: function Reader_showProgressDelayed() {
    this._win.setTimeout(function() {
      // Article has already been loaded, no need to show
      // progress anymore.
      if (this._article)
        return;

      this._headerElement.style.display = "none";
      this._contentElement.style.display = "none";

      this._messageElement.innerHTML = gStrings.GetStringFromName("aboutReader.loading");
      this._messageElement.style.display = "block";
    }.bind(this), 300);
  },

  /**
   * Returns the original article URL for this about:reader view.
   */
  _getOriginalUrl: function() {
    let url = this._win.location.href;
    let searchParams = new URLSearchParams(url.split("?")[1]);
    if (!searchParams.has("url")) {
      Cu.reportError("Error finding original URL for about:reader URL: " + url);
      return url;
    }
    return decodeURIComponent(searchParams.get("url"));
  },

  _setupSegmentedButton: function Reader_setupSegmentedButton(id, options, initialValue, callback) {
    let doc = this._doc;
    let segmentedButton = doc.getElementById(id);

    for (let i = 0; i < options.length; i++) {
      let option = options[i];

      let item = doc.createElement("li");
      let link = doc.createElement("a");
      link.textContent = option.name;
      item.appendChild(link);

      if (option.itemClass !== undefined)
        item.classList.add(option.itemClass);

      if (option.linkClass !== undefined)
        link.classList.add(option.linkClass);

      if (option.description !== undefined) {
        let description = doc.createElement("div");
        description.textContent = option.description;
        item.appendChild(description);
      }

      link.style.MozUserSelect = 'none';
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

  _setupButton: function Reader_setupButton(id, callback, titleEntity) {
    let button = this._doc.getElementById(id);
    button.setAttribute("title", gStrings.GetStringFromName(titleEntity));

    button.addEventListener("click", function(aEvent) {
      if (!aEvent.isTrusted)
        return;

      aEvent.stopPropagation();
      callback();
    }, true);
  },

  _setupStyleDropdown: function Reader_setupStyleDropdown() {
    let doc = this._doc;
    let win = this._win;

    let dropdown = doc.getElementById("style-dropdown");
    let dropdownToggle = dropdown.querySelector(".dropdown-toggle");
    let dropdownPopup = dropdown.querySelector(".dropdown-popup");
    let dropdownArrow = dropdown.querySelector(".dropdown-arrow");

    let updatePopupPosition = () => {
      if (this._isToolbarVertical) {
        let toggleHeight = dropdownToggle.offsetHeight;
        let toggleTop = dropdownToggle.offsetTop;
        let popupTop = toggleTop - toggleHeight / 2;
        dropdownPopup.style.top = popupTop + "px";
      } else {
        let popupWidth = dropdownPopup.offsetWidth + 30;
        let arrowWidth = dropdownArrow.offsetWidth;
        let toggleWidth = dropdownToggle.offsetWidth;
        let toggleLeft = dropdownToggle.offsetLeft;

        let popupShift = (toggleWidth - popupWidth) / 2;
        let popupLeft = Math.max(0, Math.min(win.innerWidth - popupWidth, toggleLeft + popupShift));
        dropdownPopup.style.left = popupLeft + "px";

        let arrowShift = (toggleWidth - arrowWidth) / 2;
        let arrowLeft = toggleLeft - popupLeft + arrowShift;
        dropdownArrow.style.left = arrowLeft + "px";
      }
    };

    win.addEventListener("resize", event => {
      if (!event.isTrusted)
        return;

      // Wait for reflow before calculating the new position of the popup.
      win.setTimeout(updatePopupPosition, 0);
    }, true);

    dropdownToggle.setAttribute("title", gStrings.GetStringFromName("aboutReader.toolbar.typeControls"));
    dropdownToggle.addEventListener("click", event => {
      if (!event.isTrusted)
        return;

      event.stopPropagation();

      if (dropdown.classList.contains("open")) {
        dropdown.classList.remove("open");
      } else {
        dropdown.classList.add("open");
        updatePopupPosition();
      }
    }, true);
  },
};
