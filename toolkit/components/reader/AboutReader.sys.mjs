/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ReaderMode } from "resource://gre/modules/ReaderMode.sys.mjs";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
let gScrollPositions = new Map();
let lastSelectedTheme = "auto";
let improvedTextMenuEnabled = Services.prefs.getBoolPref(
  "reader.improved_text_menu.enabled",
  false
);

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncPrefs: "resource://gre/modules/AsyncPrefs.sys.mjs",
  NarrateControls: "resource://gre/modules/narrate/NarrateControls.sys.mjs",
});

ChromeUtils.defineLazyGetter(
  lazy,
  "numberFormat",
  () => new Services.intl.NumberFormat(undefined)
);
ChromeUtils.defineLazyGetter(
  lazy,
  "pluralRules",
  () => new Services.intl.PluralRules(undefined)
);
ChromeUtils.defineLazyGetter(
  lazy,
  "l10n",
  () => new Localization(["toolkit/about/aboutReader.ftl"], true)
);

const FONT_TYPE_L10N_IDS = {
  serif: "about-reader-font-type-serif",
  "sans-serif": "about-reader-font-type-sans-serif",
  monospace: "about-reader-font-type-monospace",
};

const FONT_WEIGHT_L10N_IDS = {
  light: "about-reader-font-weight-light",
  regular: "about-reader-font-weight-regular",
  bold: "about-reader-font-weight-bold",
};

const DEFAULT_TEXT_LAYOUT = {
  fontSize: 5,
  fontType: "sans-serif",
  fontWeight: "regular",
  contentWidth: 3,
  lineSpacing: 4,
  characterSpacing: 0,
  wordSpacing: 0,
  textAlignment: "start",
};

const COLORSCHEME_L10N_IDS = {
  auto: "about-reader-color-auto-theme",
  light: "about-reader-color-light-theme",
  dark: "about-reader-color-dark-theme",
  sepia: "about-reader-color-sepia-theme",
  contrast: "about-reader-color-contrast-theme",
  gray: "about-reader-color-gray-theme",
};

const CUSTOM_THEME_COLOR_INPUTS = [
  "foreground",
  "background",
  "unvisited-links",
  "visited-links",
  "selection-highlight",
];

const COLORS_MENU_TABS = ["fxtheme", "customtheme"];

const DEFAULT_COLORS = {
  background: "#FFFFFF",
  foreground: "#14151A",
  "unvisited-links": "#0060DF",
  "visited-links": "#321C64",
  "selection-highlight": "#FFFFCC",
};

Services.telemetry.setEventRecordingEnabled("readermode", true);

const zoomOnCtrl =
  Services.prefs.getIntPref("mousewheel.with_control.action", 3) == 3;
const zoomOnMeta =
  Services.prefs.getIntPref("mousewheel.with_meta.action", 1) == 3;
const isAppLocaleRTL = Services.locale.isAppLocaleRTL;

