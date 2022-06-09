/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutReader"];

const { ReaderMode } = ChromeUtils.import(
  "resource://gre/modules/ReaderMode.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "AsyncPrefs",
  "resource://gre/modules/AsyncPrefs.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "NarrateControls",
  "resource://gre/modules/narrate/NarrateControls.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "PluralForm",
  "resource://gre/modules/PluralForm.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "NimbusFeatures",
  "resource://nimbus/ExperimentAPI.jsm"
);

var gStrings = Services.strings.createBundle(
  "chrome://global/locale/aboutReader.properties"
);

Services.telemetry.setEventRecordingEnabled("readermode", true);

const zoomOnCtrl =
  Services.prefs.getIntPref("mousewheel.with_control.action", 3) == 3;
const zoomOnMeta =
  Services.prefs.getIntPref("mousewheel.with_meta.action", 1) == 3;
const isAppLocaleRTL = Services.locale.isAppLocaleRTL;

var AboutReader = function(
  actor,
  articlePromise,
  docContentType = "document",
  docTitle = ""
) {
  let win = actor.contentWindow;
  let url = this._getOriginalUrl(win);
  if (
    !(
      url.startsWith("http://") ||
      url.startsWith("https://") ||
      url.startsWith("file://")
    )
  ) {
    let errorMsg =
      "Only http://, https:// and file:// URLs can be loaded in about:reader.";
    if (Services.prefs.getBoolPref("reader.errors.includeURLs")) {
      errorMsg += " Tried to load: " + url + ".";
    }
    Cu.reportError(errorMsg);
    win.location.href = "about:blank";
    return;
  }

  let doc = win.document;
  if (isAppLocaleRTL) {
    doc.dir = "rtl";
  }
  doc.documentElement.setAttribute("platform", AppConstants.platform);

  doc.title = docTitle;

  this._actor = actor;
  this._isLoggedInPocketUser = undefined;

  this._docRef = Cu.getWeakReference(doc);
  this._winRef = Cu.getWeakReference(win);
  this._innerWindowId = win.windowGlobalChild.innerWindowId;

  this._article = null;
  this._languagePromise = new Promise(resolve => {
    this._foundLanguage = resolve;
  });

  if (articlePromise) {
    this._articlePromise = articlePromise;
  }

  this._headerElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-header")
  );
  this._domainElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-domain")
  );
  this._titleElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-title")
  );
  this._readTimeElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-estimated-time")
  );
  this._creditsElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-credits")
  );
  this._contentElementRef = Cu.getWeakReference(
    doc.querySelector(".moz-reader-content")
  );
  this._toolbarContainerElementRef = Cu.getWeakReference(
    doc.querySelector(".toolbar-container")
  );
  this._toolbarElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-controls")
  );
  this._messageElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-message")
  );
  this._containerElementRef = Cu.getWeakReference(
    doc.querySelector(".container")
  );

  doc.addEventListener("mousedown", this);
  doc.addEventListener("click", this);
  doc.addEventListener("touchstart", this);

  win.addEventListener("pagehide", this);
  win.addEventListener("resize", this);
  win.addEventListener("wheel", this, { passive: false });

  this.colorSchemeMediaList = win.matchMedia("(prefers-color-scheme: dark)");
  this.colorSchemeMediaList.addEventListener("change", this);

  this._topScrollChange = this._topScrollChange.bind(this);
  this._intersectionObs = new win.IntersectionObserver(this._topScrollChange, {
    root: null,
    threshold: [0, 1],
  });
  this._intersectionObs.observe(doc.querySelector(".top-anchor"));

  this._ctaIntersectionObserver = new win.IntersectionObserver(
    this._pocketCTAObserved.bind(this),
    {
      threshold: 0.5,
    }
  );

  Services.obs.addObserver(this, "inner-window-destroyed");

  this._setupButton(
    "close-button",
    this._onReaderClose.bind(this),
    "aboutReader.toolbar.close"
  );

  // we're ready for any external setup, send a signal for that.
  this._actor.sendAsyncMessage("Reader:OnSetup");

  let colorSchemeValues = JSON.parse(
    Services.prefs.getCharPref("reader.color_scheme.values")
  );
  let colorSchemeOptions = colorSchemeValues.map(value => {
    return {
      name: gStrings.GetStringFromName("aboutReader.colorScheme." + value),
      groupName: "color-scheme",
      value,
      itemClass: value + "-button",
    };
  });
  let colorScheme = Services.prefs.getCharPref("reader.color_scheme");

  this._setupSegmentedButton(
    "color-scheme-buttons",
    colorSchemeOptions,
    colorScheme,
    this._setColorSchemePref.bind(this)
  );
  this._setColorSchemePref(colorScheme);

  let styleButton = this._doc.querySelector(".style-button");
  this._setButtonTip(styleButton, "aboutReader.toolbar.typeControls");

  // See bug 1637089.
  // let fontTypeSample = gStrings.GetStringFromName("aboutReader.fontTypeSample");

  let fontTypeOptions = [
    {
      name: gStrings.GetStringFromName("aboutReader.fontType.sans-serif"),
      groupName: "font-type",
      value: "sans-serif",
      itemClass: "sans-serif-button",
    },
    {
      name: gStrings.GetStringFromName("aboutReader.fontType.serif"),
      groupName: "font-type",
      value: "serif",
      itemClass: "serif-button",
    },
  ];

  let fontType = Services.prefs.getCharPref("reader.font_type");
  this._setupSegmentedButton(
    "font-type-buttons",
    fontTypeOptions,
    fontType,
    this._setFontType.bind(this)
  );
  this._setFontType(fontType);

  this._setupFontSizeButtons();

  this._setupContentWidthButtons();

  this._setupLineHeightButtons();

  if (
    win.speechSynthesis &&
    Services.prefs.getBoolPref("narrate.enabled") &&
    !Services.prefs.getBoolPref("privacy.resistFingerprinting", false)
  ) {
    new lazy.NarrateControls(win, this._languagePromise);
  }

  this._loadArticle(docContentType);

  let dropdown = this._toolbarElement;

  let elemL10nMap = {
    ".minus-button": "minus",
    ".plus-button": "plus",
    ".content-width-minus-button": "contentwidthminus",
    ".content-width-plus-button": "contentwidthplus",
    ".line-height-minus-button": "lineheightminus",
    ".line-height-plus-button": "lineheightplus",
    ".light-button": "colorschemelight",
    ".dark-button": "colorschemedark",
    ".sepia-button": "colorschemesepia",
    ".auto-button": "colorschemeauto",
  };

  for (let [selector, stringID] of Object.entries(elemL10nMap)) {
    dropdown
      .querySelector(selector)
      ?.setAttribute(
        "title",
        gStrings.GetStringFromName("aboutReader.toolbar." + stringID)
      );
  }
};

