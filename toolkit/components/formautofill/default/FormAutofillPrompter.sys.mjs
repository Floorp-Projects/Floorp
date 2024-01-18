/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements doorhanger singleton that wraps up the PopupNotifications and handles
 * the doorhager UI for formautofill related features.
 */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { FormAutofill } from "resource://autofill/FormAutofill.sys.mjs";
import { FormAutofillUtils } from "resource://gre/modules/shared/FormAutofillUtils.sys.mjs";

import { AutofillTelemetry } from "resource://autofill/AutofillTelemetry.sys.mjs";
import { showConfirmation } from "resource://gre/modules/FillHelpers.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.sys.mjs",
  formAutofillStorage: "resource://autofill/FormAutofillStorage.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () =>
  FormAutofill.defineLogGetter(lazy, "FormAutofillPrompter")
);

const l10n = new Localization(
  [
    "browser/preferences/formAutofill.ftl",
    "toolkit/formautofill/formAutofill.ftl",
    "branding/brand.ftl",
  ],
  true
);

const { ENABLED_AUTOFILL_CREDITCARDS_PREF } = FormAutofill;

let CONTENT = {};

/**
 * `AutofillDoorhanger` provides a base for both address capture and credit card
 * capture doorhanger notifications. It handles the UI generation and logic
 * related to displaying the doorhanger,
 *
 * The UI data sourced from the `CONTENT` variable is used for rendering. Derived classes
 * should override the `render()` method to customize the layout.
 */
export class AutofillDoorhanger {
  /**
   * Constructs an instance of the `AutofillDoorhanger` class.
   *
   * @param {object} browser   The browser where the doorhanger will be displayed.
   * @param {object} oldRecord The old record that can be merged with the new record
   * @param {object} newRecord The new record submitted by users
   */
  static headerClass = "address-capture-header";
  static descriptionClass = "address-capture-description";
  static contentClass = "address-capture-content";
  static menuButtonId = "address-capture-menu-button";

  static preferenceURL = null;
  static learnMoreURL = null;

  constructor(browser, oldRecord, newRecord, flowId) {
    this.browser = browser;
    this.oldRecord = oldRecord ?? {};
    this.newRecord = newRecord;
    this.flowId = flowId;
  }

  get ui() {
    return CONTENT[this.constructor.name];
  }

  // PopupNotification appends a "-notification" suffix to the id to avoid
  // id conflict.
  get notificationId() {
    return this.ui.id + "-notification";
  }

  // The popup notification element
  get panel() {
    return this.browser.ownerDocument.getElementById(this.notificationId);
  }

  get doc() {
    return this.browser.ownerDocument;
  }

  get chromeWin() {
    return this.browser.ownerGlobal;
  }

  /*
   * An autofill doorhanger consists 3 parts - header, description, and content
   * The content part contains customized UI layout for this doorhanger
   */

  // The container of the header part
  static header(panel) {
    return panel.querySelector(`.${AutofillDoorhanger.headerClass}`);
  }
  get header() {
    return AutofillDoorhanger.header(this.panel);
  }

  // The container of the description part
  static description(panel) {
    return panel.querySelector(`.${AutofillDoorhanger.descriptionClass}`);
  }
  get description() {
    return AutofillDoorhanger.description(this.panel);
  }

  // The container of the content part
  static content(panel) {
    return panel.querySelector(`.${AutofillDoorhanger.contentClass}`);
  }
  get content() {
    return AutofillDoorhanger.content(this.panel);
  }

  static menuButton(panel) {
    return panel.querySelector(`#${AutofillDoorhanger.menuButtonId}`);
  }
  get menuButton() {
    return AutofillDoorhanger.menuButton(this.panel);
  }

  static menuPopup(panel) {
    return AutofillDoorhanger.menuButton(panel).querySelector(
      `.toolbar-menupopup`
    );
  }
  get menuPopup() {
    return AutofillDoorhanger.menuPopup(this.panel);
  }

  static preferenceButton(panel) {
    return AutofillDoorhanger.menuButton(panel).querySelector(
      `[data-l10n-id=address-capture-manage-address-button]`
    );
  }
  static learnMoreButton(panel) {
    return AutofillDoorhanger.menuButton(panel).querySelector(
      `[data-l10n-id=address-capture-learn-more-button]`
    );
  }

  get preferenceURL() {
    return this.constructor.preferenceURL;
  }
  get learnMoreURL() {
    return this.constructor.learnMoreURL;
  }

  onMenuItemClick(evt) {
    AutofillTelemetry.recordDoorhangerClicked(
      this.constructor.telemetryType,
      evt,
      this.constructor.telemetryObject,
      this.flowId
    );

    if (evt == "open-pref") {
      this.browser.ownerGlobal.openPreferences(this.preferenceURL);
    } else if (evt == "learn-more") {
      const url =
        Services.urlFormatter.formatURLPref("app.support.baseURL") +
        this.learnMoreURL;
      this.browser.ownerGlobal.openWebLinkIn(url, "tab", {
        relatedToCurrent: true,
      });
    }
  }

