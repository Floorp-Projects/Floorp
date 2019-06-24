/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

"use strict";

ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

const showOnAnyType = () => false;
const hideOnAnyType = () => true;
const hideOnThemeType = (addonType) => addonType === "theme";

// The reasons string used as a key in this Map is expected to stay in sync
// with the reasons string used in the "abuseReports.ftl" locale file and
// the suggestions templates included in abuse-reports-xulframe.html.
const ABUSE_REASONS = window.ABUSE_REPORT_REASONS = {
  "damage": {
    isExampleHidden: showOnAnyType,
    isReasonHidden: hideOnThemeType,
  },
  "spam": {
    isExampleHidden: showOnAnyType,
    isReasonHidden: showOnAnyType,
  },
  "settings": {
    hasSuggestions: true,
    isExampleHidden: hideOnAnyType,
    isReasonHidden: hideOnThemeType,
  },
  "deceptive": {
    isExampleHidden: showOnAnyType,
    isReasonHidden: showOnAnyType,
  },
  "broken": {
    hasAddonTypeL10nId: true,
    hasAddonTypeSuggestionTemplate: true,
    hasSuggestions: true,
    isExampleHidden: hideOnThemeType,
    isReasonHidden: showOnAnyType,
  },
  "policy": {
    hasSuggestions: true,
    isExampleHidden: hideOnAnyType,
    isReasonHidden: showOnAnyType,
  },
  "unwanted": {
    isExampleHidden: showOnAnyType,
    isReasonHidden: hideOnThemeType,
  },
  "other": {
    isExampleHidden: hideOnAnyType,
    isReasonHidden: showOnAnyType,
  },
};

function getReasonL10nId(reason, addonType) {
  let l10nId = `abuse-report-${reason}-reason`;
  // Special case reasons that have a addonType-specific
  // l10n id.
  if (ABUSE_REASONS[reason].hasAddonTypeL10nId) {
    l10nId += `-${addonType}`;
  }
  return l10nId;
}

function getSuggestionsTemplate(reason, addonType) {
  const reasonInfo = ABUSE_REASONS[reason];
  if (!reasonInfo.hasSuggestions) {
    return null;
  }
  let templateId = `tmpl-suggestions-${reason}`;
  // Special case reasons that have a addonType-specific
  // suggestion template.
  if (reasonInfo.hasAddonTypeSuggestionTemplate) {
    templateId += `-${addonType}`;
  }
  return document.getElementById(templateId);
}

// Map of the learnmore links metadata, keyed by link element class.
const LEARNMORE_LINKS = {
  ".abuse-report-learnmore": {
    path: "reporting-extensions-and-themes-abuse",
  },
  ".abuse-settings-search-learnmore": {
    path: "prefs-search",
  },
  ".abuse-settings-homepage-learnmore": {
    path: "prefs-homepage",
  },
  ".abuse-policy-learnmore": {
    baseURL: "https://www.mozilla.org/%LOCALE%/",
    path: "about/legal/report-infringement/",
  },
};

// Format links that match the selector in the LEARNMORE_LINKS map
// found in a given container element.
function formatLearnMoreURLs(containerEl) {
  for (const [linkClass, linkInfo] of Object.entries(LEARNMORE_LINKS)) {
    for (const element of containerEl.querySelectorAll(linkClass)) {
      const baseURL = linkInfo.baseURL ?
        Services.urlFormatter.formatURL(linkInfo.baseURL) :
        Services.urlFormatter.formatURLPref("app.support.baseURL");

      element.href = baseURL + linkInfo.path;
    }
  }
}

// Define a set of getters from a Map<propertyName, selector>.
function defineElementSelectorsGetters(object, propsMap) {
  const props = Object.entries(propsMap).reduce((acc, entry) => {
    const [name, selector] = entry;
    acc[name] = {get: () => object.querySelector(selector)};
    return acc;
  }, {});
  Object.defineProperties(object, props);
}

