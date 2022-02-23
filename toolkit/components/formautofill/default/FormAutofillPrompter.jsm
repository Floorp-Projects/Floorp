/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements doorhanger singleton that wraps up the PopupNotifications and handles
 * the doorhager UI for formautofill related features.
 */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofillPrompter"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { FormAutofill } = ChromeUtils.import(
  "resource://autofill/FormAutofill.jsm"
);
const { FormAutofillUtils } = ChromeUtils.import(
  "resource://autofill/FormAutofillUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  CreditCard: "resource://gre/modules/CreditCard.jsm",
});

this.log = null;
FormAutofill.defineLazyLogGetter(this, EXPORTED_SYMBOLS[0]);

const GetStringFromName = FormAutofillUtils.stringBundle.GetStringFromName;
const formatStringFromName =
  FormAutofillUtils.stringBundle.formatStringFromName;
const brandShortName = FormAutofillUtils.brandBundle.GetStringFromName(
  "brandShortName"
);
let changeAutofillOptsKey = "changeAutofillOptions";
let autofillOptsKey = "autofillOptionsLink";
if (AppConstants.platform == "macosx") {
  changeAutofillOptsKey += "OSX";
  autofillOptsKey += "OSX";
}

const CONTENT = {
  firstTimeUse: {
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
          log.debug("Set addresses sync to", checked);
        },
      },
      hideClose: true,
    },
  },
  updateAddress: {
    notificationId: "autofill-address",
    message: GetStringFromName("updateAddressMessage"),
    descriptionLabel: GetStringFromName("updateAddressDescriptionLabel"),
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
          let { secondaryButton, menubutton } = event.target.closest(
            "popupnotification"
          );
          let checked = event.target.checked;
          Services.prefs.setBoolPref(
            "services.sync.engine.creditcards",
            checked
          );
          secondaryButton.disabled = checked;
          menubutton.disabled = checked;
          log.debug("Set creditCard sync to", checked);
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

let FormAutofillPrompter = {
  /**
   * Generate the main action and secondary actions from content parameters and
   * promise resolve.
   *
   * @private
   * @param  {Object} mainActionParams
   *         Parameters for main action.
   * @param  {Array<Object>} secondaryActionParams
   *         Array of the parameters for secondary actions.
   * @param  {Function} resolve Should be called in action callback.
   * @returns {Array<Object>}
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
   * @param  {XULElement} content
   *         popupnotificationcontent
   * @param  {string} descriptionLabel
   *         The label showing above description.
   * @param  {string} descriptionIcon
   *         The src of description icon.
   */
  _appendDescription(content, descriptionLabel, descriptionIcon) {
    let chromeDoc = content.ownerDocument;
    let docFragment = chromeDoc.createDocumentFragment();

    let descriptionLabelElement = chromeDoc.createXULElement("label");
    descriptionLabelElement.setAttribute("value", descriptionLabel);
    docFragment.appendChild(descriptionLabelElement);

    let descriptionWrapper = chromeDoc.createXULElement("hbox");
    descriptionWrapper.className = "desc-message-box";

    if (descriptionIcon) {
      let descriptionIconElement = chromeDoc.createXULElement("image");
      descriptionWrapper.appendChild(descriptionIconElement);
    }

    let descriptionElement = chromeDoc.createXULElement("description");
    descriptionWrapper.appendChild(descriptionElement);
    docFragment.appendChild(descriptionWrapper);

    content.appendChild(docFragment);
  },

  _updateDescription(content, description) {
    content.querySelector("description").textContent = description;
  },

  /**
   * Create an image element for notification anchor if it doesn't already exist.
   * @param  {XULElement} browser
   *         Target browser element for showing doorhanger.
   * @param  {Object} anchor
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

  async promptToSaveAddress(browser, type, description) {
    return this._showCCorAddressCaptureDoorhanger(browser, type, description);
  },

  async promptToSaveCreditCard(browser, creditCard, storage) {
    let number =
      creditCard.record["cc-number"] ||
      creditCard.record["cc-number-decrypted"];
    let name = creditCard.record["cc-name"];
    const description = await CreditCard.getLabel({ name, number });

    const telemetryObject = creditCard.guid
      ? "update_doorhanger"
      : "capture_doorhanger";
    Services.telemetry.recordEvent(
      "creditcard",
      "show",
      telemetryObject,
      creditCard.flowId
    );

    const state = await FormAutofillPrompter._showCCorAddressCaptureDoorhanger(
      browser,
      creditCard.guid ? "updateCreditCard" : "addCreditCard",
      description
    );

    if (state == "cancel") {
      Services.telemetry.recordEvent(
        "creditcard",
        "cancel",
        telemetryObject,
        creditCard.flowId
      );
      return;
    }

    if (state == "disable") {
      Services.prefs.setBoolPref(
        "extensions.formautofill.creditCards.enabled",
        false
      );
      Services.telemetry.recordEvent(
        "creditcard",
        "disable",
        telemetryObject,
        creditCard.flowId
      );
      return;
    }

    if (!(await FormAutofillUtils.ensureLoggedIn()).authenticated) {
      log.warn("User canceled encryption login");
      return;
    }

    let changedGUIDs = [];
    if (creditCard.guid) {
      if (state == "update") {
        Services.telemetry.recordEvent(
          "creditcard",
          "update",
          telemetryObject,
          creditCard.flowId
        );
        await storage.creditCards.update(
          creditCard.guid,
          creditCard.record,
          true
        );
        changedGUIDs.push(creditCard.guid);
      } else if ("create") {
        Services.telemetry.recordEvent(
          "creditcard",
          "save",
          telemetryObject,
          creditCard.flowId
        );
        changedGUIDs.push(await storage.creditCards.add(creditCard.record));
      }
    } else {
      changedGUIDs.push(
        ...(await storage.creditCards.mergeToStorage(creditCard.record))
      );
      if (!changedGUIDs.length) {
        Services.telemetry.recordEvent(
          "creditcard",
          "save",
          telemetryObject,
          creditCard.flowId
        );
        changedGUIDs.push(await storage.creditCards.add(creditCard.record));
      }
    }
    changedGUIDs.forEach(guid => storage.creditCards.notifyUsed(guid));
  },

  /**
   * Show different types of doorhanger by leveraging PopupNotifications.
   * @param  {XULElement} browser
   *         Target browser element for showing doorhanger.
   * @param  {string} type
   *         The type of the doorhanger. There will have first time use/update/credit card.
   * @param  {string} description
   *         The message that provides more information on doorhanger.
   * @returns {Promise}
              Resolved with action type when action callback is triggered.
   */
  async _showCCorAddressCaptureDoorhanger(browser, type, description) {
    log.debug("show doorhanger with type:", type);
    return new Promise(resolve => {
      let {
        notificationId,
        message,
        descriptionLabel,
        descriptionIcon,
        linkMessage,
        spotlightURL,
        anchor,
        mainAction,
        secondaryActions,
        options,
      } = CONTENT[type];

      const { ownerGlobal: chromeWin, ownerDocument: chromeDoc } = browser;
      options.eventCallback = topic => {
        log.debug("eventCallback:", topic);

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
        if (type == "firstTimeUse") {
          return;
        }

        const notificationElementId = notificationId + "-notification";
        const notification = chromeDoc.getElementById(notificationElementId);
        const notificationContent =
          notification.querySelector("popupnotificationcontent") ||
          chromeDoc.createXULElement("popupnotificationcontent");
        if (!notification.contains(notificationContent)) {
          notificationContent.setAttribute("orient", "vertical");
          this._appendDescription(
            notificationContent,
            descriptionLabel,
            descriptionIcon
          );
          this._appendPrivacyPanelLink(
            notificationContent,
            linkMessage,
            spotlightURL
          );
          notification.appendNotificationContent(notificationContent);
        }
        this._updateDescription(notificationContent, description);
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
    });
  },
};