  // Build the doorhanger markup
  render() {
    this.renderHeader();

    this.renderDescription();

    // doorhanger specific content
    this.renderContent();
  }

  renderHeader() {
    // Render the header text
    const text = this.header.querySelector(`p`);
    this.doc.l10n.setAttributes(text, this.ui.header.l10nId);

    // Render the menu button
    if (!this.ui.menu?.length || AutofillDoorhanger.menuButton(this.panel)) {
      return;
    }

    const button = this.doc.createElement("button");
    button.setAttribute("id", AutofillDoorhanger.menuButtonId);
    button.setAttribute("class", "address-capture-icon-button");
    this.doc.l10n.setAttributes(button, "address-capture-open-menu-button");

    const menupopup = this.doc.createXULElement("menupopup");
    menupopup.setAttribute("id", AutofillDoorhanger.menuButtonId);
    menupopup.setAttribute("class", "toolbar-menupopup");

    for (const [index, element] of this.ui.menu.entries()) {
      const menuitem = this.doc.createXULElement("menuitem");
      this.doc.l10n.setAttributes(menuitem, element.l10nId);
      /* eslint-disable mozilla/balanced-listeners */
      menuitem.addEventListener("command", event => {
        event.stopPropagation();
        this.onMenuItemClick(element.evt);
      });
      menupopup.appendChild(menuitem);

      if (index != this.ui.menu.length - 1) {
        menupopup.appendChild(this.doc.createXULElement("menuseparator"));
      }
    }

    button.appendChild(menupopup);
    /* eslint-disable mozilla/balanced-listeners */
    button.addEventListener("click", event => {
      event.stopPropagation();
      menupopup.openPopup(button, "after_start");
    });
    this.header.appendChild(button);
  }

  renderDescription() {
    if (this.ui.description?.l10nId) {
      const text = this.description.querySelector(`p`);
      this.doc.l10n.setAttributes(text, this.ui.description.l10nId);
      this.description?.setAttribute("style", "");
    } else {
      this.description?.setAttribute("style", "display:none");
    }
  }

  onEventCallback(state) {
    lazy.log.debug(`Doorhanger receives event callback: ${state}`);

    if (state == "showing") {
      this.render();
    }
  }

  async show() {
    AutofillTelemetry.recordDoorhangerShown(
      this.constructor.telemetryType,
      this.constructor.telemetryObject,
      this.flowId
    );

    let options = {
      ...this.ui.options,
      eventCallback: state => this.onEventCallback(state),
    };

    this.#setAnchor();

    return new Promise(resolve => {
      this.resolve = resolve;
      this.chromeWin.PopupNotifications.show(
        this.browser,
        this.ui.id,
        this.getNotificationHeader?.() ?? "",
        this.ui.anchor.id,
        ...this.#createActions(),
        options
      );
    });
  }

  /**
   * Closes the doorhanger with a given action.
   * This method is specifically intended for closing the doorhanger in scenarios
   * other than clicking the main or secondary buttons.
   */
  closeDoorhanger(action) {
    this.resolve(action);
    const notification = this.chromeWin.PopupNotifications.getNotification(
      this.ui.id,
      this.browser
    );
    if (notification) {
      this.chromeWin.PopupNotifications.remove(notification);
    }
  }

  /**
   * Create an image element for notification anchor if it doesn't already exist.
   */
  #setAnchor() {
    let anchor = this.doc.getElementById(this.ui.anchor.id);
    if (!anchor) {
      // Icon shown on URL bar
      anchor = this.doc.createXULElement("image");
      anchor.id = this.ui.anchor.id;
      anchor.setAttribute("src", this.ui.anchor.URL);
      anchor.classList.add("notification-anchor-icon");
      anchor.setAttribute("role", "button");
      anchor.setAttribute("tooltiptext", this.ui.anchor.tooltiptext);

      const popupBox = this.doc.getElementById("notification-popup-box");
      popupBox.appendChild(anchor);
    }
  }

  /**
   * Generate the main action and secondary actions from content parameters and
   * promise resolve.
   */
  #createActions() {
    function getLabelAndAccessKey(param) {
      const msg = l10n.formatMessagesSync([{ id: param.l10nId }])[0];
      return {
        label: msg.attributes.find(x => x.name == "label").value,
        accessKey: msg.attributes.find(x => x.name == "accessKey").value,
        dismiss: param.dismiss,
      };
    }

    const mainActionParams = this.ui.footer.mainAction;
    const secondaryActionParams = this.ui.footer.secondaryActions;

    const callback = () => {
      AutofillTelemetry.recordDoorhangerClicked(
        this.constructor.telemetryType,
        mainActionParams.callbackState,
        this.constructor.telemetryObject,
        this.flowId
      );

      this.resolve(mainActionParams.callbackState);
    };

    const mainAction = {
      ...getLabelAndAccessKey(mainActionParams),
      callback,
    };

    let secondaryActions = [];
    for (const params of secondaryActionParams) {
      const callback = () => {
        AutofillTelemetry.recordDoorhangerClicked(
          this.constructor.telemetryType,
          params.callbackState,
          this.constructor.telemetryObject,
          this.flowId
        );

        this.resolve(params.callbackState);
      };

      secondaryActions.push({
        ...getLabelAndAccessKey(params),
        callback,
      });
    }

    return [mainAction, secondaryActions];
  }
}

