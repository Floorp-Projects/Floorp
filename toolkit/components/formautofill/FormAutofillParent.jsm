/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements a service used to access storage and communicate with content.
 *
 * A "fields" array is used to communicate with FormAutofillContent. Each item
 * represents a single input field in the content page as well as its
 * @autocomplete properties. The schema is as below. Please refer to
 * FormAutofillContent.js for more details.
 *
 * [
 *   {
 *     section,
 *     addressType,
 *     contactType,
 *     fieldName,
 *     value,
 *     index
 *   },
 *   {
 *     // ...
 *   }
 * ]
 */

"use strict";

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
var EXPORTED_SYMBOLS = ["FormAutofillParent", "FormAutofillStatus"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { FormAutofill } = ChromeUtils.import(
  "resource://autofill/FormAutofill.jsm"
);
const { FormAutofillUtils } = ChromeUtils.import(
  "resource://autofill/FormAutofillUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  CreditCard: "resource://gre/modules/CreditCard.jsm",
  FormAutofillPreferences: "resource://autofill/FormAutofillPreferences.jsm",
  FormAutofillPrompter: "resource://autofill/FormAutofillPrompter.jsm",
  OSKeyStore: "resource://gre/modules/OSKeyStore.jsm",
});

FormAutofill.defineLazyLogGetter(lazy, EXPORTED_SYMBOLS[0]);

const {
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,
} = FormAutofill;

const {
  ADDRESSES_COLLECTION_NAME,
  CREDITCARDS_COLLECTION_NAME,
} = FormAutofillUtils;

let gMessageObservers = new Set();

let FormAutofillStatus = {
  _initialized: false,

  /**
   * Cache of the Form Autofill status (considering preferences and storage).
   */
  _active: null,

  /**
   * Initializes observers and registers the message handler.
   */
  init() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    Services.obs.addObserver(this, "privacy-pane-loaded");

    // Observing the pref and storage changes
    Services.prefs.addObserver(ENABLED_AUTOFILL_ADDRESSES_PREF, this);
    Services.obs.addObserver(this, "formautofill-storage-changed");

    // Only listen to credit card related preference if it is available
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      Services.prefs.addObserver(ENABLED_AUTOFILL_CREDITCARDS_PREF, this);
    }

    // We have to use empty window type to get all opened windows here because the
    // window type parameter may not be available during startup.
    for (let win of Services.wm.getEnumerator("")) {
      let { documentElement } = win.document;
      if (documentElement?.getAttribute("windowtype") == "navigator:browser") {
        this.injectElements(win.document);
      } else {
        // Manually call onOpenWindow for windows that are already opened but not
        // yet have the window type set. This ensures we inject the elements we need
        // when its docuemnt is ready.
        this.onOpenWindow(win);
      }
    }
    Services.wm.addListener(this);

    Services.telemetry.setEventRecordingEnabled("creditcard", true);
  },

  /**
   * Uninitializes FormAutofillStatus. This is for testing only.
   *
   * @private
   */
  uninit() {
    lazy.gFormAutofillStorage._saveImmediately();

    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    this._active = null;

    Services.obs.removeObserver(this, "privacy-pane-loaded");
    Services.prefs.removeObserver(ENABLED_AUTOFILL_ADDRESSES_PREF, this);
    Services.wm.removeListener(this);

    if (FormAutofill.isAutofillCreditCardsAvailable) {
      Services.prefs.removeObserver(ENABLED_AUTOFILL_CREDITCARDS_PREF, this);
    }
  },

  get formAutofillStorage() {
    return lazy.gFormAutofillStorage;
  },

  /**
   * Broadcast the status to frames when the form autofill status changes.
   */
  onStatusChanged() {
    lazy.log.debug("onStatusChanged: Status changed to", this._active);
    Services.ppmm.sharedData.set("FormAutofill:enabled", this._active);
    // Sync autofill enabled to make sure the value is up-to-date
    // no matter when the new content process is initialized.
    Services.ppmm.sharedData.flush();
  },

  /**
   * Query preference and storage status to determine the overall status of the
   * form autofill feature.
   *
   * @returns {boolean} whether form autofill is active (enabled and has data)
   */
  computeStatus() {
    const savedFieldNames = Services.ppmm.sharedData.get(
      "FormAutofill:savedFieldNames"
    );

    return (
      (Services.prefs.getBoolPref(ENABLED_AUTOFILL_ADDRESSES_PREF) ||
        Services.prefs.getBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF)) &&
      savedFieldNames &&
      savedFieldNames.size > 0
    );
  },

  /**
   * Update the status and trigger onStatusChanged, if necessary.
   */
  updateStatus() {
    lazy.log.debug("updateStatus");
    let wasActive = this._active;
    this._active = this.computeStatus();
    if (this._active !== wasActive) {
      this.onStatusChanged();
    }
  },

  async updateSavedFieldNames() {
    lazy.log.debug("updateSavedFieldNames");

    let savedFieldNames;
    const addressNames = await lazy.gFormAutofillStorage.addresses.getSavedFieldNames();

    // Don't access the credit cards store unless it is enabled.
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      const creditCardNames = await lazy.gFormAutofillStorage.creditCards.getSavedFieldNames();
      savedFieldNames = new Set([...addressNames, ...creditCardNames]);
    } else {
      savedFieldNames = addressNames;
    }

    Services.ppmm.sharedData.set(
      "FormAutofill:savedFieldNames",
      savedFieldNames
    );
    Services.ppmm.sharedData.flush();

    this.updateStatus();
  },

  injectElements(doc) {
    Services.scriptloader.loadSubScript(
      "chrome://formautofill/content/customElements.js",
      doc.ownerGlobal
    );
  },

  onOpenWindow(xulWindow) {
    const win = xulWindow.docShell.domWindow;
    win.addEventListener(
      "load",
      () => {
        if (
          win.document.documentElement.getAttribute("windowtype") ==
          "navigator:browser"
        ) {
          this.injectElements(win.document);
        }
      },
      { once: true }
    );
  },

  onCloseWindow() {},

  async observe(subject, topic, data) {
    lazy.log.debug("observe:", topic, "with data:", data);
    switch (topic) {
      case "privacy-pane-loaded": {
        let formAutofillPreferences = new lazy.FormAutofillPreferences();
        let document = subject.document;
        let prefFragment = formAutofillPreferences.init(document);
        let formAutofillGroupBox = document.getElementById(
          "formAutofillGroupBox"
        );
        formAutofillGroupBox.appendChild(prefFragment);
        break;
      }

      case "nsPref:changed": {
        // Observe pref changes and update _active cache if status is changed.
        this.updateStatus();
        break;
      }

      case "formautofill-storage-changed": {
        // Early exit if only metadata is changed
        if (data == "notifyUsed") {
          break;
        }

        await this.updateSavedFieldNames();
        break;
      }

      default: {
        throw new Error(
          `FormAutofillStatus: Unexpected topic observed: ${topic}`
        );
      }
    }
  },
};

