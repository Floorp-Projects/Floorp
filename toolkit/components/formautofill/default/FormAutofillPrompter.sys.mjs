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
  FormAutofillNameUtils:
    "resource://gre/modules/shared/FormAutofillNameUtils.sys.mjs",
  formAutofillStorage: "resource://autofill/FormAutofillStorage.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () =>
  FormAutofill.defineLogGetter(lazy, "FormAutofillPrompter")
);
ChromeUtils.defineLazyGetter(lazy, "l10n", () => {
  return new Localization(
    ["browser/preferences/formAutofill.ftl", "branding/brand.ftl"],
    true
  );
});

const { ENABLED_AUTOFILL_CREDITCARDS_PREF } = FormAutofill;

const GetStringFromName = FormAutofillUtils.stringBundle.GetStringFromName;
const formatStringFromName =
  FormAutofillUtils.stringBundle.formatStringFromName;
const brandShortName =
  FormAutofillUtils.brandBundle.GetStringFromName("brandShortName");
let autofillOptsKey = "autofillOptionsLink";
if (AppConstants.platform == "macosx") {
  autofillOptsKey += "OSX";
}

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
    this.oldRecord = oldRecord;
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

  static preferenceButton(panel) {
    const menu = AutofillDoorhanger.menuButton(panel);
    return menu.menupopup.querySelector(
      `[data-l10n-id=address-capture-manage-address-button]`
    );
  }
  static learnMoreButton(panel) {
    const menu = AutofillDoorhanger.menuButton(panel);
    return menu.menupopup.querySelector(
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

    const button = this.doc.createXULElement("toolbarbutton");
    button.setAttribute("id", AutofillDoorhanger.menuButtonId);

    const menupopup = this.doc.createXULElement("menupopup");
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
    button.addEventListener("command", event => {
      event.stopPropagation();
      button.menupopup.openPopup(button, "after_start");
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

  show(resolve, callback = null) {
    AutofillTelemetry.recordDoorhangerShown(
      this.constructor.telemetryType,
      this.constructor.telemetryObject,
      this.flowId
    );

    // call render to setup the doorhanger
    this.render();

    let options = {
      ...this.ui.options,
      eventCallback: state => {
        callback?.(state);

        if (state == "removed") {
          // Remove data that for this submission when the doorhanger is removed
          this.content.replaceChildren();
        }
      },
    };

    AutofillDoorhanger.setAnchor(this.doc, this.ui.anchor);

    return this.chromeWin.PopupNotifications.show(
      this.browser,
      this.ui.id,
      "",
      this.ui.anchor.id,
      ...AutofillDoorhanger.createActions(
        this.ui.footer.mainAction,
        this.ui.footer.secondaryActions,
        resolve,
        {
          type: this.constructor.telemetryType,
          object: this.constructor.telemetryObject,
          flowId: this.flowId,
        }
      ),
      options
    );
  }

  /**
   * Create an image element for notification anchor if it doesn't already exist.
   *
   * @static
   * @param {Document} doc - The document object where the anchor element should be created or modified.
   * @param {object} anchor - An object containing the attributes for the anchor element.
   * @param {string} anchor.id - The ID to assign to the anchor element.
   * @param {string} anchor.URL - The image URL to set for the anchor element.
   * @param {string} anchor.tooltiptext - The tooltip text to set for the anchor element.
   */
  // TODO: this is a static method so credit card doorhangers can also use this API.
  //       we can change this to non-static after we impleemnt doorhanger with `class AutofillDoorhanger`
  static setAnchor(doc, anchor) {
    let anchorElement = doc.getElementById(anchor.id);
    if (!anchorElement) {
      const popupBox = doc.getElementById("notification-popup-box");
      // Icon shown on URL bar
      anchorElement = doc.createXULElement("image");
      anchorElement.id = anchor.id;
      anchorElement.setAttribute("src", anchor.URL);
      anchorElement.classList.add("notification-anchor-icon");
      anchorElement.setAttribute("role", "button");
      anchorElement.setAttribute("tooltiptext", anchor.tooltiptext);
      popupBox.appendChild(anchorElement);
    }
  }

  /**
   * Generate the main action and secondary actions from content parameters and
   * promise resolve.
   *
   * @private
   * @param  {object} mainActionParams
   *         Parameters for main action.
   * @param  {Array<object>} secondaryActionParams
   *         Array of the parameters for secondary actions.
   * @param  {Function} resolve Should be called in action callback.
   * @returns {Array<object>}
              Return the mainAction and secondary actions in an array for showing doorhanger
   */
  // TODO: this is a static method so credit card doorhangers can also use this API.
  static createActions(
    mainActionParams,
    secondaryActionParams,
    resolve,
    telemetryOptions
  ) {
    function getLabelAndAccessKey(param) {
      // This should be removed once we port credit card capture doorhanger to use fluent
      if (!param.l10nId) {
        return { label: param.label, accessKey: param.accessKey };
      }

      const msg = lazy.l10n.formatMessagesSync([{ id: param.l10nId }])[0];
      return {
        label: msg.attributes.find(x => x.name == "label").value,
        accessKey: msg.attributes.find(x => x.name == "accessKey").value,
        dismiss: param.dismiss,
      };
    }

    const callback = () => {
      AutofillTelemetry.recordDoorhangerClicked(
        telemetryOptions.type,
        mainActionParams.callbackState,
        telemetryOptions.object,
        telemetryOptions.flowId
      );

      resolve({
        state: mainActionParams.callbackState,
        confirmationHintId: mainActionParams.confirmationHintId,
      });
    };

    const mainAction = {
      ...getLabelAndAccessKey(mainActionParams),
      callback,
    };

    let secondaryActions = [];
    for (const params of secondaryActionParams) {
      const cb = () => {
        AutofillTelemetry.recordDoorhangerClicked(
          telemetryOptions.type,
          params.callbackState,
          telemetryOptions.object,
          telemetryOptions.flowId
        );

        resolve({
          state: params.callbackState,
          confirmationHintId: params.confirmationHintId,
        });
      };
      secondaryActions.push({
        ...getLabelAndAccessKey(params),
        callback: cb,
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

  #editAddressCb = null;

  constructor(browser, oldRecord, newRecord, flowId, editAddressCb) {
    super(browser, oldRecord, newRecord, flowId);

    this.#editAddressCb = editAddressCb;
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
      const s = this.doc.createElement("span");
      s.textContent = text;

      if (showDiff) {
        if (style == "remove") {
          s.setAttribute("class", "address-update-text-diff-removed");
        } else if (style == "add") {
          s.setAttribute("class", "address-update-text-diff-added");
        }
      }
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
      case "name":
        data = ["given-name", "additional-name", "family-name"].map(field => [
          field,
          this.oldRecord[field],
          this.newRecord[field],
        ]);
        break;
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
        const button = this.doc.createXULElement("toolbarbutton");
        button.setAttribute("id", AddressSaveDoorhanger.editButtonId);

        // The element will be removed after the popup is closed
        /* eslint-disable mozilla/balanced-listeners */
        button.addEventListener("command", event => {
          event.stopPropagation();
          this.#editAddressCb(event);
        });
        section.appendChild(button);
      }
    }
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

    this.newRecord = Object.assign(this.newRecord, this.getRecord());

    // The layout of the address edit doorhanger should be changed when the
    // country is changed.
    this.renderContent();
  }

  renderContent() {
    this.#buildFixedAddressFields();

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

  // TODO: probably repalce this boolean flag with something smarter
  #hasBuiltFixedFields = false;
  #buildFixedAddressFields() {
    // render fixed fields
    if (!this.#hasBuiltFixedFields) {
      this.#hasBuiltFixedFields = true;
      this.#buildAddressFields(this.content, this.ui.content.fixedFields);
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

  #getFieldDisplayData(field) {
    if (field == "name") {
      return lazy.FormAutofillNameUtils.joinNameParts({
        given: this.newRecord["given-name"],
        middle: this.newRecord["additional-name"],
        family: this.newRecord["family-name"],
      });
    }
    return this.newRecord[field];
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
   * @param {string} fieldName - The name of the address field
   */
  #createInputField(fieldName) {
    const div = this.doc.createElement("div");
    div.setAttribute("class", "address-edit-input-container");

    const label = this.doc.createElement("label");
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

    input.setAttribute("id", AddressEditDoorhanger.getInputId(fieldName));

    const value = this.#getFieldDisplayData(fieldName) ?? null;
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

  static #getInputIdMatchRegexp() {
    const regex = /^address-edit-(.+)-input$/;
    return regex;
  }

  static getInputId(fieldName) {
    return `address-edit-${fieldName}-input`;
  }

  getRecord() {
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
}

CONTENT = {
  [AddressSaveDoorhanger.name]: {
    id: "address-save-update",
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
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
        confirmationHintId: "confirmation-hint-address-created",
      },
      secondaryActions: [
        {
          l10nId: "address-capture-not-now-button",
          callbackState: "cancel",
        },
      ],
    },
    options: {
      persistWhileVisible: true,
      hideClose: true,
    },
  },

  [AddressUpdateDoorhanger.name]: {
    id: "address-save-update",
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
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
        confirmationHintId: "confirmation-hint-address-updated",
      },
      secondaryActions: [
        {
          l10nId: "address-capture-not-now-button",
          callbackState: "cancel",
        },
      ],
    },
    options: {
      persistWhileVisible: true,
      hideClose: true,
    },
  },

  [AddressEditDoorhanger.name]: {
    id: "address-edit",
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
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
        confirmationHintId: "confirmation-hint-address-created",
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
      persistWhileVisible: true,
      removeOnDismissal: true,
      hideClose: true,
    },
  },

  addCreditCard: {
    notificationId: "autofill-credit-card-add",
    message: formatStringFromName("saveCreditCardMessage", [brandShortName]),
    descriptionLabel: GetStringFromName("saveCreditCardDescriptionLabel"),
    descriptionIcon: true,
    linkMessage: GetStringFromName(autofillOptsKey),
    spotlightURL: "about:preferences#privacy-credit-card-autofill",
    anchor: {
      id: "autofill-credit-card-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
    },
    mainAction: {
      label: GetStringFromName("saveCreditCardLabel"),
      accessKey: GetStringFromName("saveCreditCardAccessKey"),
      callbackState: "save",
      confimationHintId: "confirmation-hint-credit-card-created",
    },
    secondaryActions: [
      {
        label: GetStringFromName("cancelCreditCardLabel"),
        accessKey: GetStringFromName("cancelCreditCardAccessKey"),
        callbackState: "cancel",
      },
      {
        label: GetStringFromName("neverSaveCreditCardLabel"),
        accessKey: GetStringFromName("neverSaveCreditCardAccessKey"),
        callbackState: "disable",
      },
    ],
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
            ? GetStringFromName("creditCardsSyncCheckbox")
            : null;
        },
        callback(event) {
          let { secondaryButton, menubutton } =
            event.target.closest("popupnotification");
          let checked = event.target.checked;
          Services.prefs.setBoolPref(
            "services.sync.engine.creditcards",
            checked
          );
          secondaryButton.disabled = checked;
          menubutton.disabled = checked;
          lazy.log.debug("Set creditCard sync to", checked);
        },
      },
    },
  },
  updateCreditCard: {
    notificationId: "autofill-credit-card-update",
    message: GetStringFromName("updateCreditCardMessage"),
    descriptionLabel: GetStringFromName("updateCreditCardDescriptionLabel"),
    descriptionIcon: true,
    linkMessage: GetStringFromName(autofillOptsKey),
    spotlightURL: "about:preferences#privacy-credit-card-autofill",
    anchor: {
      id: "autofill-credit-card-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
    },
    mainAction: {
      label: GetStringFromName("updateCreditCardLabel"),
      accessKey: GetStringFromName("updateCreditCardAccessKey"),
      callbackState: "update",
      confirmationHintId: "confirmation-hint-credit-card-updated",
    },
    secondaryActions: [
      {
        label: GetStringFromName("createCreditCardLabel"),
        accessKey: GetStringFromName("createCreditCardAccessKey"),
        callbackState: "create",
        confirmationHintId: "confirmation-hint-credit-card-created",
      },
    ],
    options: {
      persistWhileVisible: true,
      popupIconURL: "chrome://formautofill/content/icon-credit-card.svg",
      hideClose: true,
    },
  },
};