export class AddressSaveDoorhanger extends AutofillDoorhanger {
  static preferenceURL = "privacy-address-autofill";
  static learnMoreURL = "automatically-fill-your-address-web-forms";
  static editButtonId = "address-capture-edit-address-button";

  static telemetryType = AutofillTelemetry.ADDRESS;
  static telemetryObject = "capture_doorhanger";

  constructor(browser, oldRecord, newRecord, flowId) {
    super(browser, oldRecord, newRecord, flowId);
  }

  static editButton(panel) {
    return panel.querySelector(`#${AddressSaveDoorhanger.editButtonId}`);
  }
  get editButton() {
    return AddressSaveDoorhanger.editButton(this.panel);
  }

  /**
   * Formats a line by comparing the old and the new address field and returns an array of
   * <span> elements that represents the formatted line.
   *
   * @param {Array<Array<string>>} datalist An array of pairs, where each pair contains old and new data.
   * @param {boolean}              showDiff True to format the text line that highlight the diff part.
   *
   * @returns {Array<HTMLSpanElement>} An array of formatted text elements.
   */
  #formatLine(datalist, showDiff) {
    const createSpan = (text, style = null) => {
      let s;

      if (showDiff) {
        if (style == "remove") {
          s = this.doc.createElement("del");
          s.setAttribute("class", "address-update-text-diff-removed");
        } else if (style == "add") {
          s = this.doc.createElement("mark");
          s.setAttribute("class", "address-update-text-diff-added");
        } else {
          s = this.doc.createElement("span");
        }
      } else {
        s = this.doc.createElement("span");
      }
      s.textContent = text;
      return s;
    };

    let spans = [];
    let previousField;
    for (const [field, oldData, newData] of datalist) {
      if (!oldData && !newData) {
        continue;
      }

      // Always add a whitespace between field data that we put in the same line.
      // Ex. first-name: John, family-name: Doe becomes
      // "John Doe"
      if (spans.length) {
        if (previousField == "address-level2" && field == "address-level1") {
          spans.push(createSpan(", "));
        } else {
          spans.push(createSpan(" "));
        }
      }

      if (!oldData) {
        spans.push(createSpan(newData, "add"));
      } else if (!newData || oldData == newData) {
        // The same
        spans.push(createSpan(oldData));
      } else if (newData.startsWith(oldData)) {
        // Have the same prefix
        const diff = newData.slice(oldData.length).trim();
        spans.push(createSpan(newData.slice(0, newData.length - diff.length)));
        spans.push(createSpan(diff, "add"));
      } else if (newData.endsWith(oldData)) {
        // Have the same suffix
        const diff = newData.slice(0, newData.length - oldData.length).trim();
        spans.push(createSpan(diff, "add"));
        spans.push(createSpan(newData.slice(diff.length)));
      } else {
        spans.push(createSpan(oldData, "remove"));
        spans.push(createSpan(" "));
        spans.push(createSpan(newData, "add"));
      }

      previousField = field;
    }