// Lazily load the storage JSM to avoid disk I/O until absolutely needed.
// Once storage is loaded we need to update saved field names and inform content processes.
XPCOMUtils.defineLazyGetter(lazy, "gFormAutofillStorage", () => {
  let { formAutofillStorage } = ChromeUtils.import(
    "resource://autofill/FormAutofillStorage.jsm"
  );
  lazy.log.debug("Loading formAutofillStorage");

  formAutofillStorage.initialize().then(() => {
    // Update the saved field names to compute the status and update child processes.
    FormAutofillStatus.updateSavedFieldNames();
  });

  return formAutofillStorage;
});

class FormAutofillParent extends JSWindowActorParent {
  constructor() {
    super();
    FormAutofillStatus.init();
  }

  static addMessageObserver(observer) {
    gMessageObservers.add(observer);
  }

  static removeMessageObserver(observer) {
    gMessageObservers.delete(observer);
  }

  /**
   * Handles the message coming from FormAutofillContent.
   *
   * @param   {string} message.name The name of the message.
   * @param   {object} message.data The data of the message.
   */
  async receiveMessage({ name, data }) {
    switch (name) {
      case "FormAutofill:InitStorage": {
        await lazy.gFormAutofillStorage.initialize();
        await FormAutofillStatus.updateSavedFieldNames();
        break;
      }
      case "FormAutofill:GetRecords": {
        return FormAutofillParent._getRecords(data);
      }
      case "FormAutofill:OnFormSubmit": {
        this.notifyMessageObservers("onFormSubmitted", data);
        await this._onFormSubmit(data);
        break;
      }
      case "FormAutofill:OpenPreferences": {
        const win = lazy.BrowserWindowTracker.getTopWindow();
        win.openPreferences("privacy-form-autofill");
        break;
      }
      case "FormAutofill:GetDecryptedString": {
        let { cipherText, reauth } = data;
        if (!FormAutofillUtils._reauthEnabledByUser) {
          lazy.log.debug("Reauth is disabled");
          reauth = false;
        }
        let string;
        try {
          string = await lazy.OSKeyStore.decrypt(cipherText, reauth);
        } catch (e) {
          if (e.result != Cr.NS_ERROR_ABORT) {
            throw e;
          }
          lazy.log.warn("User canceled encryption login");
        }
        return string;
      }
      case "FormAutofill:UpdateWarningMessage":
        this.notifyMessageObservers("updateWarningNote", data);
        break;

      case "FormAutofill:FieldsIdentified":
        this.notifyMessageObservers("fieldsIdentified", data);
        break;

      // The remaining Save and Remove messages are invoked only by tests.
      case "FormAutofill:SaveAddress": {
        if (data.guid) {
          await lazy.gFormAutofillStorage.addresses.update(
            data.guid,
            data.address
          );
        } else {
          await lazy.gFormAutofillStorage.addresses.add(data.address);
        }
        break;
      }
      case "FormAutofill:SaveCreditCard": {
        if (!(await FormAutofillUtils.ensureLoggedIn()).authenticated) {
          lazy.log.warn("User canceled encryption login");
          return undefined;
        }
        await lazy.gFormAutofillStorage.creditCards.add(data.creditcard);
        break;
      }
      case "FormAutofill:RemoveAddresses": {
        data.guids.forEach(guid =>
          lazy.gFormAutofillStorage.addresses.remove(guid)
        );
        break;
      }
      case "FormAutofill:RemoveCreditCards": {
        data.guids.forEach(guid =>
          lazy.gFormAutofillStorage.creditCards.remove(guid)
        );
        break;
      }
    }

    return undefined;
  }