AboutReader.prototype = {
  _BLOCK_IMAGES_SELECTOR:
    ".content p > img:only-child, " +
    ".content p > a:only-child > img:only-child, " +
    ".content .wp-caption img, " +
    ".content figure img",

  _TABLES_SELECTOR: ".content table",

  FONT_SIZE_MIN: 1,

  FONT_SIZE_LEGACY_MAX: 9,

  FONT_SIZE_MAX: 15,

  FONT_SIZE_EXTENDED_VALUES: [32, 40, 56, 72, 96, 128],

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

  get _readTimeElement() {
    return this._readTimeElementRef.get();
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

  get _toolbarContainerElement() {
    return this._toolbarContainerElementRef.get();
  },

  get _messageElement() {
    return this._messageElementRef.get();
  },

  get _containerElement() {
    return this._containerElementRef.get();
  },

  get _isToolbarVertical() {
    if (this._toolbarVertical !== undefined) {
      return this._toolbarVertical;
    }
    return (this._toolbarVertical = Services.prefs.getBoolPref(
      "reader.toolbar.vertical"
    ));
  },

  receiveMessage(message) {
    switch (message.name) {
      case "Reader:AddButton": {
        if (
          message.data.id &&
          message.data.image &&
          !this._doc.getElementsByClassName(message.data.id)[0]
        ) {
          let btn = this._doc.createElement("button");
          btn.dataset.buttonid = message.data.id;
          btn.dataset.telemetryId = `reader-${message.data.telemetryId}`;
          btn.className = "button " + message.data.id;
          let tip = this._doc.createElement("span");
          tip.className = "hover-label";
          tip.textContent = message.data.label;
          btn.append(tip);
          btn.setAttribute("aria-label", message.data.label);
          btn.style.backgroundImage = "url('" + message.data.image + "')";
          if (message.data.width && message.data.height) {
            btn.style.backgroundSize = `${message.data.width}px ${message.data.height}px`;
          }
          let tb = this._toolbarElement;
          tb.appendChild(btn);
          this._setupButton(message.data.id, button => {
            this._actor.sendAsyncMessage(
              "Reader:Clicked-" + button.dataset.buttonid,
              { article: this._article }
            );
          });
        }
        break;
      }
      case "Reader:RemoveButton": {
        if (message.data.id) {
          let btn = this._doc.getElementsByClassName(message.data.id)[0];
          if (btn) {
            btn.remove();
          }
        }
        break;
      }
      case "Reader:ZoomIn": {
        this._changeFontSize(+1);
        break;
      }
      case "Reader:ZoomOut": {
        this._changeFontSize(-1);
        break;
      }
      case "Reader:ResetZoom": {
        this._resetFontSize();
        break;
      }
    }
  },

  handleEvent(aEvent) {
    if (!aEvent.isTrusted) {
      return;
    }

    let target = aEvent.target;
    switch (aEvent.type) {
      case "touchstart":
      /* fall through */
      case "mousedown":
        if (
          !target.closest(".dropdown-popup") &&
          // Skip handling the toggle button here becase
          // the dropdown will get toggled with the 'click' event.
          !target.classList.contains("dropdown-toggle")
        ) {
          this._closeDropdowns();
        }
        break;
      case "click":
        const buttonLabel = target.attributes.getNamedItem(`data-telemetry-id`)
          ?.value;

        if (buttonLabel) {
          Services.telemetry.recordEvent(
            "readermode",
            "button",
            "click",
            null,
            {
              label: buttonLabel,
            }
          );
        }

        if (target.classList.contains("dropdown-toggle")) {
          this._toggleDropdownClicked(aEvent);
        }
        break;
      case "scroll":
        let lastHeight = this._lastHeight;
        let { windowUtils } = this._win;
        this._lastHeight = windowUtils.getBoundsWithoutFlushing(
          this._doc.body
        ).height;
        // Only close dropdowns if the scroll events are not a result of line
        // height / font-size changes that caused a page height change.
        if (lastHeight == this._lastHeight) {
          this._closeDropdowns(true);
        }

        break;
      case "resize":
        this._updateImageMargins();
        this._scheduleToolbarOverlapHandler();
        break;

      case "wheel":
        let doZoom =
          (aEvent.ctrlKey && zoomOnCtrl) || (aEvent.metaKey && zoomOnMeta);
        if (!doZoom) {
          return;
        }
        aEvent.preventDefault();

        // Throttle events to once per 150ms. This avoids excessively fast zooming.
        if (aEvent.timeStamp <= this._zoomBackoffTime) {
          return;
        }
        this._zoomBackoffTime = aEvent.timeStamp + 150;

        // Determine the direction of the delta (we don't care about its size);
        // This code is adapted from normalizeWheelEventDelta in
        // toolkt/components/pdfjs/content/web/viewer.js
        let delta = Math.abs(aEvent.deltaX) + Math.abs(aEvent.deltaY);
        let angle = Math.atan2(aEvent.deltaY, aEvent.deltaX);
        if (-0.25 * Math.PI < angle && angle < 0.75 * Math.PI) {
          delta = -delta;
        }

        if (delta > 0) {
          this._changeFontSize(+1);
        } else if (delta < 0) {
          this._changeFontSize(-1);
        }
        break;

      case "pagehide":
        this._closeDropdowns();

        this._actor.readerModeHidden();
        this.clearActor();

        // Disconnect and delete IntersectionObservers to prevent memory leaks:

        this._intersectionObs.unobserve(this._doc.querySelector(".top-anchor"));
        this._ctaIntersectionObserver.disconnect();

        delete this._intersectionObs;
        delete this._ctaIntersectionObserver;

        break;

      case "change":
        // We should only be changing the color scheme in relation to a preference change
        // if the user has the color scheme preference set to "Auto"
        if (Services.prefs.getCharPref("reader.color_scheme") === "auto") {
          let colorScheme = this.colorSchemeMediaList.matches
            ? "dark"
            : "light";

          this._setColorScheme(colorScheme);
        }

        break;
    }
  },

  clearActor() {
    this._actor = null;
  },

  _onReaderClose() {
    if (this._actor) {
      this._actor.closeReaderMode();
    }
  },

  async _resetFontSize() {
    await lazy.AsyncPrefs.reset("reader.font_size");
    let currentSize = Services.prefs.getIntPref("reader.font_size");
    this._setFontSize(currentSize);
  },

  _setFontSize(newFontSize) {
    this._fontSize = Math.min(
      this.FONT_SIZE_MAX,
      Math.max(this.FONT_SIZE_MIN, newFontSize)
    );
    let size;
    if (this._fontSize > this.FONT_SIZE_LEGACY_MAX) {
      // -1 because we're indexing into a 0-indexed array, so the first value
      // over the legacy max should be 0, the next 1, etc.
      let index = this._fontSize - this.FONT_SIZE_LEGACY_MAX - 1;
      size = this.FONT_SIZE_EXTENDED_VALUES[index];
    } else {
      size = 10 + 2 * this._fontSize;
    }

    let readerBody = this._doc.body;
    readerBody.style.setProperty("--font-size", size + "px");
    return lazy.AsyncPrefs.set("reader.font_size", this._fontSize);
  },

  _setupFontSizeButtons() {
    let plusButton = this._doc.querySelector(".plus-button");
    let minusButton = this._doc.querySelector(".minus-button");

    let currentSize = Services.prefs.getIntPref("reader.font_size");
    this._setFontSize(currentSize);
    this._updateFontSizeButtonControls();

    plusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();
        this._changeFontSize(+1);
      },
      true
    );

    minusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();
        this._changeFontSize(-1);
      },
      true
    );
  },

  _updateFontSizeButtonControls() {
    let plusButton = this._doc.querySelector(".plus-button");
    let minusButton = this._doc.querySelector(".minus-button");

    let currentSize = this._fontSize;
    let fontValue = this._doc.querySelector(".font-size-value");
    fontValue.textContent = currentSize;

    if (currentSize === this.FONT_SIZE_MIN) {
      minusButton.setAttribute("disabled", true);
    } else {
      minusButton.removeAttribute("disabled");
    }
    if (currentSize === this.FONT_SIZE_MAX) {
      plusButton.setAttribute("disabled", true);
    } else {
      plusButton.removeAttribute("disabled");
    }
  },

  _changeFontSize(changeAmount) {
    let currentSize =
      Services.prefs.getIntPref("reader.font_size") + changeAmount;
    this._setFontSize(currentSize);
    this._updateFontSizeButtonControls();
    this._scheduleToolbarOverlapHandler();
  },

  _setContentWidth(newContentWidth) {
    this._contentWidth = newContentWidth;
    this._displayContentWidth(newContentWidth);
    let width = 20 + 5 * (this._contentWidth - 1) + "em";
    this._doc.body.style.setProperty("--content-width", width);
    this._scheduleToolbarOverlapHandler();
    return lazy.AsyncPrefs.set("reader.content_width", this._contentWidth);
  },

  _displayContentWidth(currentContentWidth) {
    let contentWidthValue = this._doc.querySelector(".content-width-value");
    contentWidthValue.textContent = currentContentWidth;
  },

  _setupContentWidthButtons() {
    const CONTENT_WIDTH_MIN = 1;
    const CONTENT_WIDTH_MAX = 9;

    let currentContentWidth = Services.prefs.getIntPref("reader.content_width");
    currentContentWidth = Math.max(
      CONTENT_WIDTH_MIN,
      Math.min(CONTENT_WIDTH_MAX, currentContentWidth)
    );

    this._displayContentWidth(currentContentWidth);

    let plusButton = this._doc.querySelector(".content-width-plus-button");
    let minusButton = this._doc.querySelector(".content-width-minus-button");

    function updateControls() {
      if (currentContentWidth === CONTENT_WIDTH_MIN) {
        minusButton.setAttribute("disabled", true);
      } else {
        minusButton.removeAttribute("disabled");
      }
      if (currentContentWidth === CONTENT_WIDTH_MAX) {
        plusButton.setAttribute("disabled", true);
      } else {
        plusButton.removeAttribute("disabled");
      }
    }

    updateControls();
    this._setContentWidth(currentContentWidth);

    plusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();

        if (currentContentWidth >= CONTENT_WIDTH_MAX) {
          return;
        }

        currentContentWidth++;
        updateControls();
        this._setContentWidth(currentContentWidth);
      },
      true
    );

    minusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();

        if (currentContentWidth <= CONTENT_WIDTH_MIN) {
          return;
        }

        currentContentWidth--;
        updateControls();
        this._setContentWidth(currentContentWidth);
      },
      true
    );
  },

  _setLineHeight(newLineHeight) {
    this._displayLineHeight(newLineHeight);
    let height = 1 + 0.2 * (newLineHeight - 1) + "em";
    this._containerElement.style.setProperty("--line-height", height);
    return lazy.AsyncPrefs.set("reader.line_height", newLineHeight);
  },

  _displayLineHeight(currentLineHeight) {
    let lineHeightValue = this._doc.querySelector(".line-height-value");
    lineHeightValue.textContent = currentLineHeight;
  },

  _setupLineHeightButtons() {
    const LINE_HEIGHT_MIN = 1;
    const LINE_HEIGHT_MAX = 9;

    let currentLineHeight = Services.prefs.getIntPref("reader.line_height");
    currentLineHeight = Math.max(
      LINE_HEIGHT_MIN,
      Math.min(LINE_HEIGHT_MAX, currentLineHeight)
    );

    this._displayLineHeight(currentLineHeight);

    let plusButton = this._doc.querySelector(".line-height-plus-button");
    let minusButton = this._doc.querySelector(".line-height-minus-button");

    function updateControls() {
      if (currentLineHeight === LINE_HEIGHT_MIN) {
        minusButton.setAttribute("disabled", true);
      } else {
        minusButton.removeAttribute("disabled");
      }
      if (currentLineHeight === LINE_HEIGHT_MAX) {
        plusButton.setAttribute("disabled", true);
      } else {
        plusButton.removeAttribute("disabled");
      }
    }

    updateControls();
    this._setLineHeight(currentLineHeight);

    plusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();

        if (currentLineHeight >= LINE_HEIGHT_MAX) {
          return;
        }

        currentLineHeight++;
        updateControls();
        this._setLineHeight(currentLineHeight);
      },
      true
    );

    minusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();

        if (currentLineHeight <= LINE_HEIGHT_MIN) {
          return;
        }

        currentLineHeight--;
        updateControls();
        this._setLineHeight(currentLineHeight);
      },
      true
    );
  },

  _setColorScheme(newColorScheme) {
    // There's nothing to change if the new color scheme is the same as our current scheme.
    if (this._colorScheme === newColorScheme) {
      return;
    }

    let bodyClasses = this._doc.body.classList;

    if (this._colorScheme) {
      bodyClasses.remove(this._colorScheme);
    }

    if (newColorScheme === "auto") {
      this._colorScheme = this.colorSchemeMediaList.matches ? "dark" : "light";
    } else {
      this._colorScheme = newColorScheme;
    }

    bodyClasses.add(this._colorScheme);
  },

  // Pref values include "dark", "light", "sepia", and "auto"
  _setColorSchemePref(colorSchemePref) {
    this._setColorScheme(colorSchemePref);

    lazy.AsyncPrefs.set("reader.color_scheme", colorSchemePref);
  },

  _setFontType(newFontType) {
    if (this._fontType === newFontType) {
      return;
    }

    let bodyClasses = this._doc.body.classList;

    if (this._fontType) {
      bodyClasses.remove(this._fontType);
    }

    this._fontType = newFontType;
    bodyClasses.add(this._fontType);

    lazy.AsyncPrefs.set("reader.font_type", this._fontType);
  },

  async _loadArticle(docContentType = "document") {
    let url = this._getOriginalUrl();
    this._showProgressDelayed();

    let article;
    if (this._articlePromise) {
      article = await this._articlePromise;
    }

    if (!article) {
      try {
        article = await ReaderMode.downloadAndParseDocument(
          url,
          docContentType
        );
      } catch (e) {
        if (e?.newURL && this._actor) {
          await this._actor.sendQuery("RedirectTo", {
            newURL: e.newURL,
            article: e.article,
          });

          let readerURL = "about:reader?url=" + encodeURIComponent(e.newURL);
          this._win.location.replace(readerURL);
          return;
        }
      }
    }

    if (!this._actor) {
      return;
    }

    // Replace the loading message with an error message if there's a failure.
    // Users are supposed to navigate away by themselves (because we cannot
    // remove ourselves from session history.)
    if (!article) {
      this._showError();
      return;
    }

    this._showContent(article);
  },

  async _requestPocketLoginStatus() {
    let isLoggedIn = await this._actor.sendQuery(
      "Reader:PocketLoginStatusRequest"
    );

    return isLoggedIn;
  },

  async _requestPocketArticleInfo(url) {
    let articleInfo = await this._actor.sendQuery(
      "Reader:PocketGetArticleInfo",
      {
        url,
      }
    );

    return articleInfo?.item_preview?.item_id;
  },

  async _requestPocketArticleRecs(itemID) {
    let recs = await this._actor.sendQuery("Reader:PocketGetArticleRecs", {
      itemID,
    });

    return recs;
  },

  async _savePocketArticle(url) {
    let result = await this._actor.sendQuery("Reader:PocketSaveArticle", {
      url,
    });

    return result;
  },

  async _requestFavicon() {
    let iconDetails = await this._actor.sendQuery("Reader:FaviconRequest", {
      url: this._article.url,
      preferredWidth: 16 * this._win.devicePixelRatio,
    });

    if (iconDetails) {
      this._loadFavicon(iconDetails.url, iconDetails.faviconUrl);
    }
  },

  _loadFavicon(url, faviconUrl) {
    if (this._article.url !== url) {
      return;
    }

    let doc = this._doc;

    let link = doc.createElement("link");
    link.rel = "shortcut icon";
    link.href = faviconUrl;

    doc.getElementsByTagName("head")[0].appendChild(link);
  },

  _updateImageMargins() {
    let windowWidth = this._win.innerWidth;
    let bodyWidth = this._doc.body.clientWidth;

    let setImageMargins = function(img) {
      img.classList.add("moz-reader-block-img");

      // If the image is at least as wide as the window, make it fill edge-to-edge on mobile.
      if (img.naturalWidth >= windowWidth) {
        img.setAttribute("moz-reader-full-width", true);
      } else {
        img.removeAttribute("moz-reader-full-width");
      }

      // If the image is at least half as wide as the body, center it on desktop.
      if (img.naturalWidth >= bodyWidth / 2) {
        img.setAttribute("moz-reader-center", true);
      } else {
        img.removeAttribute("moz-reader-center");
      }
    };

    let imgs = this._doc.querySelectorAll(this._BLOCK_IMAGES_SELECTOR);
    for (let i = imgs.length; --i >= 0; ) {
      let img = imgs[i];

      if (img.naturalWidth > 0) {
        setImageMargins(img);
      } else {
        img.onload = function() {
          setImageMargins(img);
        };
      }
    }
  },

  _updateWideTables() {
    let windowWidth = this._win.innerWidth;

    // Avoid horizontal overflow in the document by making tables that are wider than half browser window's size
    // by making it scrollable.
    let tables = this._doc.querySelectorAll(this._TABLES_SELECTOR);
    for (let i = tables.length; --i >= 0; ) {
      let table = tables[i];
      let rect = table.getBoundingClientRect();
      let tableWidth = rect.width;

      if (windowWidth / 2 <= tableWidth) {
        table.classList.add("moz-reader-wide-table");
      }
    }
  },

  _maybeSetTextDirection: function Read_maybeSetTextDirection(article) {
    // Set the article's "dir" on the contents.
    // If no direction is specified, the contents should automatically be LTR
    // regardless of the UI direction to avoid inheriting the parent's direction
    // if the UI is RTL.
    this._containerElement.dir = article.dir || "ltr";

    // The native locale could be set differently than the article's text direction.
    this._readTimeElement.dir = isAppLocaleRTL ? "rtl" : "ltr";

    // This is used to mirror the line height buttons in the toolbar, when relevant.
    this._toolbarElement.setAttribute("articledir", article.dir || "ltr");
  },

  _formatReadTime(slowEstimate, fastEstimate) {
    let displayStringKey = "aboutReader.estimatedReadTimeRange1";

    // only show one reading estimate when they are the same value
    if (slowEstimate == fastEstimate) {
      displayStringKey = "aboutReader.estimatedReadTimeValue1";
    }

    return lazy.PluralForm.get(
      slowEstimate,
      gStrings.GetStringFromName(displayStringKey)
    )
      .replace("#1", fastEstimate)
      .replace("#2", slowEstimate);
  },

  _showError() {
    this._headerElement.classList.remove("reader-show-element");
    this._contentElement.classList.remove("reader-show-element");

    let errorMessage = gStrings.GetStringFromName("aboutReader.loadError");
    this._messageElement.textContent = errorMessage;
    this._messageElement.style.display = "block";

    this._doc.title = errorMessage;

    this._doc.documentElement.dataset.isError = true;

    this._error = true;

    this._doc.dispatchEvent(
      new this._win.CustomEvent("AboutReaderContentError", {
        bubbles: true,
        cancelable: false,
      })
    );
  },

  // This function is the JS version of Java's StringUtils.stripCommonSubdomains.
  _stripHost(host) {
    if (!host) {
      return host;
    }

    let start = 0;

    if (host.startsWith("www.")) {
      start = 4;
    } else if (host.startsWith("m.")) {
      start = 2;
    } else if (host.startsWith("mobile.")) {
      start = 7;
    }

    return host.substring(start);
  },

  _showContent(article) {
    this._messageElement.classList.remove("reader-show-element");

    this._article = article;

    this._domainElement.href = article.url;
    let articleUri = Services.io.newURI(article.url);

    try {
      this._domainElement.textContent = this._stripHost(articleUri.host);
    } catch (ex) {
      let url = this._actor.document.URL;
      url = url.substring(url.indexOf("%2F") + 6);
      url = url.substring(0, url.indexOf("%2F"));

      this._domainElement.textContent = url;
    }

    this._creditsElement.textContent = article.byline;

    this._titleElement.textContent = article.title;
    this._readTimeElement.textContent = this._formatReadTime(
      article.readingTimeMinsSlow,
      article.readingTimeMinsFast
    );

    // If a document title was not provided in the constructor, we'll fall back
    // to using the article title.
    if (!this._doc.title) {
      this._doc.title = article.title;
    }

    this._containerElement.setAttribute("lang", article.lang);

    this._headerElement.classList.add("reader-show-element");

    let parserUtils = Cc["@mozilla.org/parserutils;1"].getService(
      Ci.nsIParserUtils
    );
    let contentFragment = parserUtils.parseFragment(
      article.content,
      Ci.nsIParserUtils.SanitizerDropForms |
        Ci.nsIParserUtils.SanitizerAllowStyle,
      false,
      articleUri,
      this._contentElement
    );
    this._contentElement.innerHTML = "";
    this._contentElement.appendChild(contentFragment);
    this._maybeSetTextDirection(article);
    this._foundLanguage(article.language);

    this._contentElement.classList.add("reader-show-element");
    this._updateImageMargins();
    this._updateWideTables();

    this._requestFavicon();
    this._doc.body.classList.add("loaded");

    this._goToReference(articleUri.ref);

    Services.obs.notifyObservers(this._win, "AboutReader:Ready");

    this._doc.dispatchEvent(
      new this._win.CustomEvent("AboutReaderContentReady", {
        bubbles: true,
        cancelable: false,
      })
    );

    // Show Pocket CTA block after article has loaded to prevent it flashing in prematurely
    this._setupPocketCTA();
  },

  _hideContent() {
    this._headerElement.classList.remove("reader-show-element");
    this._contentElement.classList.remove("reader-show-element");
  },

  _showProgressDelayed() {
    this._win.setTimeout(() => {
      // No need to show progress if the article has been loaded,
      // if the window has been unloaded, or if there was an error
      // trying to load the article.
      if (this._article || !this._actor || this._error) {
        return;
      }

      this._headerElement.classList.remove("reader-show-element");
      this._contentElement.classList.remove("reader-show-element");

      this._messageElement.textContent = gStrings.GetStringFromName(
        "aboutReader.loading2"
      );
      this._messageElement.classList.add("reader-show-element");
    }, 300);
  },

  /**
   * Returns the original article URL for this about:reader view.
   */
  _getOriginalUrl(win) {
    let url = win ? win.location.href : this._win.location.href;
    return ReaderMode.getOriginalUrl(url) || url;
  },

  _setupSegmentedButton(id, options, initialValue, callback) {
    let doc = this._doc;
    let segmentedButton = doc.getElementsByClassName(id)[0];

    for (let option of options) {
      let radioButton = doc.createElement("input");
      radioButton.id = "radio-item" + option.itemClass;
      radioButton.type = "radio";
      radioButton.classList.add("radio-button");
      radioButton.name = option.groupName;
      segmentedButton.appendChild(radioButton);

      let item = doc.createElement("label");
      item.textContent = option.name;
      item.htmlFor = radioButton.id;
      item.classList.add(option.itemClass);

      segmentedButton.appendChild(item);

      radioButton.addEventListener(
        "input",
        function(aEvent) {
          if (!aEvent.isTrusted) {
            return;
          }

          let labels = segmentedButton.children;
          for (let label of labels) {
            label.removeAttribute("checked");
          }

          aEvent.target.nextElementSibling.setAttribute("checked", "true");
          callback(option.value);
        },
        true
      );

      if (option.value === initialValue) {
        radioButton.checked = true;
        item.setAttribute("checked", "true");
      }
    }
  },

  _setupButton(id, callback, titleEntity) {
    let button = this._doc.querySelector("." + id);
    if (titleEntity) {
      this._setButtonTip(button, titleEntity);
    }

    button.removeAttribute("hidden");
    button.addEventListener(
      "click",
      function(aEvent) {
        if (!aEvent.isTrusted) {
          return;
        }

        let btn = aEvent.target;
        callback(btn);
      },
      true
    );
  },

  /**
   * Sets a tooltip-style label on a button.
   * @param   Localizable string providing UI element usage tip.
   */
  _setButtonTip(button, titleEntity) {
    let tip = this._doc.createElement("span");
    let localizedString = gStrings.GetStringFromName(titleEntity);
    tip.textContent = localizedString;
    tip.className = "hover-label";
    button.setAttribute("aria-label", localizedString);
    button.append(tip);
  },

  _toggleDropdownClicked(event) {
    let dropdown = event.target.closest(".dropdown");

    if (!dropdown) {
      return;
    }

    event.stopPropagation();

    if (dropdown.classList.contains("open")) {
      this._closeDropdowns();
    } else {
      this._openDropdown(dropdown);
    }
  },

  /*
   * If the ReaderView banner font-dropdown is closed, open it.
   */
  _openDropdown(dropdown, window) {
    if (dropdown.classList.contains("open")) {
      return;
    }

    this._closeDropdowns();

    // Get the height of the doc and start handling scrolling:
    let { windowUtils } = this._win;
    this._lastHeight = windowUtils.getBoundsWithoutFlushing(
      this._doc.body
    ).height;
    this._doc.addEventListener("scroll", this);

    dropdown.classList.add("open");
    this._toolbarElement.classList.add("dropdown-open");

    this._toolbarContainerElement.classList.add("dropdown-open");
    this._toggleToolbarFixedPosition(true);
  },

  /*
   * If the ReaderView has open dropdowns, close them. If we are closing the
   * dropdowns because the page is scrolling, allow popups to stay open with
   * the keep-open class.
   */
  _closeDropdowns(scrolling) {
    let selector = ".dropdown.open";
    if (scrolling) {
      selector += ":not(.keep-open)";
    }

    let openDropdowns = this._doc.querySelectorAll(selector);
    let haveOpenDropdowns = openDropdowns.length;
    for (let dropdown of openDropdowns) {
      dropdown.classList.remove("open");
    }
    this._toolbarElement.classList.remove("dropdown-open");

    if (haveOpenDropdowns) {
      this._toolbarContainerElement.classList.remove("dropdown-open");
      this._toggleToolbarFixedPosition(false);
    }

    // Stop handling scrolling:
    this._doc.removeEventListener("scroll", this);
  },

  _toggleToolbarFixedPosition(shouldBeFixed) {
    let el = this._toolbarContainerElement;
    let fontSize = this._doc.body.style.getPropertyValue("--font-size");
    let contentWidth = this._doc.body.style.getPropertyValue("--content-width");
    if (shouldBeFixed) {
      el.style.setProperty("--font-size", fontSize);
      el.style.setProperty("--content-width", contentWidth);
      el.classList.add("transition-location");
    } else {
      let expectTransition =
        el.style.getPropertyValue("--font-size") != fontSize ||
        el.style.getPropertyValue("--content-width") != contentWidth;
      if (expectTransition) {
        el.addEventListener(
          "transitionend",
          () => el.classList.remove("transition-location"),
          { once: true }
        );
      } else {
        el.classList.remove("transition-location");
      }
      el.style.removeProperty("--font-size");
      el.style.removeProperty("--content-width");
      el.classList.remove("overlaps");
    }
  },

  _scheduleToolbarOverlapHandler() {
    if (this._enqueuedToolbarOverlapHandler) {
      return;
    }
    this._enqueuedToolbarOverlapHandler = this._win.requestAnimationFrame(
      () => {
        this._win.setTimeout(() => this._toolbarOverlapHandler(), 0);
      }
    );
  },

  _toolbarOverlapHandler() {
    delete this._enqueuedToolbarOverlapHandler;
    // Ensure the dropdown is still open to avoid racing with that changing.
    if (this._toolbarContainerElement.classList.contains("dropdown-open")) {
      let { windowUtils } = this._win;
      let toolbarBounds = windowUtils.getBoundsWithoutFlushing(
        this._toolbarElement.parentNode
      );
      let textBounds = windowUtils.getBoundsWithoutFlushing(
        this._containerElement
      );
      let overlaps = false;
      if (isAppLocaleRTL) {
        overlaps = textBounds.right > toolbarBounds.left;
      } else {
        overlaps = textBounds.left < toolbarBounds.right;
      }
      this._toolbarContainerElement.classList.toggle("overlaps", overlaps);
    }
  },

  _topScrollChange(entries) {
    if (!entries.length) {
      return;
    }
    // If we don't intersect the item at the top of the document, we're
    // scrolled down:
    let scrolled = !entries[entries.length - 1].isIntersecting;
    let tbc = this._toolbarContainerElement;
    tbc.classList.toggle("scrolled", scrolled);
  },

  /*
   * Scroll reader view to a reference
   */
  _goToReference(ref) {
    if (ref) {
      if (this._doc.readyState == "complete") {
        this._win.location.hash = ref;
      } else {
        this._win.addEventListener(
          "load",
          () => {
            this._win.location.hash = ref;
          },
          { once: true }
        );
      }
    }
  },

  _enableDismissCTA() {
    let elDismissCta = this._doc.querySelector(`.pocket-dismiss-cta`);

    elDismissCta?.addEventListener(`click`, e => {
      this._doc.querySelector("#pocket-cta-container").hidden = true;

      Services.telemetry.recordEvent(
        "readermode",
        "pocket_cta",
        "close_cta",
        null,
        {}
      );
    });
  },

  _enableRecShowHide() {
    let elPocketRecs = this._doc.querySelector(`.pocket-recs`);
    let elCollapseRecs = this._doc.querySelector(`.pocket-collapse-recs`);
    let elSignUp = this._doc.querySelector(`div.pocket-sign-up-wrapper`);

    let toggleRecsVisibility = () => {
      let isClosed = elPocketRecs.classList.contains(`closed`);

      isClosed = !isClosed; // Toggle

      if (isClosed) {
        elPocketRecs.classList.add(`closed`);
        elCollapseRecs.classList.add(`closed`);
        elSignUp.setAttribute(`hidden`, true);

        Services.telemetry.recordEvent(
          "readermode",
          "pocket_cta",
          "minimize_recs_click",
          null,
          {}
        );
      } else {
        elPocketRecs.classList.remove(`closed`);
        elCollapseRecs.classList.remove(`closed`);
        elSignUp.removeAttribute(`hidden`);
      }
    };

    elCollapseRecs?.addEventListener(`click`, e => {
      toggleRecsVisibility();
    });
  },

  _buildPocketRec(title, url, publisher, thumb, time) {
    let fragment = this._doc.createDocumentFragment();

    let elContainer = this._doc.createElement(`div`);
    let elTitle = this._doc.createElement(`header`);
    let elMetadata = this._doc.createElement(`p`);
    let elThumb = this._doc.createElement(`img`);
    let elSideWrap = this._doc.createElement(`div`);
    let elTop = this._doc.createElement(`a`);
    let elBottom = this._doc.createElement(`div`);
    let elAdd = this._doc.createElement(`button`);

    elAdd.classList.add(`pocket-btn-add`);
    elBottom.classList.add(`pocket-rec-bottom`);
    elTop.classList.add(`pocket-rec-top`);
    elSideWrap.classList.add(`pocket-rec-side`);
    elContainer.classList.add(`pocket-rec`);
    elTitle.classList.add(`pocket-rec-title`);
    elMetadata.classList.add(`pocket-rec-meta`);

    elTop.setAttribute(`href`, url);

    elTop.addEventListener(`click`, e => {
      Services.telemetry.recordEvent(
        "readermode",
        "pocket_cta",
        "rec_click",
        null,
        {}
      );
    });

    elThumb.classList.add(`pocket-rec-thumb`);
    elThumb.setAttribute(`loading`, `lazy`);
    elThumb.addEventListener(`load`, () => {
      elThumb.classList.add(`pocket-rec-thumb-loaded`);
    });
    elThumb.setAttribute(
      `src`,
      `https://img-getpocket.cdn.mozilla.net/132x132/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${thumb}`
    );

    elAdd.textContent = `Save`;
    elTitle.textContent = title;

    if (publisher && time) {
      elMetadata.textContent = `${publisher} · ${time} min`;
    } else if (publisher) {
      elMetadata.textContent = `${publisher}`;
    } else if (time) {
      elMetadata.textContent = `${time} min`;
    }

    elSideWrap.appendChild(elTitle);
    elSideWrap.appendChild(elMetadata);
    elTop.appendChild(elSideWrap);
    elTop.appendChild(elThumb);
    elBottom.appendChild(elAdd);
    elContainer.appendChild(elTop);
    elContainer.appendChild(elBottom);
    fragment.appendChild(elContainer);

    elAdd.addEventListener(`click`, e => {
      this._savePocketArticle(url);
      elAdd.textContent = `Saved`;
      elAdd.classList.add(`saved`);

      Services.telemetry.recordEvent(
        "readermode",
        "pocket_cta",
        "rec_saved",
        null,
        {}
      );
    });

    return fragment;
  },

  async _getAndBuildPocketRecs() {
    let elTarget = this._doc.querySelector(`.pocket-recs`);
    let url = this._getOriginalUrl();
    let itemID = await this._requestPocketArticleInfo(url);
    let articleRecs = await this._requestPocketArticleRecs(itemID);

    articleRecs.recommendations.forEach(rec => {
      // Parse a domain from the article URL in case the Publisher name isn't available
      let parsedDomain = new URL(rec.item?.normal_url)?.hostname;

      // Calculate read time from word count in case it's not available
      let calculatedReadTime = Math.ceil(rec.item?.word_count / 220);

      let elRec = this._buildPocketRec(
        rec.item?.title,
        rec.item?.normal_url,
        rec.item?.domain_metadata?.name || parsedDomain,
        rec.item?.top_image_url,
        rec.item?.time_to_read || calculatedReadTime
      );

      elTarget.appendChild(elRec);
    });
  },

  _pocketCTAObserved(entries) {
    if (entries && entries[0]?.isIntersecting) {
      this._ctaIntersectionObserver.disconnect();

      Services.telemetry.recordEvent(
        "readermode",
        "pocket_cta",
        "cta_seen",
        null,
        {
          logged_in: `${this._isLoggedInPocketUser}`,
        }
      );
    }
  },

  async _setupPocketCTA() {
    let ctaVersion = lazy.NimbusFeatures.readerMode.getAllVariables()
      ?.pocketCTAVersion;
    this._isLoggedInPocketUser = await this._requestPocketLoginStatus();
    let elPocketCTAWrapper = this._doc.querySelector("#pocket-cta-container");

    // Show the Pocket CTA container if the pref is set and valid
    if (ctaVersion === `cta-and-recs` || ctaVersion === `cta-only`) {
      if (ctaVersion === `cta-and-recs` && this._isLoggedInPocketUser) {
        this._getAndBuildPocketRecs();
        this._enableRecShowHide();
      } else if (ctaVersion === `cta-and-recs` && !this._isLoggedInPocketUser) {
        // Fall back to cta only for logged out users:
        ctaVersion = `cta-only`;
      }

      if (ctaVersion == `cta-only`) {
        this._enableDismissCTA();
      }

      elPocketCTAWrapper.hidden = false;
      elPocketCTAWrapper.classList.add(`pocket-cta-container-${ctaVersion}`);
      elPocketCTAWrapper.classList.add(
        `pocket-cta-container-${
          this._isLoggedInPocketUser ? `logged-in` : `logged-out`
        }`
      );

      // Set up tracking for sign up buttons
      this._doc
        .querySelectorAll(`.pocket-sign-up, .pocket-discover-more`)
        .forEach(el => {
          el.addEventListener(`click`, e => {
            Services.telemetry.recordEvent(
              "readermode",
              "pocket_cta",
              "sign_up_click",
              null,
              {}
            );
          });
        });

      // Set up tracking for user seeing CTA
      this._ctaIntersectionObserver.observe(
        this._doc.querySelector(`#pocket-cta-container`)
      );
    }
  },
};