    return spans;
  }

  #formatTextByAddressCategory(fieldName) {
    let data = [];
    switch (fieldName) {
      case "street-address":
        data = [
          [
            fieldName,
            FormAutofillUtils.toOneLineAddress(
              this.oldRecord["street-address"]
            ),
            FormAutofillUtils.toOneLineAddress(
              this.newRecord["street-address"]
            ),
          ],
        ];
        break;
      case "address":
        data = ["address-level2", "address-level1", "postal-code"].map(
          field => [field, this.oldRecord[field], this.newRecord[field]]
        );
        break;
      case "name":
      case "country":
      case "tel":
      case "email":
      case "organization":
        data = [
          [fieldName, this.oldRecord[fieldName], this.newRecord[fieldName]],
        ];
        break;
    }

    const showDiff = !!Object.keys(this.oldRecord).length;
    return this.#formatLine(data, showDiff);
  }

  renderDescription() {
    if (lazy.formAutofillStorage.addresses.isEmpty()) {
      super.renderDescription();
    } else {
      this.description?.setAttribute("style", "display:none");
    }
  }

  renderContent() {
    this.content.replaceChildren();

    // Each section contains address fields that are grouped together while displaying
    // the doorhanger.
    for (const { imgClass, categories } of this.ui.content.sections) {
      // Add all the address fields that are in the same category
      let texts = [];
      categories.forEach(category => {
        const line = this.#formatTextByAddressCategory(category);
        if (line.length) {
          texts.push(line);
        }
      });

      // If there is no data for this section, just ignore it.
      if (!texts.length) {
        continue;
      }

      const section = this.doc.createElement("div");
      section.setAttribute("class", "address-save-update-row-container");

      // Add image icon for this section
      //const img = this.doc.createElement("img");
      const img = this.doc.createXULElement("image");
      img.setAttribute("class", imgClass);
      section.appendChild(img);

      // Each line is consisted of multiple <span> to form diff style texts
      const lineContainer = this.doc.createElement("div");
      for (const spans of texts) {
        const p = this.doc.createElement("p");
        spans.forEach(span => p.appendChild(span));
        lineContainer.appendChild(p);
      }
      section.appendChild(lineContainer);

      this.content.appendChild(section);

      // Put the edit address button in the first section
      if (!AddressSaveDoorhanger.editButton(this.panel)) {
        const button = this.doc.createElement("button");
        button.setAttribute("id", AddressSaveDoorhanger.editButtonId);
        button.setAttribute("class", "address-capture-icon-button");
        this.doc.l10n.setAttributes(
          button,
          "address-capture-edit-address-button"
        );

        // The element will be removed after the popup is closed
        /* eslint-disable mozilla/balanced-listeners */
        button.addEventListener("click", event => {
          event.stopPropagation();
          this.closeDoorhanger("edit-address");
        });
        section.appendChild(button);
      }
    }
  }

  // The record to be saved by this doorhanger
  recordToSave() {
    return this.newRecord;
  }
}

/**
 * Address Update doorhanger and Address Save doorhanger have the same implementation.
 * The only difference is UI.
 */
export class AddressUpdateDoorhanger extends AddressSaveDoorhanger {
  static telemetryObject = "update_doorhanger";
}

export class AddressEditDoorhanger extends AutofillDoorhanger {
  static telemetryType = AutofillTelemetry.ADDRESS;
  static telemetryObject = "edit_doorhanger";

  constructor(browser, record, flowId) {
    // Address edit dialog doesn't have "old" record
    super(browser, null, record, flowId);

    this.country = record.country || FormAutofill.DEFAULT_REGION;
  }

  // Address edit doorhanger changes layout according to the country
  #layout = null;
  get layout() {
    if (this.#layout?.country != this.country) {
      this.#layout = FormAutofillUtils.getFormFormat(this.country);
    }
    return this.#layout;
  }

  get country() {
    return this.newRecord.country;
  }

  set country(c) {
    if (this.newRecord.country == c) {
      return;
    }

    // `recordToSave` only contains the latest data the current country support.
    // For example, if a country doesn't have `address-level2`, `recordToSave`
    // will not have the address field.
    // `newRecord` is where we keep all the data regardless what the country is.
    // Merge `recordToSave` to `newRecord` before switching country to keep
    // `newRecord` update-to-date.
    this.newRecord = Object.assign(this.newRecord, this.recordToSave());

    // The layout of the address edit doorhanger should be changed when the
    // country is changed.
    this.#buildCountrySpecificAddressFields();
  }

  renderContent() {
    this.content.replaceChildren();

    this.#buildAddressFields(this.content, this.ui.content.fixedFields);

    this.#buildCountrySpecificAddressFields();
  }

  // Put address fields that should be in the same line together.
  // Determined by the `newLine` property that is defined in libaddressinput
  #buildAddressFields(container, fields) {
    const createRowContainer = () => {
      const div = this.doc.createElement("div");
      div.setAttribute("class", "address-edit-row-container");
      container.appendChild(div);
      return div;
    };

    let row = null;
    let createRow = true;
    for (const { fieldId, newLine } of fields) {
      if (createRow) {
        row = createRowContainer();
      }
      row.appendChild(this.#createInputField(fieldId));
      createRow = newLine;
    }
  }

  #buildCountrySpecificAddressFields() {
    const fixedFieldIds = this.ui.content.fixedFields.map(f => f.fieldId);
    let container = this.doc.getElementById(
      "country-specific-fields-container"
    );
    if (container) {
      // Country-specific fields might be rebuilt after users update the country
      // field, so if the container already exists, we remove all its childern and
      // then rebuild it.
      container.replaceChildren();
    } else {
      container = this.doc.createElement("div");
      container.setAttribute("id", "country-specific-fields-container");

      // Find where to insert country-specific fields
      const nth = fixedFieldIds.indexOf(
        this.ui.content.countrySpecificFieldsBefore
      );
      this.content.insertBefore(container, this.content.children[nth]);
    }