// Define a set of properties getters and setters for a
// Map<propertyName, attributeName>.
function defineElementAttributesProperties(object, propsMap) {
  const props = Object.entries(propsMap).reduce((acc, entry) => {
    const [name, attr] = entry;
    acc[name] = {
      get: () => object.getAttribute(attr),
      set: (value) => {
        object.setAttribute(attr, value);
      },
    };
    return acc;
  }, {});
  Object.defineProperties(object, props);
}

// Return an object with properties associated to elements
// found using the related selector in the propsMap.
function getElements(containerEl, propsMap) {
  return Object.entries(propsMap).reduce((acc, entry) => {
    const [name, selector] = entry;
    let elements = containerEl.querySelectorAll(selector);
    acc[name] = elements.length > 1 ? elements : elements[0];
    return acc;
  }, {});
}

function dispatchCustomEvent(el, eventName, detail) {
  el.dispatchEvent(new CustomEvent(eventName, {detail}));
}

// This WebComponent extends the li item to represent an abuse report reason
// and it is responsible for:
// - embedding a photon styled radio buttons
// - localizing the reason list item
// - optionally embedding a localized example, positioned
//   below the reason label, and adjusts the item height
//   accordingly
class AbuseReasonListItem extends HTMLLIElement {
  constructor() {
    super();
    defineElementAttributesProperties(this, {
      addonType: "addon-type",
      reason: "report-reason",
      checked: "checked",
    });
  }

  connectedCallback() {
    this.update();
  }

  async update() {
    if (this.reason !== "other" && !this.addonType) {
      return;
    }

    const {reason, checked, addonType} = this;

    this.textContent = "";
    const content = document.importNode(this.template.content, true);

    if (reason) {
      const reasonId = `abuse-reason-${reason}`;
      const reasonInfo = ABUSE_REASONS[reason] || {};

      const {labelEl, descriptionEl, radioEl} = getElements(content, {
        labelEl: "label",
        descriptionEl: ".reason-description",
        radioEl: "input[type=radio]",
      });

      labelEl.setAttribute("for", reasonId);
      radioEl.id = reasonId;
      radioEl.value = reason;
      radioEl.checked = !!checked;

      // This reason has a different localized description based on the
      // addon type.
      document.l10n.setAttributes(
        descriptionEl, getReasonL10nId(reason, addonType));

      // Show the reason example if supported for the addon type.
      if (!reasonInfo.isExampleHidden(addonType)) {
        const exampleEl = content.querySelector(".reason-example");
        document.l10n.setAttributes(
          exampleEl, `abuse-report-${reason}-example`);
        exampleEl.hidden = false;
      }
    }

    formatLearnMoreURLs(content);

    this.appendChild(content);
  }

  get template() {
    return document.getElementById("tmpl-reason-listitem");
  }
}

// This WebComponents implements the first step of the abuse
// report submission and embeds a randomized reasons list.
class AbuseReasonsPanel extends HTMLElement {
  constructor() {
    super();
    defineElementAttributesProperties(this, {
      addonType: "addon-type",
    });
  }

  connectedCallback() {
    this.update();
  }

  update() {
    if (!this.isConnected || !this.addonType) {
      return;
    }

    const {addonType} = this;

    this.textContent = "";
    const content = document.importNode(this.template.content, true);

    const {titleEl, listEl} = getElements(content, {
      titleEl: ".abuse-report-title",
      listEl: "ul.abuse-report-reasons",
    });

    // Change the title l10n-id if the addon type is theme.
    document.l10n.setAttributes(titleEl, `abuse-report-title-${addonType}`);

    // Create the randomized list of reasons.
    const reasons = Object.keys(ABUSE_REASONS)
                          .filter(reason => reason !== "other")
                          .sort(() => Math.random() - 0.5);

    for (const reason of reasons) {
      const reasonInfo = ABUSE_REASONS[reason];
      if (!reasonInfo || reasonInfo.isReasonHidden(addonType)) {
        // Skip an extension only reason while reporting a theme.
        continue;
      }
      const item = document.createElement("li", {
        is: "abuse-report-reason-listitem",
      });
      item.reason = reason;
      item.addonType = addonType;

      listEl.prepend(item);
    }

    listEl.firstElementChild.checked = true;
    formatLearnMoreURLs(content);

    this.appendChild(content);
  }