  notifyMessageObservers(callbackName, data) {
    for (let observer of gMessageObservers) {
      try {
        if (callbackName in observer) {
          observer[callbackName](
            data,
            this.manager.browsingContext.topChromeWindow
          );
        }
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  }

  /**
   * Get the records from profile store and return results back to content
   * process. It will decrypt the credit card number and append
   * "cc-number-decrypted" to each record if OSKeyStore isn't set.
   *
   * This is static as a unit test calls this.
   *
   * @private
   * @param  {string} data.collectionName
   *         The name used to specify which collection to retrieve records.
   * @param  {string} data.searchString
   *         The typed string for filtering out the matched records.
   * @param  {string} data.info
   *         The input autocomplete property's information.
   */
  static async _getRecords({ collectionName, searchString, info }) {
    let collection = lazy.gFormAutofillStorage[collectionName];
    if (!collection) {
      return [];
    }

    let recordsInCollection = await collection.getAll();
    if (!info || !info.fieldName || !recordsInCollection.length) {
      return recordsInCollection;
    }

    let isCC = collectionName == CREDITCARDS_COLLECTION_NAME;
    // We don't filter "cc-number"
    if (isCC && info.fieldName == "cc-number") {
      recordsInCollection = recordsInCollection.filter(
        record => !!record["cc-number"]
      );
      return recordsInCollection;
    }

    let records = [];
    let lcSearchString = searchString.toLowerCase();

    for (let record of recordsInCollection) {
      let fieldValue = record[info.fieldName];
      if (!fieldValue) {
        continue;
      }

      if (
        collectionName == ADDRESSES_COLLECTION_NAME &&
        record.country &&
        !FormAutofill.isAutofillAddressesAvailableInCountry(record.country)
      ) {
        // Address autofill isn't supported for the record's country so we don't
        // want to attempt to potentially incorrectly fill the address fields.
        continue;
      }

      if (
        lcSearchString &&
        !String(fieldValue)
          .toLowerCase()
          .startsWith(lcSearchString)
      ) {
        continue;
      }
      records.push(record);
    }

    return records;
  }

  async _onAddressSubmit(address, browser, timeStartedFillingMS) {
    let showDoorhanger = null;
    if (!FormAutofill.isAutofillAddressesCaptureEnabled) {
      return showDoorhanger;
    }
    if (address.guid) {
      // Avoid updating the fields that users don't modify.
      let originalAddress = await lazy.gFormAutofillStorage.addresses.get(
        address.guid
      );
      for (let field in address.record) {
        if (address.untouchedFields.includes(field) && originalAddress[field]) {
          address.record[field] = originalAddress[field];
        }
      }

      if (
        !(await lazy.gFormAutofillStorage.addresses.mergeIfPossible(
          address.guid,
          address.record,
          true
        ))
      ) {
        this._recordFormFillingTime(
          "address",
          "autofill-update",
          timeStartedFillingMS
        );

        showDoorhanger = async () => {
          const description = FormAutofillUtils.getAddressLabel(address.record);
          const state = await lazy.FormAutofillPrompter.promptToSaveAddress(
            browser,
            "updateAddress",
            description
          );
          let changedGUIDs = await lazy.gFormAutofillStorage.addresses.mergeToStorage(
            address.record,
            true
          );
          switch (state) {
            case "create":
              if (!changedGUIDs.length) {
                changedGUIDs.push(
                  await lazy.gFormAutofillStorage.addresses.add(address.record)
                );
              }
              break;
            case "update":
              if (!changedGUIDs.length) {
                await lazy.gFormAutofillStorage.addresses.update(
                  address.guid,
                  address.record,
                  true
                );
                changedGUIDs.push(address.guid);
              } else {
                lazy.gFormAutofillStorage.addresses.remove(address.guid);
              }
              break;
          }
          changedGUIDs.forEach(guid =>
            lazy.gFormAutofillStorage.addresses.notifyUsed(guid)
          );
        };
        // Address should be updated
        Services.telemetry.scalarAdd(
          "formautofill.addresses.fill_type_autofill_update",
          1
        );
      } else {
        this._recordFormFillingTime(
          "address",
          "autofill",
          timeStartedFillingMS
        );
        lazy.gFormAutofillStorage.addresses.notifyUsed(address.guid);
        // Address is merged successfully
        Services.telemetry.scalarAdd(
          "formautofill.addresses.fill_type_autofill",
          1
        );
      }
    } else {
      let changedGUIDs = await lazy.gFormAutofillStorage.addresses.mergeToStorage(
        address.record
      );
      if (!changedGUIDs.length) {
        changedGUIDs.push(
          await lazy.gFormAutofillStorage.addresses.add(address.record)
        );
      }
      changedGUIDs.forEach(guid =>
        lazy.gFormAutofillStorage.addresses.notifyUsed(guid)
      );
      this._recordFormFillingTime("address", "manual", timeStartedFillingMS);

      // Show first time use doorhanger
      if (FormAutofill.isAutofillAddressesFirstTimeUse) {
        Services.prefs.setBoolPref(
          FormAutofill.ADDRESSES_FIRST_TIME_USE_PREF,
          false
        );
        showDoorhanger = async () => {
          const description = FormAutofillUtils.getAddressLabel(address.record);
          const state = await lazy.FormAutofillPrompter.promptToSaveAddress(
            browser,
            "firstTimeUse",
            description
          );
          if (state !== "open-pref") {
            return;
          }

          browser.ownerGlobal.openPreferences("privacy-address-autofill");
        };
      } else {
        // We want to exclude the first time form filling.
        Services.telemetry.scalarAdd(
          "formautofill.addresses.fill_type_manual",
          1
        );
      }
    }
    return showDoorhanger;
  }

  async _onCreditCardSubmit(creditCard, browser, timeStartedFillingMS) {
    if (FormAutofill.isAutofillCreditCardsHideUI) {
      return false;
    }

    // Updates the used status for shield/heartbeat to recognize users who have
    // used Credit Card Autofill.
    let setUsedStatus = status => {
      if (FormAutofill.AutofillCreditCardsUsedStatus < status) {
        Services.prefs.setIntPref(
          FormAutofill.CREDITCARDS_USED_STATUS_PREF,
          status
        );
      }
    };

    // Remove invalid cc-type values
    if (
      creditCard.record["cc-type"] &&
      !lazy.CreditCard.isValidNetwork(creditCard.record["cc-type"])
    ) {
      // Let's reset the credit card to empty, and then network auto-detect will
      // pick it up.
      creditCard.record["cc-type"] = "";
    }

    // If `guid` is present, the form has been autofilled.
    if (creditCard.guid) {
      // Indicate that the user has used Credit Card Autofill to fill in a form.
      setUsedStatus(3);

      let originalCCData = await lazy.gFormAutofillStorage.creditCards.get(
        creditCard.guid
      );
      let recordUnchanged = true;
      for (let field in creditCard.record) {
        if (creditCard.record[field] === "" && !originalCCData[field]) {
          continue;
        }
        // Avoid updating the fields that users don't modify, but skip number field
        // because we don't want to trigger decryption here.
        let untouched = creditCard.untouchedFields.includes(field);
        if (untouched && field !== "cc-number") {
          creditCard.record[field] = originalCCData[field];
        }
        // recordUnchanged will be false if one of the field is changed.
        recordUnchanged &= untouched;
      }

      if (recordUnchanged) {
        lazy.gFormAutofillStorage.creditCards.notifyUsed(creditCard.guid);
        // Add probe to record credit card autofill(without modification).
        Services.telemetry.scalarAdd(
          "formautofill.creditCards.fill_type_autofill",
          1
        );
        this._recordFormFillingTime(
          "creditCard",
          "autofill",
          timeStartedFillingMS
        );
        return false;
      }
      // Add the probe to record credit card autofill with modification.
      Services.telemetry.scalarAdd(
        "formautofill.creditCards.fill_type_autofill_modified",
        1
      );
      this._recordFormFillingTime(
        "creditCard",
        "autofill-update",
        timeStartedFillingMS
      );
    } else {
      // Add the probe to record credit card manual filling.
      Services.telemetry.scalarAdd(
        "formautofill.creditCards.fill_type_manual",
        1
      );
      this._recordFormFillingTime("creditCard", "manual", timeStartedFillingMS);

      let existingGuid = await lazy.gFormAutofillStorage.creditCards.getDuplicateGuid(
        creditCard.record
      );

      if (existingGuid) {
        creditCard.guid = existingGuid;

        let originalCCData = await lazy.gFormAutofillStorage.creditCards.get(
          creditCard.guid
        );

        lazy.gFormAutofillStorage.creditCards._normalizeRecord(
          creditCard.record
        );

        // If the credit card record is a duplicate, check if the fields match the
        // record.
        let recordUnchanged = true;
        for (let field in creditCard.record) {
          if (field == "cc-number") {
            continue;
          }
          if (creditCard.record[field] != originalCCData[field]) {
            recordUnchanged = false;
            break;
          }
        }

        if (recordUnchanged) {
          // Indicate that the user neither sees the doorhanger nor uses Autofill
          // but somehow has a duplicate record in the storage. Will be reset to 2
          // if the doorhanger actually shows below.
          setUsedStatus(1);
          lazy.gFormAutofillStorage.creditCards.notifyUsed(creditCard.guid);
          return false;
        }
      }
    }

    // Indicate that the user has seen the doorhanger.
    setUsedStatus(2);

    return async () => {
      // Suppress the pending doorhanger from showing up if user disabled credit card in previous doorhanger.
      if (!FormAutofill.isAutofillCreditCardsEnabled) {
        return;
      }

      await lazy.FormAutofillPrompter.promptToSaveCreditCard(
        browser,
        creditCard,
        lazy.gFormAutofillStorage
      );
    };
  }

  async _onFormSubmit(data) {
    let {
      profile: { address, creditCard },
      timeStartedFillingMS,
    } = data;

    // Don't record filling time if any type of records has more than one section being
    // populated. We've been recording the filling time, so the other cases that aren't
    // recorded on the same basis should be out of the data samples. E.g. Filling time of
    // populating one profile is different from populating two sections, therefore, we
    // shouldn't record the later to regress the representation of existing statistics.
    if (address.length > 1 || creditCard.length > 1) {
      timeStartedFillingMS = null;
    }

    let browser = this.manager.browsingContext.top.embedderElement;

    // Transmit the telemetry immediately in the meantime form submitted, and handle these pending
    // doorhangers at a later.
    await Promise.all(
      [
        await Promise.all(
          address.map(addrRecord =>
            this._onAddressSubmit(addrRecord, browser, timeStartedFillingMS)
          )
        ),
        await Promise.all(
          creditCard.map(ccRecord =>
            this._onCreditCardSubmit(ccRecord, browser, timeStartedFillingMS)
          )
        ),
      ]
        .map(pendingDoorhangers => {
          return pendingDoorhangers.filter(
            pendingDoorhanger =>
              !!pendingDoorhanger && typeof pendingDoorhanger == "function"
          );
        })
        .map(pendingDoorhangers =>
          (async () => {
            for (const showDoorhanger of pendingDoorhangers) {
              await showDoorhanger();
            }
          })()
        )
    );
  }

  /**
   * Set the probes for the filling time with specific filling type and form type.
   *
   * @private
   * @param  {string} formType
   *         3 type of form (address/creditcard/address-creditcard).
   * @param  {string} fillingType
   *         3 filling type (manual/autofill/autofill-update).
   * @param  {int|null} startedFillingMS
   *         Time that form started to filling in ms. Early return if start time is null.
   */
  _recordFormFillingTime(formType, fillingType, startedFillingMS) {
    if (!startedFillingMS) {
      return;
    }
    let histogram = Services.telemetry.getKeyedHistogramById(
      "FORM_FILLING_REQUIRED_TIME_MS"
    );
    histogram.add(`${formType}-${fillingType}`, Date.now() - startedFillingMS);
  }
}