    this.#buildAddressFields(
      container,
      // Filter out fields that are always displayed
      this.layout.fieldsOrder.filter(f => !fixedFieldIds.includes(f.fieldId))
    );
  }

  #buildCountryMenupopup() {
    const menupopup = this.doc.createXULElement("menupopup");

    let menuitem = this.doc.createXULElement("menuitem");
    menuitem.setAttribute("value", "");
    menupopup.appendChild(menuitem);

    const countries = [...FormAutofill.countries.entries()].sort((e1, e2) =>
      e1[1].localeCompare(e2[1])
    );
    for (const [country] of countries) {
      const countryName = Services.intl.getRegionDisplayNames(undefined, [
        country.toLowerCase(),
      ]);
      menuitem = this.doc.createXULElement("menuitem");
      menuitem.setAttribute("label", countryName);
      menuitem.setAttribute("value", country);
      menupopup.appendChild(menuitem);
    }

    return menupopup;
  }

  #buildAddressLevel1Menupopup() {
    const menupopup = this.doc.createXULElement("menupopup");

    let menuitem = this.doc.createXULElement("menuitem");
    menuitem.setAttribute("value", "");
    menupopup.appendChild(menuitem);

    for (const [regionCode, regionName] of this.layout.addressLevel1Options) {
      menuitem = this.doc.createXULElement("menuitem");
      menuitem.setAttribute("label", regionCode);
      menuitem.setAttribute("value", regionName);
      menupopup.appendChild(menuitem);
    }

    return menupopup;
  }

  /**
   * Creates an input field with a label and attaches it to a container element.
   * The type of the input field is determined by the `fieldName`.
   *
   * @param {string} fieldName The name of the address field
   */
  #createInputField(fieldName) {
    const div = this.doc.createElement("div");
    div.setAttribute("class", "address-edit-input-container");

    const inputId = AddressEditDoorhanger.getInputId(fieldName);
    const label = this.doc.createElement("label");
    label.setAttribute("for", inputId);

    switch (fieldName) {
      case "address-level1":
        this.doc.l10n.setAttributes(label, this.layout.addressLevel1L10nId);
        break;
      case "address-level2":
        this.doc.l10n.setAttributes(label, this.layout.addressLevel2L10nId);
        break;
      case "address-level3":
        this.doc.l10n.setAttributes(label, this.layout.addressLevel3L10nId);
        break;
      case "postal-code":
        this.doc.l10n.setAttributes(label, this.layout.postalCodeL10nId);
        break;
      case "country":
        // workaround because `autofill-address-country` is already defined
        this.doc.l10n.setAttributes(
          label,
          `autofill-address-${fieldName}-only`
        );
        break;
      default:
        this.doc.l10n.setAttributes(label, `autofill-address-${fieldName}`);
        break;
    }
    div.appendChild(label);

    let input;
    let popup;
    if ("street-address".includes(fieldName)) {
      input = this.doc.createElement("textarea");
      input.setAttribute("rows", 3);
    } else if (fieldName == "country") {
      input = this.doc.createXULElement("menulist");
      popup = this.#buildCountryMenupopup();
      popup.addEventListener("popuphidden", e => e.stopPropagation());
      input.appendChild(popup);

      // The element will be removed after the popup is closed
      /* eslint-disable mozilla/balanced-listeners */
      input.addEventListener("command", event => {
        event.stopPropagation();
        this.country = input.selectedItem.value;
      });
    } else if (
      fieldName == "address-level1" &&
      this.layout.addressLevel1Options
    ) {
      input = this.doc.createXULElement("menulist");
      popup = this.#buildAddressLevel1Menupopup();
      popup.addEventListener("popuphidden", e => e.stopPropagation());
      input.appendChild(popup);
    } else {
      input = this.doc.createElement("input");
    }

    input.setAttribute("id", inputId);

    const value = this.newRecord[fieldName] ?? "";
    if (popup) {
      const menuitem = Array.from(popup.childNodes).find(
        item =>
          item.label.toLowerCase() === value?.toLowerCase() ||
          item.value.toLowerCase() === value?.toLowerCase()
      );
      input.selectedItem = menuitem;
    } else {
      input.value = value;
    }

    div.appendChild(input);

    return div;
  }

  /*
   * This method generates a unique input ID using the field name of the address field.
   *
   * @param {string} fieldName The name of the address field
   */
  static getInputId(fieldName) {
    return `address-edit-${fieldName}-input`;
  }

  /*
   * Return a regular expression that matches the ID pattern generated by getInputId.
   */
  static #getInputIdMatchRegexp() {
    const regex = /^address-edit-(.+)-input$/;
    return regex;
  }

  /**
   * Collects data from all visible address field inputs within the doorhanger.
   * Since address fields may vary by country, only fields present for the
   * current country's address format are included in the record.
   */
  recordToSave() {
    let record = {};
    const regex = AddressEditDoorhanger.#getInputIdMatchRegexp();
    const elements = this.panel.querySelectorAll("input, textarea, menulist");
    for (const element of elements) {
      const match = element.id.match(regex);
      if (match && match[1]) {
        record[match[1]] = element.value;
      }
    }
    return record;
  }

  onEventCallback(state) {
    super.onEventCallback(state);

    // Close the edit address doorhanger when it has been dismissed.
    if (state == "dismissed") {
      this.closeDoorhanger("cancel");
    }
  }
}