  get template() {
    return document.getElementById("tmpl-reasons-panel");
  }
}

// This WebComponent is responsible for the suggestions, which are:
// - generated based on a template keyed by abuse report reason
// - localized by assigning fluent ids generated from the abuse report reason
// - learn more and extension support url are then generated when the
//   specific reason expects it
class AbuseReasonSuggestions extends HTMLElement {
  constructor() {
    super();
    defineElementAttributesProperties(this, {
      extensionSupportURL: "extension-support-url",
      reason: "report-reason",
    });
  }

  update() {
    const {addonType, extensionSupportURL, reason} = this;

    if (!addonType) {
      return;
    }

    this.textContent = "";

    let template = getSuggestionsTemplate(reason, addonType);
    if (template) {
      let content = document.importNode(template.content, true);

      formatLearnMoreURLs(content);

      let extSupportLink = content.querySelector("a.extension-support-link");
      if (extSupportLink) {
        extSupportLink.href = extensionSupportURL;
      }

      this.appendChild(content);
      this.hidden = false;
    } else {
      this.hidden = true;
    }
  }

  get LEARNMORE_LINKS() {
    return Object.keys(LEARNMORE_LINKS);
  }
}

// This WebComponents implements the last step of the abuse report submission.
class AbuseSubmitPanel extends HTMLElement {
  constructor() {
    super();
    defineElementAttributesProperties(this, {
      addonType: "addon-type",
      reason: "report-reason",
      extensionSupportURL: "extensionSupportURL",
    });
    defineElementSelectorsGetters(this, {
      _textarea: "textarea",
      _title: ".abuse-reason-title",
      _suggestions: "abuse-report-reason-suggestions",
    });
  }

  connectedCallback() {
    this.render();
  }

  render() {
    this.textContent = "";
    this.appendChild(document.importNode(this.template.content, true));
  }

  update() {
    if (!this.isConnected || !this.addonType) {
      return;
    }
    const {addonType, reason, _suggestions, _title} = this;
    document.l10n.setAttributes(_title, getReasonL10nId(reason, addonType));
    _suggestions.reason = reason;
    _suggestions.addonType = addonType;
    _suggestions.extensionSupportURL = this.extensionSupportURL;
    _suggestions.update();
  }

  clear() {
    this._textarea.value = "";
  }

  get template() {
    return document.getElementById("tmpl-submit-panel");
  }
}

// This WebComponent provides the abuse report
class AbuseReport extends HTMLElement {
  constructor() {
    super();
    this._report = null;
    defineElementSelectorsGetters(this, {
      _form: "form",
      _textarea: "textarea",
      _radioCheckedReason: "[type=radio]:checked",
      _reasonsPanel: "abuse-report-reasons-panel",
      _submitPanel: "abuse-report-submit-panel",
      _reasonsPanelButtons: ".abuse-report-reasons-buttons",
      _submitPanelButtons: ".abuse-report-submit-buttons",
      _iconClose: ".abuse-report-close-icon",
      _btnNext: "button.abuse-report-next",
      _btnCancel: "button.abuse-report-cancel",
      _btnGoBack: "button.abuse-report-goback",
      _btnSubmit: "button.abuse-report-submit",
      _addonIconElement: ".abuse-report-header img.addon-icon",
      _addonNameElement: ".abuse-report-header .addon-name",
      _linkAddonAuthor: ".abuse-report-header .addon-author-box a.author",
    });
  }

  connectedCallback() {
    this.render();

    this.addEventListener("click", this);

    // Start listening to keydown events (to close the modal
    // when Escape has been pressed and to handling the keyboard
    // navigation).
    document.addEventListener("keydown", this);
  }

