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

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () =>
  FormAutofill.defineLogGetter(lazy, "FormAutofillPrompter")
);

const { ENABLED_AUTOFILL_CREDITCARDS_PREF } = FormAutofill;

const GetStringFromName = FormAutofillUtils.stringBundle.GetStringFromName;
const formatStringFromName =
  FormAutofillUtils.stringBundle.formatStringFromName;
const brandShortName =
  FormAutofillUtils.brandBundle.GetStringFromName("brandShortName");
let changeAutofillOptsKey = "changeAutofillOptions";
let autofillOptsKey = "autofillOptionsLink";
if (AppConstants.platform == "macosx") {
  changeAutofillOptsKey += "OSX";
  autofillOptsKey += "OSX";
}

const CONTENT = {
  addFirstTimeUse: {
    notificationId: "autofill-address",
    message: formatStringFromName("saveAddressesMessage", [brandShortName]),
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
    },
    mainAction: {
      label: GetStringFromName(changeAutofillOptsKey),
      accessKey: GetStringFromName("changeAutofillOptionsAccessKey"),
      callbackState: "open-pref",
    },
    options: {
      persistWhileVisible: true,
      popupIconURL: "chrome://formautofill/content/icon-address-save.svg",
      checkbox: {
        get checked() {
          return Services.prefs.getBoolPref("services.sync.engine.addresses");
        },
        get label() {
          // If sync account is not set, return null label to hide checkbox
          return Services.prefs.prefHasUserValue("services.sync.username")
            ? GetStringFromName("addressesSyncCheckbox")
            : null;
        },
        callback(event) {
          let checked = event.target.checked;
          Services.prefs.setBoolPref("services.sync.engine.addresses", checked);
          lazy.log.debug("Set addresses sync to", checked);
        },
      },
      hideClose: true,
    },
  },
  addAddress: {
    notificationId: "autofill-address",
    message: formatStringFromName("saveAddressesMessage", [brandShortName]),
    descriptionLabel: GetStringFromName("saveAddressDescriptionLabel"),
    descriptionIcon: true,
    linkMessage: GetStringFromName(autofillOptsKey),
    spotlightURL: "about:preferences#privacy-address-autofill",
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
    },
    mainAction: {
      label: GetStringFromName("saveAddressLabel"),
      accessKey: GetStringFromName("saveAddressAccessKey"),
      callbackState: "create",
    },
    secondaryActions: [
      {
        label: GetStringFromName("cancelAddressLabel"),
        accessKey: GetStringFromName("cancelAddressAccessKey"),
        callbackState: "cancel",
      },
    ],
    options: {
      persistWhileVisible: true,
      popupIconURL: "chrome://formautofill/content/icon-address-update.svg",
      hideClose: true,
    },
  },
  updateAddress: {
    notificationId: "autofill-address",
    message: GetStringFromName("updateAddressMessage"),
    descriptionLabel: GetStringFromName("updateAddressNewDescriptionLabel"),
    additionalDescriptionLabel: GetStringFromName(
      "updateAddressOldDescriptionLabel"
    ),
    descriptionIcon: false,
    linkMessage: GetStringFromName(autofillOptsKey),
    spotlightURL: "about:preferences#privacy-address-autofill",
    anchor: {
      id: "autofill-address-notification-icon",
      URL: "chrome://formautofill/content/formfill-anchor.svg",
      tooltiptext: GetStringFromName("openAutofillMessagePanel"),
    },
    mainAction: {
      label: GetStringFromName("updateAddressLabel"),
      accessKey: GetStringFromName("updateAddressAccessKey"),
      callbackState: "update",
    },
    secondaryActions: [
      {
        label: GetStringFromName("createAddressLabel"),
        accessKey: GetStringFromName("createAddressAccessKey"),
        callbackState: "create",
      },
    ],
    options: {
      persistWhileVisible: true,
      popupIconURL: "chrome://formautofill/content/icon-address-update.svg",
      hideClose: true,
    },
  },
  addCreditCard: {
    notificationId: "autofill-credit-card",
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
    notificationId: "autofill-credit-card",
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
    },
    secondaryActions: [
      {
        label: GetStringFromName("createCreditCardLabel"),
        accessKey: GetStringFromName("createCreditCardAccessKey"),
        callbackState: "create",
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
  _createActions(mainActionParams, secondaryActionParams, resolve) {
    if (!mainActionParams) {
      return [null, null];
    }

    let { label, accessKey, callbackState } = mainActionParams;
    let callback = resolve.bind(null, callbackState);
    let mainAction = { label, accessKey, callback };

    if (!secondaryActionParams) {
      return [mainAction, null];
    }

    let secondaryActions = [];
    for (let params of secondaryActionParams) {
      let cb = resolve.bind(null, params.callbackState);
      secondaryActions.push({
        label: params.label,
        accessKey: params.accessKey,
        callback: cb,
      });
    }

    return [mainAction, secondaryActions];
  },
  _getNotificationElm(browser, id) {
    let notificationId = id + "-notification";
    let chromeDoc = browser.ownerDocument;
    return chromeDoc.getElementById(notificationId);
  },
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

  /**
   * Create an image element for notification anchor if it doesn't already exist.
   *
   * @param  {XULElement} browser
   *         Target browser element for showing doorhanger.
   * @param  {object} anchor
   *         Anchor options for setting the anchor element.
   * @param  {string} anchor.id
   *         ID of the anchor element.
   * @param  {string} anchor.URL
   *         Path of the icon asset.
   * @param  {string} anchor.tooltiptext
   *         Tooltip string for the anchor.
   */
  _setAnchor(browser, anchor) {
    let chromeDoc = browser.ownerDocument;
    let { id, URL, tooltiptext } = anchor;
    let anchorEt = chromeDoc.getElementById(id);
    if (!anchorEt) {
      let notificationPopupBox = chromeDoc.getElementById(
        "notification-popup-box"
      );
      // Icon shown on URL bar
      let anchorElement = chromeDoc.createXULElement("image");
      anchorElement.id = id;
      anchorElement.setAttribute("src", URL);
      anchorElement.classList.add("notification-anchor-icon");
      anchorElement.setAttribute("role", "button");
      anchorElement.setAttribute("tooltiptext", tooltiptext);
      notificationPopupBox.appendChild(anchorElement);
    }
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

  /**
   * Show save or update address doorhanger
   *
   * @param {Element<browser>} browser  Browser to show the save/update address prompt
   * @param {object} storage Address storage
   * @param {object} newRecord Address record to save
   * @param {string} flowId Unique GUID to record a series of the same user action
   * @param {object} options
   * @param {object} [options.mergeableRecord] Record to be merged
   * @param {Array}  [options.mergeableFields] List of field name that can be merged
   */
  async promptToSaveAddress(
    browser,
    storage,
    newRecord,
    flowId,
    { mergeableRecord, mergeableFields }
  ) {
    // Overwrite the guid if there is a duplicate
    let doorhangerType;
    if (mergeableRecord) {
      doorhangerType = "updateAddress";
    } else if (FormAutofill.isAutofillAddressesCaptureV2Enabled) {
      doorhangerType = "addAddress";
    } else {
      doorhangerType = "addFirstTimeUse";
      this._updateStorageAfterInteractWithPrompt("save", storage, newRecord);

      // Show first time use doorhanger
      if (FormAutofill.isAutofillAddressesFirstTimeUse) {
        Services.prefs.setBoolPref(
          FormAutofill.ADDRESSES_FIRST_TIME_USE_PREF,
          false
        );
      } else {
        return;
      }
    }

    const description = FormAutofillUtils.getAddressLabel(newRecord);
    const additionalDescription = mergeableRecord
      ? FormAutofillUtils.getAddressLabel(mergeableRecord)
      : null;

    const state = await FormAutofillPrompter._showCCorAddressCaptureDoorhanger(
      browser,
      doorhangerType,
      description,
      flowId,
      { additionalDescription }
    );

    if (state == "cancel") {
      return;
    } else if (state == "open-pref") {
      browser.ownerGlobal.openPreferences("privacy-address-autofill");
      return;
    }

    this._updateStorageAfterInteractWithPrompt(
      state,
      storage,
      newRecord,
      mergeableRecord?.guid
    );
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

    const state = await FormAutofillPrompter._showCCorAddressCaptureDoorhanger(
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
   * @param {string} options.additionalDescription The message that provides more information on doorhanger.
   * @returns {Promise} Resolved with action type when action callback is triggered.
   */
  async _showCCorAddressCaptureDoorhanger(
    browser,
    type,
    description,
    flowId,
    { descriptionIcon = null, additionalDescription = null }
  ) {
    const telemetryType = type.endsWith("CreditCard")
      ? AutofillTelemetry.CREDIT_CARD
      : AutofillTelemetry.ADDRESS;
    const isCapture = type.startsWith("add");

    AutofillTelemetry.recordDoorhangerShown(telemetryType, flowId, isCapture);

    lazy.log.debug("show doorhanger with type:", type);
    return new Promise(resolve => {
      let {
        notificationId,
        message,
        descriptionLabel,
        additionalDescriptionLabel,
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

        // There's no preferences link or other customization in first time use doorhanger.
        if (type == "addFirstTimeUse") {
          return;
        }

        const DESCRIPTION_ID = "description";
        const ADDITIONAL_DESCRIPTION_ID = "additional-description";
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

          if (additionalDescription) {
            this._appendDescription(
              notificationContent,
              additionalDescriptionLabel,
              descriptionIcon,
              ADDITIONAL_DESCRIPTION_ID
            );
          }

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
        if (additionalDescription) {
          this._updateDescription(
            notificationContent,
            ADDITIONAL_DESCRIPTION_ID,
            additionalDescription
          );
        }
      };
      this._setAnchor(browser, anchor);
      chromeWin.PopupNotifications.show(
        browser,
        notificationId,
        message,
        anchor.id,
        ...this._createActions(mainAction, secondaryActions, resolve),
        options
      );
    }).then(state => {
      AutofillTelemetry.recordDoorhangerClicked(
        telemetryType,
        state,
        flowId,
        isCapture
      );
      return state;
    });
  },
};