export class CreditCardSaveDoorhanger extends AutofillDoorhanger {
  static contentClass = "credit-card-capture-content";

  static telemetryType = AutofillTelemetry.CREDIT_CARD;
  static telemetryObject = "capture_doorhanger";

  static spotlightURL = "about:preferences#privacy-credit-card-autofill";

  constructor(browser, oldRecord, newRecord, flowId) {
    super(browser, oldRecord, newRecord, flowId);
  }

  /**
   * We have not yet sync address and credit card design. After syncing,
   * we should be able to use the same "class"
   */
  static content(panel) {
    return panel.querySelector(`.${CreditCardSaveDoorhanger.contentClass}`);
  }
  get content() {
    return CreditCardSaveDoorhanger.content(this.panel);
  }

  addCheckboxListener() {
    if (!this.ui.options.checkbox) {
      return;
    }

    const { checkbox } = this.panel;
    if (checkbox && !checkbox.hidden) {
      checkbox.addEventListener("command", event => {
        let { secondaryButton, menubutton } =
          event.target.closest("popupnotification");
        let checked = event.target.checked;
        Services.prefs.setBoolPref("services.sync.engine.creditcards", checked);
        secondaryButton.disabled = checked;
        menubutton.disabled = checked;
        lazy.log.debug("Set creditCard sync to", checked);
      });
    }
  }

  removeCheckboxListener() {
    if (!this.ui.options.checkbox) {
      return;
    }

    const { checkbox } = this.panel;

    if (checkbox && !checkbox.hidden) {
      checkbox.removeEventListener(
        "command",
        this.ui.options.checkbox.callback
      );
    }
  }

  appendDescription() {
    const docFragment = this.doc.createDocumentFragment();

    const label = this.doc.createXULElement("label");
    this.doc.l10n.setAttributes(label, this.ui.description.l10nId);
    docFragment.appendChild(label);

    const descriptionWrapper = this.doc.createXULElement("hbox");
    descriptionWrapper.className = "desc-message-box";

    const number =
      this.newRecord["cc-number"] || this.newRecord["cc-number-decrypted"];
    const name = this.newRecord["cc-name"];
    const network = lazy.CreditCard.getType(number);

    const descriptionIcon = lazy.CreditCard.getCreditCardLogo(network);
    if (descriptionIcon) {
      const icon = this.doc.createXULElement("image");
      if (
        typeof descriptionIcon == "string" &&
        (descriptionIcon.includes("cc-logo") ||
          descriptionIcon.includes("icon-credit"))
      ) {
        icon.setAttribute("src", descriptionIcon);
      }
      descriptionWrapper.appendChild(icon);
    }

    const description = this.doc.createXULElement("description");
    description.textContent =
      `${lazy.CreditCard.getMaskedNumber(number)}` + (name ? `, ${name}` : ``);

    descriptionWrapper.appendChild(description);
    docFragment.appendChild(descriptionWrapper);

    this.content.appendChild(docFragment);
  }

  appendPrivacyPanelLink() {
    const privacyLinkElement = this.doc.createXULElement("label", {
      is: "text-link",
    });
    privacyLinkElement.setAttribute("useoriginprincipal", true);
    privacyLinkElement.setAttribute(
      "href",
      CreditCardSaveDoorhanger.spotlightURL ||
        "about:preferences#privacy-form-autofill"
    );

    const linkId = `autofill-options-link${
      AppConstants.platform == "macosx" ? "-osx" : ""
    }`;
    this.doc.l10n.setAttributes(privacyLinkElement, linkId);

    this.content.appendChild(privacyLinkElement);
  }

  // TODO: Currently, the header and description are unused. Align
  // these with the address doorhanger's implementation during
  // the credit card doorhanger redesign.
  getNotificationHeader() {
    return l10n.formatValueSync(this.ui.header.l10nId);
  }

  renderHeader() {
    // Not implement
  }

  renderDescription() {
    // Not implement
  }

  renderContent() {
    this.content.replaceChildren();

    this.appendDescription();

    this.appendPrivacyPanelLink();
  }

  onEventCallback(state) {
    super.onEventCallback(state);

    if (state == "removed" || state == "dismissed") {
      this.removeCheckboxListener();
    } else if (state == "shown") {
      this.addCheckboxListener();
    }
  }

  // The record to be saved by this doorhanger
  recordToSave() {
    return this.newRecord;
  }
}

export class CreditCardUpdateDoorhanger extends CreditCardSaveDoorhanger {
  static telemetryType = AutofillTelemetry.CREDIT_CARD;
  static telemetryObject = "update_doorhanger";

  constructor(browser, oldRecord, newRecord, flowId) {
    super(browser, oldRecord, newRecord, flowId);
  }
}