  disconnectedCallback() {
    this.textContent = "";
    this.removeEventListener("click", this);
    document.removeEventListener("keydown", this);
  }

  handleEvent(evt) {
    if (!this.isConnected || !this.addon) {
      return;
    }

    switch (evt.type) {
      case "keydown":
        if (evt.key === "Escape") {
          // Prevent Esc to close the panel if the textarea is
          // empty.
          if (this.message && !this._submitPanel.hidden) {
            return;
          }
          this.cancel();
        }
        this.handleKeyboardNavigation(evt);
        break;
      case "click":
        if (evt.target === this._iconClose ||
            evt.target === this._btnCancel) {
          // NOTE: clear the focus on the clicked element to ensure that
          // -moz-focusring pseudo class is not still set on the element
          // when the panel is going to be shown again (See Bug 1560949).
          evt.target.blur();
          this.cancel();
        }
        if (evt.target === this._btnNext) {
          this.switchToSubmitMode();
        }
        if (evt.target === this._btnGoBack) {
          this.switchToListMode();
        }
        if (evt.target === this._btnSubmit) {
          this.submit();
        }
        if (evt.target.localName === "a") {
          evt.preventDefault();
          evt.stopPropagation();
          const url = evt.target.getAttribute("href");
          // Ignore if url is empty.
          if (url) {
            window.windowRoot.ownerGlobal.openWebLinkIn(url, "tab", {
              relatedToCurrent: true,
            });
          }
        }
        break;
    }
  }

  handleKeyboardNavigation(evt) {
    if (evt.keyCode !== evt.DOM_VK_TAB ||
      evt.altKey || evt.controlKey || evt.metaKey) {
      return;
    }

    const fm = Services.focus;
    const backward = evt.shiftKey;

    const isFirstFocusableElement = el => {
      // Also consider the document body as a valid first focusable element.
      if (el === document.body) {
        return true;
      }
      // XXXrpl unfortunately there is no way to get the first focusable element
      // without asking the focus manager to move focus to it (similar strategy
      // is also being used in about:prefereces subdialog.js).
      const rv = el == fm.moveFocus(window, null, fm.MOVEFOCUS_FIRST, 0);
      fm.setFocus(el, 0);
      return rv;
    };

    // If the focus is exiting the panel while navigating
    // backward, focus the previous element sibling on the
    // Firefox UI.
    if (backward && isFirstFocusableElement(evt.target)) {
      evt.preventDefault();
      evt.stopImmediatePropagation();
      const chromeWin = window.windowRoot.ownerGlobal;
      Services.focus.moveFocus(
        chromeWin, null,
        Services.MOVEFOCUS_BACKWARD, Services.focus.FLAG_BYKEY);
    }
  }

  render() {
    this.textContent = "";
    this.appendChild(document.importNode(this.template.content, true));
  }

  async update() {
    if (!this.addon) {
      return;
    }

    const {
      addonId,
      _addonIconElement,
      _addonNameElement,
      _linkAddonAuthor,
      _reasonsPanel,
      _submitPanel,
    } = this;

    // Ensure that the first step of the abuse submission is the one
    // currently visible.
    this.switchToListMode();

    // Cancel the abuse report if the addon is not an extension or theme.
    if (!["extension", "theme"].includes(this.addonType)) {
      this.cancel();
      return;
    }

    _addonNameElement.textContent = this.addonName;

    _linkAddonAuthor.href = this.authorURL || this.homepageURL;
    _linkAddonAuthor.textContent = this.authorName;
    document.l10n.setAttributes(_linkAddonAuthor.parentNode,
                                "abuse-report-addon-authored-by",
                                {"author-name": this.authorName});

    _addonIconElement.setAttribute("src", this.iconURL);

    _reasonsPanel.addonType = this.addonType;
    _reasonsPanel.update();

    _submitPanel.addonType = this.addonType;
    _submitPanel.reason = this.reason;
    _submitPanel.extensionSupportURL = this.supportURL;
    _submitPanel.update();

    this.focus();
    dispatchCustomEvent(this, "abuse-report:updated", {
      addonId, panel: "reasons",
    });
  }