export let FormAutofillPrompter = {
  /**
   * Append the link label element to the popupnotificationcontent.
   *
   * @param  {XULElement} content
   *         popupnotificationcontent
   * @param  {string} message
   *         The localized string for link title.
   * @param  {string} link
   *         Makes it possible to open and highlight a section in preferences
   */
  _appendPrivacyPanelLink(content, message, link) {
    let chromeDoc = content.ownerDocument;
    let privacyLinkElement = chromeDoc.createXULElement("label", {
      is: "text-link",
    });
    privacyLinkElement.setAttribute("useoriginprincipal", true);
    privacyLinkElement.setAttribute(
      "href",
      link || "about:preferences#privacy-form-autofill"
    );
    privacyLinkElement.setAttribute("value", message);
    content.appendChild(privacyLinkElement);
  },

  /**
   * Append the description section to the popupnotificationcontent.
   *
   * @param  {XULElement} content
   *         popupnotificationcontent
   * @param  {string} descriptionLabel
   *         The label showing above description.
   * @param  {string} descriptionIcon
   *         The src of description icon.
   * @param  {string} descriptionId
   *         The id of description
   */
  _appendDescription(
    content,
    descriptionLabel,
    descriptionIcon,
    descriptionId
  ) {
    let chromeDoc = content.ownerDocument;
    let docFragment = chromeDoc.createDocumentFragment();

    let descriptionLabelElement = chromeDoc.createXULElement("label");
    descriptionLabelElement.setAttribute("value", descriptionLabel);
    docFragment.appendChild(descriptionLabelElement);

    let descriptionWrapper = chromeDoc.createXULElement("hbox");
    descriptionWrapper.className = "desc-message-box";

    if (descriptionIcon) {
      let descriptionIconElement = chromeDoc.createXULElement("image");
      if (
        typeof descriptionIcon == "string" &&
        (descriptionIcon.includes("cc-logo") ||
          descriptionIcon.includes("icon-credit"))
      ) {
        descriptionIconElement.setAttribute("src", descriptionIcon);
      }
      descriptionWrapper.appendChild(descriptionIconElement);
    }

    let descriptionElement = chromeDoc.createXULElement(descriptionId);
    descriptionWrapper.appendChild(descriptionElement);
    docFragment.appendChild(descriptionWrapper);

    content.appendChild(docFragment);
  },

  _updateDescription(content, descriptionId, description) {
    let element = content.querySelector(descriptionId);
    element.textContent = description;
  },

  _getNotificationElm(browser, id) {
    let notificationId = id + "-notification";
    let chromeDoc = browser.ownerDocument;
    return chromeDoc.getElementById(notificationId);
  },

  _addCheckboxListener(browser, { notificationId, options }) {
    if (!options.checkbox) {
      return;
    }
    let { checkbox } = this._getNotificationElm(browser, notificationId);

    if (checkbox && !checkbox.hidden) {
      checkbox.addEventListener("command", options.checkbox.callback);
    }
  },

  _removeCheckboxListener(browser, { notificationId, options }) {
    if (!options.checkbox) {
      return;
    }
    let { checkbox } = this._getNotificationElm(browser, notificationId);

    if (checkbox && !checkbox.hidden) {
      checkbox.removeEventListener("command", options.checkbox.callback);
    }
  },

  async promptToSaveCreditCard(browser, storage, record, flowId) {
    // Overwrite the guid if there is a duplicate
    let doorhangerType;
    const duplicateRecord = (await storage.getDuplicateRecords(record).next())
      .value;
    if (duplicateRecord) {
      doorhangerType = "updateCreditCard";
    } else {
      doorhangerType = "addCreditCard";
    }

    const number = record["cc-number"] || record["cc-number-decrypted"];
    const name = record["cc-name"];
    const network = lazy.CreditCard.getType(number);
    const maskedNumber = lazy.CreditCard.getMaskedNumber(number);
    const description = `${maskedNumber}` + (name ? `, ${name}` : ``);
    const descriptionIcon = lazy.CreditCard.getCreditCardLogo(network);

    const state = await FormAutofillPrompter._showCreditCardCaptureDoorhanger(
      browser,
      doorhangerType,
      description,
      flowId,
      { descriptionIcon }
    );

    if (state == "cancel") {
      return;
    } else if (state == "disable") {
      Services.prefs.setBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF, false);
      return;
    }

    if (!(await FormAutofillUtils.ensureLoggedIn()).authenticated) {
      lazy.log.warn("User canceled encryption login");
      return;
    }

    this._updateStorageAfterInteractWithPrompt(
      state,
      storage,
      record,
      duplicateRecord?.guid
    );
  },

  async _updateStorageAfterInteractWithPrompt(
    state,
    storage,
    record,
    guid = null
  ) {
    let changedGUID = null;
    if (state == "create" || state == "save") {
      changedGUID = await storage.add(record);
    } else if (state == "update") {
      await storage.update(guid, record, true);
      changedGUID = guid;
    }
    storage.notifyUsed(changedGUID);
  },

  _getUpdatedCCIcon(network) {
    return FormAutofillUtils.getCreditCardLogo(network);
  },

  /**
   * Show different types of doorhanger by leveraging PopupNotifications.
   *
   * @param {XULElement} browser Target browser element for showing doorhanger.
   * @param {string} type The type of the doorhanger. There will have first time use/update/credit card.
   * @param {string} description The message that provides more information on doorhanger.
   * @param {string} flowId guid used to correlate events relating to the same form
   * @param {object} [options = {}] a list of options for this method
   * @param {string} options.descriptionIcon The icon for descriotion
   * @returns {Promise} Resolved with action type when action callback is triggered.
   */
  async _showCreditCardCaptureDoorhanger(
    browser,
    type,
    description,
    flowId,
    { descriptionIcon = null }
  ) {
    const telemetryType = AutofillTelemetry.CREDIT_CARD;
    const telemetryObject = type.startsWith("add")
      ? "capture_doorhanger"
      : "update_doorhanger";

    AutofillTelemetry.recordDoorhangerShown(
      telemetryType,
      telemetryObject,
      flowId
    );

    lazy.log.debug("show doorhanger with type:", type);
    return new Promise(resolve => {
      let {
        notificationId,
        message,
        descriptionLabel,
        linkMessage,
        spotlightURL,
        anchor,
        mainAction,
        secondaryActions,
        options,
      } = CONTENT[type];
      descriptionIcon = descriptionIcon ?? CONTENT[type].descriptionIcon;
      const { ownerGlobal: chromeWin, ownerDocument: chromeDoc } = browser;

      options.eventCallback = topic => {
        lazy.log.debug("eventCallback:", topic);

        if (topic == "removed" || topic == "dismissed") {
          this._removeCheckboxListener(browser, { notificationId, options });
          return;
        }

        // The doorhanger is customizable only when notification box is shown
        if (topic != "shown") {
          return;
        }
        this._addCheckboxListener(browser, { notificationId, options });

        const DESCRIPTION_ID = "description";
        const NOTIFICATION_ID = notificationId + "-notification";

        const notification = chromeDoc.getElementById(NOTIFICATION_ID);
        const notificationContent =
          notification.querySelector("popupnotificationcontent") ||
          chromeDoc.createXULElement("popupnotificationcontent");
        if (!notification.contains(notificationContent)) {
          notificationContent.setAttribute("orient", "vertical");

          this._appendDescription(
            notificationContent,
            descriptionLabel,
            descriptionIcon,
            DESCRIPTION_ID
          );

          this._appendPrivacyPanelLink(
            notificationContent,
            linkMessage,
            spotlightURL
          );

          notification.appendNotificationContent(notificationContent);
        }

        this._updateDescription(
          notificationContent,
          DESCRIPTION_ID,
          description
        );
      };
      AutofillDoorhanger.setAnchor(browser.ownerDocument, anchor);
      chromeWin.PopupNotifications.show(
        browser,
        notificationId,
        message,
        anchor.id,
        ...AutofillDoorhanger.createActions(
          mainAction,
          secondaryActions,
          resolve,
          { type: telemetryType, object: telemetryObject, flowId }
        ),
        options
      );
    }).then(({ state, confirmationHintId }) => {
      if (confirmationHintId) {
        showConfirmation(browser, confirmationHintId);
      }
      return state;
    });
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
    const { state, recordToSave } =
      await FormAutofillPrompter._showAddressCaptureDoorhanger(
        browser,
        flowId,
        { oldRecord, newRecord }
      );

    if (state == "cancel") {
      return;
    } else if (state == "open-pref") {
      browser.ownerGlobal.openPreferences("privacy-address-autofill");
      return;
    } else if (state == "never-safe") {
      // TODO
      return;
    }

    this._updateStorageAfterInteractWithPrompt(
      state,
      storage,
      recordToSave,
      oldRecord?.guid
    );
  },

  /**
   * Show save address doorhanger or update address doorhanger.
   *
   * @param {XULElement} browser Target browser element for showing doorhanger.
   * @param {string} flowId guid used to correlate events relating to the same form
   * @param {object} [options = {}] a list of options for this method
   * @param {object} options.oldRecord the saved address record that can be merged with
   *                                   the new address record
   * @param {object} options.newRecord The new address record users just submitted
   * @returns {Promise} Resolved with action type when action callback is triggered.
   */
  async _showAddressCaptureDoorhanger(
    browser,
    flowId,
    { oldRecord, newRecord }
  ) {
    // If the previous doorhanger is there and then another form is submitted again
    // do nothing.
    if (this._addrSaveDoorhanger || this._addrEditDoorhanger) {
      return { state: "cancel" };
    }

    const createNewRecord = !Object.keys(oldRecord).length;

    lazy.log.debug(
      `show address ${createNewRecord ? "save" : "update"} doorhanger`
    );
    const { ownerGlobal: chromeWin } = browser;
    await chromeWin.ensureCustomElements("moz-support-link");
    chromeWin.MozXULElement.insertFTLIfNeeded(
      "browser/preferences/formAutofill.ftl"
    );

    let recordToSave = newRecord;
    return new Promise(resolve => {
      let doorhanger;
      const editAddressCb = async event => {
        const { state, editedRecord } = await this._showAddressEditDoorhanger(
          browser,
          flowId,
          newRecord
        );

        if (state == "save") {
          // If users choose "save" in the edit address doorhanger, we don't need
          // to show the save/update doorhanger after the edit address doorhanger
          // is closed
          recordToSave = editedRecord;
          chromeWin.PopupNotifications.remove(this._addrSaveDoorhanger);
          resolve({
            state: doorhanger.ui.footer.mainAction.callbackState,
            confirmationHintId:
              doorhanger.ui.footer.mainAction.confirmationHintId,
          });
        }
      };

      doorhanger = createNewRecord
        ? new AddressSaveDoorhanger(
            browser,
            oldRecord,
            newRecord,
            flowId,
            editAddressCb
          )
        : new AddressUpdateDoorhanger(
            browser,
            oldRecord,
            newRecord,
            flowId,
            editAddressCb
          );

      this._addrSaveDoorhanger = doorhanger.show(resolve, state => {
        if (state == "removed") {
          this._addrSaveDoorhanger = null;
        }
      });
    }).then(({ state, confirmationHintId }) => {
      if (confirmationHintId) {
        showConfirmation(browser, confirmationHintId);
      }
      return { state, recordToSave };
    });
  },

  /*
   * Show the edit address doorhanger
   *
   * @param {XULElement} browser Target browser element for showing doorhanger.
   * @param {string} flowId guid used to correlate events relating to the same form
   * @param {object} record the address record to be filled when displaying the doorhanger
   * @returns {Promise} Resolved with action type when action callback is triggered.
   */
  async _showAddressEditDoorhanger(browser, flowId, record) {
    const { ownerGlobal: chromeWin } = browser;

    let editedRecord;
    return new Promise(resolve => {
      // Hide the save/update doorhanger. Since edit address doorhanger uses
      // the same anchor as the save/update doorhanger, we have to set `neverShow`
      // to true so we don't show save address doorhanger after calling
      // PopupNotifications.show
      this._addrSaveDoorhanger.options.neverShow = true;

      const doorhanger = new AddressEditDoorhanger(browser, record, flowId);
      this._addrEditDoorhanger = doorhanger.show(resolve, state => {
        if (state == "showing") {
          chromeWin.PopupNotifications.suppressWhileOpen(doorhanger.panel);
        } else if (["dismissed", "removed"].includes(state)) {
          editedRecord = doorhanger.getRecord();
          this._addrEditDoorhanger = null;
          this._addrSaveDoorhanger.options.neverShow = false;
        }
      });
    }).then(({ state, confirmationHintId }) => {
      return { state, editedRecord };
    });
  },
};