CONTENT = {
  [AddressSaveDoorhanger.name]: {
    id: "address-save-update",
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: l10n.formatValueSync("autofill-message-tooltip"),
    },
    header: {
      l10nId: "address-capture-save-doorhanger-header",
    },
    description: {
      l10nId: "address-capture-save-doorhanger-description",
    },
    menu: [
      {
        l10nId: "address-capture-manage-address-button",
        evt: "open-pref",
      },
      {
        l10nId: "address-capture-learn-more-button",
        evt: "learn-more",
      },
    ],
    content: {
      // We divide address data into two sections to display in the Address Save Doorhanger.
      sections: [
        {
          imgClass: "address-capture-img-address",
          categories: [
            "name",
            "organization",
            "street-address",
            "address",
            "country",
          ],
        },
        {
          imgClass: "address-capture-img-email",
          categories: ["email", "tel"],
        },
      ],
    },
    footer: {
      mainAction: {
        l10nId: "address-capture-save-button",
        callbackState: "create",
      },
      secondaryActions: [
        {
          l10nId: "address-capture-not-now-button",
          callbackState: "cancel",
        },
      ],
    },
    options: {
      autofocus: true,
      persistWhileVisible: true,
      hideClose: true,
    },
  },

  [AddressUpdateDoorhanger.name]: {
    id: "address-save-update",
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: l10n.formatValueSync("autofill-message-tooltip"),
    },
    header: {
      l10nId: "address-capture-update-doorhanger-header",
    },
    menu: [
      {
        l10nId: "address-capture-manage-address-button",
        evt: "open-pref",
      },
      {
        l10nId: "address-capture-learn-more-button",
        evt: "learn-more",
      },
    ],
    content: {
      // Addresses fields are categoried into two sections, each section
      // has its own icon
      sections: [
        {
          imgClass: "address-capture-img-address",
          categories: [
            "name",
            "organization",
            "street-address",
            "address",
            "country",
          ],
        },
        {
          imgClass: "address-capture-img-email",
          categories: ["email", "tel"],
        },
      ],
    },
    footer: {
      mainAction: {
        l10nId: "address-capture-update-button",
        callbackState: "update",
      },
      secondaryActions: [
        {
          l10nId: "address-capture-not-now-button",
          callbackState: "cancel",
        },
      ],
    },
    options: {
      autofocus: true,
      persistWhileVisible: true,
      hideClose: true,
    },
  },

  [AddressEditDoorhanger.name]: {
    id: "address-edit",
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: l10n.formatValueSync("autofill-message-tooltip"),
    },
    header: {
      l10nId: "address-capture-edit-doorhanger-header",
    },
    menu: null,
    content: {
      // We start by organizing the fields in a specific order:
      // name, organization, and country are fixed and come first.
      // These are followed by country-specific fields, which are
      // laid out differently for each country (as referenced from libaddressinput).
      // Finally, we place the telephone and email fields at the end.
      countrySpecificFieldsBefore: "tel",
      fixedFields: [
        { fieldId: "name", newLine: true },
        { fieldId: "organization", newLine: true },
        { fieldId: "country", newLine: true },
        { fieldId: "tel", newLine: false },
        { fieldId: "email", newLine: true },
      ],
    },
    footer: {
      mainAction: {
        l10nId: "address-capture-save-button",
        callbackState: "save",
      },
      secondaryActions: [
        {
          l10nId: "address-capture-cancel-button",
          callbackState: "cancel",
          dismiss: true,
        },
      ],
    },
    options: {
      autofocus: true,
      persistWhileVisible: true,
      hideClose: true,
    },
  },

  [CreditCardSaveDoorhanger.name]: {
    id: "credit-card-save-update",
    anchor: {
      id: "autofill-credit-card-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: l10n.formatValueSync("autofill-message-tooltip"),
    },
    header: {
      l10nId: "credit-card-save-doorhanger-header",
    },
    description: {
      l10nId: "credit-card-save-doorhanger-description",
    },
    content: {},
    footer: {
      mainAction: {
        l10nId: "credit-card-capture-save-button",
        callbackState: "create",
      },
      secondaryActions: [
        {
          l10nId: "credit-card-capture-cancel-button",
          callbackState: "cancel",
        },
        {
          l10nId: "credit-card-capture-never-save-button",
          callbackState: "disable",
        },
      ],
    },
    options: {
      persistWhileVisible: true,
      popupIconURL: "chrome://formautofill/content/icon-credit-card.svg",
      hideClose: true,

      checkbox: {
        get checked() {
          return Services.prefs.getBoolPref("services.sync.engine.creditcards");
        },
        get label() {
          // Only set the label when the fallowing conditions existed:
          // - sync account is set
          // - credit card sync is disabled
          // - credit card sync is available
          // otherwise return null label to hide checkbox.
          return Services.prefs.prefHasUserValue("services.sync.username") &&
            !Services.prefs.getBoolPref("services.sync.engine.creditcards") &&
            Services.prefs.getBoolPref(
              "services.sync.engine.creditcards.available"
            )
            ? l10n.formatValueSync(
                "credit-card-doorhanger-credit-cards-sync-checkbox"
              )
            : null;
        },
      },
    },
  },

  [CreditCardUpdateDoorhanger.name]: {
    id: "credit-card-save-update",
    anchor: {
      id: "autofill-credit-card-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: l10n.formatValueSync("autofill-message-tooltip"),
    },
    header: {
      l10nId: "credit-card-update-doorhanger-header",
    },
    description: {
      l10nId: "credit-card-update-doorhanger-description",
    },
    content: {},
    footer: {
      mainAction: {
        l10nId: "credit-card-capture-update-button",
        callbackState: "update",
      },
      secondaryActions: [
        {
          l10nId: "credit-card-capture-save-new-button",
          callbackState: "create",
        },
      ],
    },
    options: {
      persistWhileVisible: true,
      popupIconURL: "chrome://formautofill/content/icon-credit-card.svg",
      hideClose: true,
    },
  },
};