export var AboutReader = function (
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
    console.error(errorMsg);
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

  this.forcedColorsMediaList = win.matchMedia("(forced-colors)");
  this.forcedColorsMediaList.addEventListener("change", this);

  this._topScrollChange = this._topScrollChange.bind(this);
  this._intersectionObs = new win.IntersectionObserver(this._topScrollChange, {
    root: null,
    threshold: [0, 1],
  });
  this._intersectionObs.observe(doc.querySelector(".top-anchor"));

  Services.obs.addObserver(this, "inner-window-destroyed");

  this._setupButton("close-button", this._onReaderClose.bind(this));

  // we're ready for any external setup, send a signal for that.
  this._actor.sendAsyncMessage("Reader:OnSetup");

  // set up segmented tab controls for colors menu.
  this._setupColorsTabs(
    COLORS_MENU_TABS,
    this._handleColorsTabClick.bind(this)
  );

  // fetch color scheme values from prefs.
  let colorsMenuColorSchemeValues = JSON.parse(
    Services.prefs.getCharPref("reader.color_scheme.values")
  );
  // remove contrast and gray options from regular menu.
  let colorSchemeValues = [...colorsMenuColorSchemeValues];
  colorSchemeValues.splice(colorSchemeValues.length - 2, 2);

  let colorsMenuColorSchemeOptions = colorsMenuColorSchemeValues.map(value => ({
    l10nId: COLORSCHEME_L10N_IDS[value],
    groupName: "color-scheme",
    value,
    itemClass: value + "-button",
  }));

  let colorSchemeOptions = colorSchemeValues.map(value => ({
    l10nId: COLORSCHEME_L10N_IDS[value],
    groupName: "color-scheme",
    value,
    itemClass: value + "-button",
  }));
  let colorScheme = Services.prefs.getCharPref("reader.color_scheme");

  if (Services.prefs.getBoolPref("reader.colors_menu.enabled", false)) {
    doc.getElementById("regular-color-scheme").hidden = true;
    doc.getElementById("custom-colors-color-scheme").hidden = false;

    this._setupSegmentedButton(
      "colors-menu-color-scheme-buttons",
      colorsMenuColorSchemeOptions,
      colorScheme,
      this._setColorSchemePref.bind(this)
    );
    this._setupCustomColors(
      CUSTOM_THEME_COLOR_INPUTS,
      "custom-colors-selection",
      "about-reader-custom-colors"
    );
    this._setupButton(
      "custom-colors-reset-button",
      this._resetCustomColors.bind(this)
    );
  } else {
    this._setupSegmentedButton(
      "color-scheme-buttons",
      colorSchemeOptions,
      colorScheme,
      this._setColorSchemePref.bind(this)
    );
  }

  this._setColorSchemePref(colorScheme);

  let fontTypeOptions = [
    {
      l10nId: "about-reader-font-type-sans-serif",
      groupName: "font-type",
      value: "sans-serif",
      itemClass: "sans-serif-button",
    },
    {
      l10nId: "about-reader-font-type-serif",
      groupName: "font-type",
      value: "serif",
      itemClass: "serif-button",
    },
  ];

  // TODO: Move font type pref getting alongside other prefs when old menu is retired.
  let fontType = Services.prefs.getCharPref("reader.font_type", "sans-serif");

  // Differentiates between the tick mark labels for width vs spacing controls
  // for localization purposes.
  const [standardSpacingLabel, wideSpacingLabel] = lazy.l10n.formatMessagesSync(
    [
      "about-reader-slider-label-spacing-standard",
      "about-reader-slider-label-spacing-wide",
    ]
  );

  let contentWidthSliderOptions = {
    min: 1,
    max: 9,
    ticks: 9,
    tickLabels: `[]`,
    l10nId: "about-reader-content-width-label",
    icon: "chrome://global/skin/reader/content-width-20.svg",
  };

  let lineSpacingSliderOptions = {
    min: 1,
    max: 9,
    ticks: 9,
    tickLabels: `[]`,
    l10nId: "about-reader-line-spacing-label",
    icon: "chrome://global/skin/reader/line-spacing-20.svg",
  };

  let characterSpacingSliderOptions = {
    min: 1,
    max: 9,
    ticks: 9,
    tickLabels: `["${standardSpacingLabel.value}", "${wideSpacingLabel.value}"]`,
    l10nId: "about-reader-character-spacing-label",
    icon: "chrome://global/skin/reader/character-spacing-20.svg",
  };

  let wordSpacingSliderOptions = {
    min: 1,
    max: 9,
    ticks: 9,
    tickLabels: `["${standardSpacingLabel.value}", "${wideSpacingLabel.value}"]`,
    l10nId: "about-reader-word-spacing-label",
    icon: "chrome://global/skin/reader/word-spacing-20.svg",
  };

  let textAlignmentOptions = [
    {
      l10nId: "about-reader-text-alignment-left",
      groupName: "text-alignment",
      value: "left",
      itemClass: "left-align-button",
    },
    {
      l10nId: "about-reader-text-alignment-center",
      groupName: "text-alignment",
      value: "center",
      itemClass: "center-align-button",
    },
    {
      l10nId: "about-reader-text-alignment-right",
      groupName: "text-alignment",
      value: "right",
      itemClass: "right-align-button",
    },
  ];

  // If the page is rtl, reverse order of text alignment options.
  if (isAppLocaleRTL) {
    textAlignmentOptions = textAlignmentOptions.reverse();
  }

  if (improvedTextMenuEnabled) {
    doc.getElementById("regular-text-menu").hidden = true;
    doc.getElementById("improved-text-menu").hidden = false;

    let selectorFontTypeValues = ["sans-serif", "serif", "monospace"];
    try {
      selectorFontTypeValues = JSON.parse(
        Services.prefs.getCharPref("reader.font_type.values")
      );
    } catch (e) {
      console.error(
        "There was an error fetching the font type values pref: ",
        e.message
      );
    }
    this._setupSelector(
      "font-type",
      selectorFontTypeValues,
      fontType,
      this._setFontTypeSelector.bind(this),
      FONT_TYPE_L10N_IDS
    );
    this._setFontTypeSelector(fontType);

    let fontWeightValues = ["regular", "light", "bold"];
    try {
      fontWeightValues = JSON.parse(
        Services.prefs.getCharPref("reader.font_weight.values")
      );
    } catch (e) {
      console.error(
        "There was an error fetching the font weight values pref: ",
        e.message
      );
    }
    let fontWeight = Services.prefs.getCharPref(
      "reader.font_weight",
      "regular"
    );
    this._setupSelector(
      "font-weight",
      fontWeightValues,
      fontWeight,
      this._setFontWeight.bind(this),
      FONT_WEIGHT_L10N_IDS
    );
    this._setFontWeight(fontWeight);

    let contentWidth = Services.prefs.getIntPref("reader.content_width", 3);
    this._setupSlider(
      "content-width",
      contentWidthSliderOptions,
      contentWidth,
      this._setContentWidthSlider.bind(this)
    );
    this._setContentWidthSlider(contentWidth);

    let lineSpacing = Services.prefs.getIntPref("reader.line_height", 4);
    this._setupSlider(
      "line-spacing",
      lineSpacingSliderOptions,
      lineSpacing,
      this._setLineSpacing.bind(this)
    );
    this._setLineSpacing(lineSpacing);

    let characterSpacing = Services.prefs.getIntPref(
      "reader.character_spacing",
      0
    );
    this._setupSlider(
      "character-spacing",
      characterSpacingSliderOptions,
      characterSpacing,
      this._setCharacterSpacing.bind(this)
    );
    this._setCharacterSpacing(characterSpacing);

    let wordSpacing = Services.prefs.getIntPref("reader.word_spacing", 0);
    this._setupSlider(
      "word-spacing",
      wordSpacingSliderOptions,
      wordSpacing,
      this._setWordSpacing.bind(this)
    );
    this._setWordSpacing(wordSpacing);

    let textAlignment = Services.prefs.getCharPref(
      "reader.text_alignment",
      "start"
    );
    this._setupSegmentedButton(
      "text-alignment-buttons",
      textAlignmentOptions,
      textAlignment,
      this._setTextAlignment.bind(this)
    );
    this._setTextAlignment(textAlignment);

    this._setupButton(
      "text-layout-reset-button",
      this._resetTextLayout.bind(this)
    );
  } else {
    this._setupSegmentedButton(
      "font-type-buttons",
      fontTypeOptions,
      fontType,
      this._setFontType.bind(this)
    );

    this._setupContentWidthButtons();

    this._setupLineHeightButtons();

    this._setFontType(fontType);
  }

  this._setupFontSizeButtons();

  if (win.speechSynthesis && Services.prefs.getBoolPref("narrate.enabled")) {
    new lazy.NarrateControls(win, this._languagePromise);
  }

  this._loadArticle(docContentType);
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

  receiveMessage({ data, name }) {
    const doc = this._doc;
    switch (name) {
      case "Reader:AddButton": {
        if (data.id && data.image && !doc.getElementsByClassName(data.id)[0]) {
          let btn = doc.createElement("button");
          btn.dataset.buttonid = data.id;
          btn.dataset.telemetryId = `reader-${data.telemetryId}`;
          btn.className = "toolbar-button " + data.id;
          btn.setAttribute("aria-labelledby", "label-" + data.id);
          let tip = doc.createElement("span");
          tip.className = "hover-label";
          tip.id = "label-" + data.id;
          doc.l10n.setAttributes(tip, data.l10nId);
          btn.append(tip);
          btn.style.backgroundImage = "url('" + data.image + "')";
          if (data.width && data.height) {
            btn.style.backgroundSize = `${data.width}px ${data.height}px`;
          }
          let tb = this._toolbarElement;
          tb.appendChild(btn);
          this._setupButton(data.id, button => {
            this._actor.sendAsyncMessage(
              "Reader:Clicked-" + button.dataset.buttonid,
              { article: this._article }
            );
          });
        }
        break;
      }
      case "Reader:RemoveButton": {
        if (data.id) {
          let btn = doc.getElementsByClassName(data.id)[0];
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
        const buttonLabel =
          target.attributes.getNamedItem(`data-telemetry-id`)?.value;

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
        // toolkit/components/pdfjs/content/web/viewer.mjs
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
        this._saveScrollPosition();

        this._actor.readerModeHidden();
        this.clearActor();

        // Disconnect and delete IntersectionObservers to prevent memory leaks:

        this._intersectionObs.unobserve(this._doc.querySelector(".top-anchor"));

        delete this._intersectionObs;

        break;

      case "change":
        let colorScheme;
        if (this.forcedColorsMediaList.matches) {
          colorScheme = "hcm";
        } else {
          colorScheme = Services.prefs.getCharPref("reader.color_scheme");
          // We should be changing the color scheme in relation to a preference change
          // if the user has the color scheme preference set to "Auto".
          if (colorScheme == "auto") {
            colorScheme = this.colorSchemeMediaList.matches ? "dark" : "light";
          }
        }
        this._setColorScheme(colorScheme);

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
    let plusButton, minusButton;

    if (improvedTextMenuEnabled) {
      plusButton = this._doc.querySelector(".text-size-plus-button");
      minusButton = this._doc.querySelector(".text-size-minus-button");
    } else {
      plusButton = this._doc.querySelector(".plus-button");
      minusButton = this._doc.querySelector(".minus-button");
    }

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
    let plusButton, minusButton;
    let currentSize = this._fontSize;

    if (improvedTextMenuEnabled) {
      plusButton = this._doc.querySelector(".text-size-plus-button");
      minusButton = this._doc.querySelector(".text-size-minus-button");
    } else {
      plusButton = this._doc.querySelector(".plus-button");
      minusButton = this._doc.querySelector(".minus-button");
      let fontValue = this._doc.querySelector(".font-size-value");
      fontValue.textContent = currentSize;
    }

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

  _setupSelector(id, options, initialValue, callback, l10nIds) {
    let doc = this._doc;
    let selector = doc.getElementById(`${id}-selector`);

    options.forEach(option => {
      let selectorOption = doc.createElement("option");
      let presetl10nId = l10nIds[option];
      if (presetl10nId) {
        doc.l10n.setAttributes(selectorOption, presetl10nId);
      } else {
        selectorOption.text = option;
      }
      selectorOption.value = option;
      selector.appendChild(selectorOption);
      if (option == initialValue) {
        selectorOption.setAttribute("selected", true);
      }
    });

    selector.addEventListener("change", e => {
      callback(e.target.value);
    });
  },

  _setFontTypeSelector(newFontType) {
    if (newFontType === "sans-serif") {
      this._doc.documentElement.style.setProperty(
        "--font-family",
        "Helvetica, Arial, sans-serif"
      );
    } else if (newFontType === "serif") {
      this._doc.documentElement.style.setProperty(
        "--font-family",
        'Georgia, "Times New Roman", serif'
      );
    } else if (newFontType === "monospace") {
      this._doc.documentElement.style.setProperty(
        "--font-family",
        '"Courier New", Courier, monospace'
      );
    } else {
      this._doc.documentElement.style.setProperty(
        "--font-family",
        `"${newFontType}"`
      );
    }

    lazy.AsyncPrefs.set("reader.font_type", newFontType);
  },

  _setFontWeight(newFontWeight) {
    if (newFontWeight === "light") {
      this._doc.documentElement.style.setProperty("--font-weight", "lighter");
    } else if (newFontWeight === "regular") {
      this._doc.documentElement.style.setProperty("--font-weight", "normal");
    } else if (newFontWeight === "bold") {
      this._doc.documentElement.style.setProperty("--font-weight", "bolder");
    }

    lazy.AsyncPrefs.set("reader.font_weight", newFontWeight);
  },

  _setupSlider(id, options, initialValue, callback) {
    let doc = this._doc;
    let slider = doc.createElement("moz-slider");

    slider.setAttribute("min", options.min);
    slider.setAttribute("max", options.max);
    slider.setAttribute("value", initialValue);
    slider.setAttribute("ticks", options.ticks);
    slider.setAttribute("tick-labels", options.tickLabels);
    slider.setAttribute("data-l10n-id", options.l10nId);
    slider.setAttribute("data-l10n-attrs", "label");
    slider.setAttribute("slider-icon", options.icon);

    slider.addEventListener("slider-changed", e => {
      callback(e.detail);
    });

    let sliderContainer = doc.getElementById(`${id}-slider`);
    sliderContainer.appendChild(slider);
  },

  // Rename this function to setContentWidth when the old menu is retired.
  _setContentWidthSlider(newContentWidth) {
    // We map the slider range [1-9] to 20-60em.
    let width = 20 + 5 * (newContentWidth - 1) + "em";
    this._doc.body.style.setProperty("--content-width", width);
    this._scheduleToolbarOverlapHandler();
    return lazy.AsyncPrefs.set(
      "reader.content_width",
      parseInt(newContentWidth)
    );
  },

  _setLineSpacing(newLineSpacing) {
    // We map the slider range [1-9] to 1-2.6em.
    let spacing = 1 + 0.2 * (newLineSpacing - 1) + "em";
    this._containerElement.style.setProperty("--line-height", spacing);
    return lazy.AsyncPrefs.set("reader.line_height", parseInt(newLineSpacing));
  },

  _setCharacterSpacing(newCharSpacing) {
    // We map the slider range [1-9] to 0-0.24em.
    let spacing = (newCharSpacing - 1) * 0.03;
    this._containerElement.style.setProperty(
      "--letter-spacing",
      `${parseFloat(spacing).toFixed(2)}em`
    );
    lazy.AsyncPrefs.set("reader.character_spacing", parseInt(newCharSpacing));
  },

  _setWordSpacing(newWordSpacing) {
    // We map the slider range [1-9] to 0-0.4em.
    let spacing = (newWordSpacing - 1) * 0.05;
    this._containerElement.style.setProperty(
      "--word-spacing",
      `${parseFloat(spacing).toFixed(2)}em`
    );
    lazy.AsyncPrefs.set("reader.word_spacing", parseInt(newWordSpacing));
  },

  _setTextAlignment(newTextAlignment) {
    if (this._textAlignment === newTextAlignment) {
      return false;
    }

    if (newTextAlignment === "start") {
      let startAlignButton;
      if (isAppLocaleRTL) {
        startAlignButton = this._doc.querySelector(".right-align-button");
      } else {
        startAlignButton = this._doc.querySelector(".left-align-button");
      }
      startAlignButton.click();
    }

    this._containerElement.style.setProperty(
      "--text-alignment",
      newTextAlignment
    );

    lazy.AsyncPrefs.set("reader.text_alignment", newTextAlignment);
    return true;
  },

  async _resetTextLayout() {
    let doc = this._doc;
    const initial = DEFAULT_TEXT_LAYOUT;
    const changeEvent = new Event("change", { bubbles: true });

    this._resetFontSize();
    let fontType = doc.getElementById("font-type-selector");
    fontType.value = initial.fontType;
    fontType.dispatchEvent(changeEvent);

    let fontWeight = doc.getElementById("font-weight-selector");
    fontWeight.value = initial.fontWeight;
    fontWeight.dispatchEvent(changeEvent);

    let contentWidth = doc.querySelector("#content-width-slider moz-slider");
    contentWidth.setAttribute("value", initial.contentWidth);
    this._setContentWidthSlider(initial.contentWidth);

    let lineSpacing = doc.querySelector("#line-spacing-slider moz-slider");
    lineSpacing.setAttribute("value", initial.lineSpacing);
    this._setLineSpacing(initial.lineSpacing);

    let characterSpacing = doc.querySelector(
      "#character-spacing-slider moz-slider"
    );
    characterSpacing.setAttribute("value", initial.characterSpacing);
    this._setCharacterSpacing(initial.characterSpacing);

    let wordSpacing = doc.querySelector("#word-spacing-slider moz-slider");
    wordSpacing.setAttribute("value", initial.wordSpacing);
    this._setWordSpacing(initial.wordSpacing);

    this._setTextAlignment(initial.textAlignment);
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

    if (!this._win.matchMedia("(forced-colors)").matches) {
      if (newColorScheme === "auto") {
        this._colorScheme = this.colorSchemeMediaList.matches
          ? "dark"
          : "light";
      } else {
        this._colorScheme = newColorScheme;
      }
    } else {
      this._colorScheme = "hcm";
    }

    if (this._colorScheme == "custom") {
      const colorInputs = this._doc.querySelectorAll("color-input");
      colorInputs.forEach(input => {
        // Set document body styles to pref values.
        let property = input.getAttribute("prop-name");
        let pref = `reader.custom_colors.${property}`;
        let customColor = Services.prefs.getStringPref(pref, "");
        // If customColor is truthy, set the value from pref.
        if (customColor) {
          let cssProp = `--custom-theme-${property}`;
          this._doc.body.style.setProperty(cssProp, customColor);
        }
      });
    }

    bodyClasses.add(this._colorScheme);
  },

  // Pref values include "auto", "dark", "light", "sepia",
  // "gray", "contrast", and "custom"
  _setColorSchemePref(colorSchemePref, fromInputEvent = false) {
    // The input event for the last selected segmented button is fired
    // upon loading a reader article in the same session. To prevent it
    // from overwriting custom colors, we return false.
    const colorsMenuEnabled = Services.prefs.getBoolPref(
      "reader.colors_menu.enabled",
      false
    );
    if (colorsMenuEnabled && this._colorScheme == "custom" && fromInputEvent) {
      lastSelectedTheme = colorSchemePref;
      return false;
    }
    this._setColorScheme(colorSchemePref);

    lazy.AsyncPrefs.set("reader.color_scheme", colorSchemePref);
    return true;
  },

  _setFontType(newFontType) {
    if (this._fontType === newFontType) {
      return false;
    }

    let bodyClasses = this._doc.body.classList;

    if (this._fontType) {
      bodyClasses.remove(this._fontType);
    }

    this._fontType = newFontType;
    bodyClasses.add(this._fontType);

    lazy.AsyncPrefs.set("reader.font_type", this._fontType);

    return true;
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
          { ...this._doc.nodePrincipal?.originAttributes },
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

    let setImageMargins = function (img) {
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
        img.onload = function () {
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

  _showError() {
    this._headerElement.classList.remove("reader-show-element");
    this._contentElement.classList.remove("reader-show-element");

    this._doc.l10n.setAttributes(
      this._messageElement,
      "about-reader-load-error"
    );
    this._doc.l10n.setAttributes(
      this._doc.getElementById("reader-title"),
      "about-reader-load-error"
    );
    this._messageElement.style.display = "block";

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

    const slow = article.readingTimeMinsSlow;
    const fast = article.readingTimeMinsFast;
    const fastStr = lazy.numberFormat.format(fast);
    const readTimeRange = lazy.numberFormat.formatRange(fast, slow);
    this._doc.l10n.setAttributes(
      this._readTimeElement,
      "about-reader-estimated-read-time",
      {
        range: fast === slow ? `~${fastStr}` : `${readTimeRange}`,
        rangePlural:
          fast === slow
            ? lazy.pluralRules.select(fast)
            : lazy.pluralRules.selectRange(fast, slow),
      }
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
    this._getScrollPosition();

    Services.obs.notifyObservers(this._win, "AboutReader:Ready");

    this._doc.dispatchEvent(
      new this._win.CustomEvent("AboutReaderContentReady", {
        bubbles: true,
        cancelable: false,
      })
    );
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

      this._doc.l10n.setAttributes(
        this._messageElement,
        "about-reader-loading"
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
      item.htmlFor = radioButton.id;
      item.classList.add(option.itemClass);
      doc.l10n.setAttributes(item, option.l10nId);

      segmentedButton.appendChild(item);

      radioButton.addEventListener(
        "input",
        function (aEvent) {
          if (!aEvent.isTrusted) {
            return;
          }

          let labels = segmentedButton.children;
          for (let label of labels) {
            label.removeAttribute("checked");
          }

          let setOption = callback(option.value, true);
          if (setOption) {
            aEvent.target.setAttribute("checked", "true");
            aEvent.target.nextElementSibling.setAttribute("checked", "true");
          }
        },
        true
      );

      if (option.value === initialValue) {
        radioButton.setAttribute("checked", "true");
        item.setAttribute("checked", "true");
      }
    }
  },

  _setupButton(id, callback) {
    let button = this._doc.querySelector("." + id);
    button.removeAttribute("hidden");
    button.addEventListener(
      "click",
      function (aEvent) {
        if (!aEvent.isTrusted) {
          return;
        }

        let btn = aEvent.target;
        callback(btn);
      },
      true
    );
  },

  _handleColorsTabClick(option) {
    let doc = this._doc;
    if (option == "customtheme") {
      this._setColorSchemePref("custom");
      lazy.AsyncPrefs.set("reader.color_scheme", "custom");

      // Store the last selected preset theme button.
      const colorSchemePresets = doc.querySelector(
        ".colors-menu-color-scheme-buttons"
      );
      const labels = colorSchemePresets.querySelectorAll("label");
      labels.forEach(label => {
        if (label.hasAttribute("checked")) {
          lastSelectedTheme = label.className.split("-")[0];
        }
      });
    }
    if (option == "fxtheme") {
      this._setColorSchemePref(lastSelectedTheme);
      lazy.AsyncPrefs.set("reader.color_scheme", lastSelectedTheme);
      // set the last selected button to checked.
      const colorSchemePresets = doc.querySelector(
        ".colors-menu-color-scheme-buttons"
      );
      const labels = colorSchemePresets.querySelectorAll("label");
      labels.forEach(label => {
        if (label.className == `${lastSelectedTheme}-button`) {
          label.setAttribute("checked", "true");
          label.previousElementSibling.setAttribute("checked", "true");
        }
      });
    }
  },

  _setupColorsTabs(options, callback) {
    let doc = this._doc;
    let colorScheme = Services.prefs.getCharPref("reader.color_scheme");
    for (let option of options) {
      let tabButton = doc.getElementById(`tabs-deck-button-${option}`);
      // Open custom theme tab if color scheme is set to custom.
      if (option == "customtheme" && colorScheme == "custom") {
        tabButton.click();
      }
      tabButton.addEventListener(
        "click",
        function (aEvent) {
          if (!aEvent.isTrusted) {
            return;
          }

          callback(option);
        },
        true
      );
    }
  },

  _setupColorInput(prop) {
    let doc = this._doc;
    let input = doc.createElement("color-input");
    input.setAttribute("prop-name", prop);
    let labelL10nId = `about-reader-custom-colors-${prop}`;
    input.setAttribute("data-l10n-id", labelL10nId);

    let pref = `reader.custom_colors.${prop}`;
    let customColor = Services.prefs.getStringPref(pref, "");
    // Set the swatch color from prefs if one has been set.
    if (customColor) {
      input.setAttribute("color", customColor);
    } else {
      let defaultColor = DEFAULT_COLORS[prop];
      input.setAttribute("color", defaultColor);
    }

    // Attach event listener to update the pref and page colors on input.
    input.addEventListener("color-picked", e => {
      const cssPropToUpdate = `--custom-theme-${prop}`;
      this._doc.body.style.setProperty(cssPropToUpdate, e.detail);

      const prefToUpdate = `reader.custom_colors.${prop}`;
      lazy.AsyncPrefs.set(prefToUpdate, e.detail);
    });

    return input;
  },

  _setupCustomColors(options, id) {
    let doc = this._doc;
    const list = doc.getElementsByClassName(id)[0];

    for (let option of options) {
      let listItem = doc.createElement("li");
      let colorInput = this._setupColorInput(option);
      listItem.appendChild(colorInput);
      list.appendChild(listItem);
    }
  },

  _resetCustomColors() {
    // Need to reset prefs, page colors, and color inputs.
    const colorInputs = this._doc.querySelectorAll("color-input");
    colorInputs.forEach(input => {
      let property = input.getAttribute("prop-name");
      let pref = `reader.custom_colors.${property}`;
      lazy.AsyncPrefs.set(pref, "");

      // Set css props to empty strings so they use fallback value.
      let cssProp = `--custom-theme-${property}`;
      this._doc.body.style.setProperty(cssProp, "");

      let defaultColor = DEFAULT_COLORS[property];
      input.setAttribute("color", defaultColor);
    });
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
  _openDropdown(dropdown) {
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

  _scrollToSavedPosition(pos) {
    this._win.scrollTo({
      top: pos,
      left: 0,
      behavior: "auto",
    });
    gScrollPositions.delete(this._win.location.href);
  },

  /*
   * Save reader view vertical scroll position
   */
  _saveScrollPosition() {
    let scrollTop = this._doc.documentElement.scrollTop;
    gScrollPositions.set(this._win.location.href, scrollTop);
  },

  /*
   * Scroll reader view to a saved position
   */
  _getScrollPosition() {
    let scrollPosition = gScrollPositions.get(this._win.location.href);
    if (scrollPosition !== undefined) {
      if (this._doc.readyState == "complete") {
        this._scrollToSavedPosition(scrollPosition);
      } else {
        this._win.addEventListener(
          "load",
          () => {
            this._scrollToSavedPosition(scrollPosition);
          },
          { once: true }
        );
      }
    }
  },
};