  setAbuseReport(abuseReport) {
    this._report = abuseReport;
    // Clear the textarea from any previously entered content.
    this._submitPanel.clear();

    if (abuseReport) {
      this.update();
      this.hidden = false;
    } else {
      this.hidden = true;
    }
  }

  focus() {
    if (!this.isConnected || !this.addon) {
      return;
    }
    if (this._reasonsPanel.hidden) {
      const {_textarea} = this;
      _textarea.focus();
      _textarea.select();
    } else {
      const {_radioCheckedReason} = this;
      if (_radioCheckedReason) {
        _radioCheckedReason.focus();
      }
    }
  }

  cancel() {
    if (!this.isConnected || !this.addon) {
      return;
    }
    this._report = null;
    dispatchCustomEvent(this, "abuse-report:cancel");
  }

  submit() {
    if (!this.isConnected || !this.addon) {
      return;
    }
    dispatchCustomEvent(this, "abuse-report:submit", {
      addonId: this.addonId,
      reason: this.reason,
      message: this.message,
      report: this._report,
    });
  }

  switchToSubmitMode() {
    if (!this.isConnected || !this.addon) {
      return;
    }
    this._submitPanel.reason = this.reason;
    this._submitPanel.update();
    this._reasonsPanel.hidden = true;
    this._reasonsPanelButtons.hidden = true;
    this._submitPanel.hidden = false;
    this._submitPanelButtons.hidden = false;
    // Adjust the focused element when switching to the submit panel.
    this.focus();
    dispatchCustomEvent(this, "abuse-report:updated", {
      addonId: this.addonId, panel: "submit",
    });
  }

  switchToListMode() {
    if (!this.isConnected || !this.addon) {
      return;
    }
    this._submitPanel.hidden = true;
    this._submitPanelButtons.hidden = true;
    this._reasonsPanel.hidden = false;
    this._reasonsPanelButtons.hidden = false;
    // Adjust the focused element when switching back to the list of reasons.
    this.focus();
    dispatchCustomEvent(this, "abuse-report:updated", {
      addonId: this.addonId, panel: "reasons",
    });
  }

  get addon() {
    return this._report && this._report.addon;
  }

  get addonId() {
    return this.addon && this.addon.id;
  }

  get addonName() {
    return this.addon && this.addon.name;
  }

  get addonType() {
    return this.addon && this.addon.type;
  }

  get addonCreator() {
    return this.addon && this.addon.creator;
  }

  get homepageURL() {
    const {addon} = this;
    return addon && addon.homepageURL || this.authorURL || "";
  }

  get authorName() {
    // The author name may be missing on some of the test extensions
    // (or for temporarily installed add-ons).
    return this.addonCreator && this.addonCreator.name || "";
  }

  get authorURL() {
    return this.addonCreator && this.addonCreator.url || "";
  }

  get iconURL() {
    return this.addon && this.addon.iconURL;
  }

  get supportURL() {
    return this.addon && this.addon.supportURL || this.homepageURL || "";
  }

  get message() {
    return this._form.elements.message.value;
  }

  get reason() {
    return this._form.elements.reason.value;
  }

  get template() {
    return document.getElementById("tmpl-abuse-report");
  }
}

customElements.define("abuse-report-reason-listitem",
                      AbuseReasonListItem, {extends: "li"});
customElements.define("abuse-report-reason-suggestions",
                      AbuseReasonSuggestions);
customElements.define("abuse-report-reasons-panel", AbuseReasonsPanel);
customElements.define("abuse-report-submit-panel", AbuseSubmitPanel);
customElements.define("addon-abuse-report", AbuseReport);

window.addEventListener("load", () => {
  document.body.prepend(document.createElement("addon-abuse-report"));
}, {once: true});