export let FormAutofillPrompter = {
  async promptToSaveCreditCard(
    browser,
    storage,
    flowId,
    { oldRecord, newRecord }
  ) {
    const showUpdateDoorhanger = !!Object.keys(oldRecord).length;

    const { ownerGlobal: win } = browser;
    win.MozXULElement.insertFTLIfNeeded(
      "toolkit/formautofill/formAutofill.ftl"
    );

    let action;
    const doorhanger = showUpdateDoorhanger
      ? new CreditCardUpdateDoorhanger(browser, oldRecord, newRecord, flowId)
      : new CreditCardSaveDoorhanger(browser, oldRecord, newRecord, flowId);
    action = await doorhanger.show();

    lazy.log.debug(`Doorhanger action is ${action}`);

    if (action == "cancel") {
      return;
    } else if (action == "disable") {
      Services.prefs.setBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF, false);
      return;
    }

    if (!(await FormAutofillUtils.ensureLoggedIn()).authenticated) {
      lazy.log.warn("User canceled encryption login");
      return;
    }

    this._updateStorageAfterInteractWithPrompt(
      browser,
      storage,
      "credit-card",
      action == "update" ? oldRecord : null,
      doorhanger.recordToSave()
    );
  },

  /**
   * Show save or update address doorhanger
   *
   * @param {Element<browser>} browser  Browser to show the save/update address prompt
   * @param {object} storage Address storage
   * @param {string} flowId Unique GUID to record a series of the same user action
   * @param {object} options
   * @param {object} [options.oldRecord] Record to be merged
   * @param {object} [options.newRecord] Record with more information
   */
  async promptToSaveAddress(
    browser,
    storage,
    flowId,
    { oldRecord, newRecord }
  ) {
    const showUpdateDoorhanger = !!Object.keys(oldRecord).length;

    lazy.log.debug(
      `Show the ${showUpdateDoorhanger ? "update" : "save"} address doorhanger`
    );

    const { ownerGlobal: win } = browser;
    await win.ensureCustomElements("moz-support-link");
    win.MozXULElement.insertFTLIfNeeded(
      "toolkit/formautofill/formAutofill.ftl"
    );
    // address-autofill-* are defined in browser/preferences now
    win.MozXULElement.insertFTLIfNeeded("browser/preferences/formAutofill.ftl");

    let doorhanger;
    let action;
    while (true) {
      doorhanger = showUpdateDoorhanger
        ? new AddressUpdateDoorhanger(browser, oldRecord, newRecord, flowId)
        : new AddressSaveDoorhanger(browser, oldRecord, newRecord, flowId);
      action = await doorhanger.show();

      if (action == "edit-address") {
        doorhanger = new AddressEditDoorhanger(
          browser,
          { ...oldRecord, ...newRecord },
          flowId
        );
        action = await doorhanger.show();

        // If users cancel the edit address doorhanger, show the save/update
        // doorhanger again.
        if (action == "cancel") {
          continue;
        }
      }

      break;
    }

    lazy.log.debug(`Doorhanger action is ${action}`);

    if (action == "cancel") {
      return;
    }

    this._updateStorageAfterInteractWithPrompt(
      browser,
      storage,
      "address",
      showUpdateDoorhanger ? oldRecord : null,
      doorhanger.recordToSave()
    );
  },

  // TODO: Simplify the code after integrating credit card prompt to use AutofillDoorhanger
  async _updateStorageAfterInteractWithPrompt(
    browser,
    storage,
    type,
    oldRecord,
    newRecord
  ) {
    let changedGUID = null;
    if (oldRecord) {
      changedGUID = oldRecord.guid;
      await storage.update(changedGUID, newRecord, true);
    } else {
      changedGUID = await storage.add(newRecord);
    }
    storage.notifyUsed(changedGUID);

    const hintId = `confirmation-hint-${type}-${
      oldRecord ? "updated" : "created"
    }`;
    showConfirmation(browser, hintId);
  },
};
