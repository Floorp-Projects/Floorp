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

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
import { FirefoxRelayTelemetry } from "resource://gre/modules/FirefoxRelayTelemetry.mjs";
import { FormAutofill } from "resource://autofill/FormAutofill.sys.mjs";
import { FormAutofillUtils } from "resource://gre/modules/shared/FormAutofillUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddressComponent: "resource://gre/modules/shared/AddressComponent.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  FormAutofillPreferences:
    "resource://autofill/FormAutofillPreferences.sys.mjs",
  FormAutofillPrompter: "resource://autofill/FormAutofillPrompter.sys.mjs",
  FirefoxRelay: "resource://gre/modules/FirefoxRelay.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  OSKeyStore: "resource://gre/modules/OSKeyStore.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () =>
  FormAutofill.defineLogGetter(lazy, "FormAutofillParent")
);

const { ENABLED_AUTOFILL_ADDRESSES_PREF, ENABLED_AUTOFILL_CREDITCARDS_PREF } =
  FormAutofill;

const { ADDRESSES_COLLECTION_NAME, CREDITCARDS_COLLECTION_NAME } =
  FormAutofillUtils;

let gMessageObservers = new Set();

export let FormAutofillStatus = {
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
    Services.telemetry.setEventRecordingEnabled("address", true);
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
    const addressNames =
      await lazy.gFormAutofillStorage.addresses.getSavedFieldNames();

    // Don't access the credit cards store unless it is enabled.
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      const creditCardNames =
        await lazy.gFormAutofillStorage.creditCards.getSavedFieldNames();
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
ChromeUtils.defineLazyGetter(lazy, "gFormAutofillStorage", () => {
  let { formAutofillStorage } = ChromeUtils.importESModule(
    "resource://autofill/FormAutofillStorage.sys.mjs"
  );
  lazy.log.debug("Loading formAutofillStorage");

  formAutofillStorage.initialize().then(() => {
    // Update the saved field names to compute the status and update child processes.
    FormAutofillStatus.updateSavedFieldNames();
  });

  return formAutofillStorage;
});

export class FormAutofillParent extends JSWindowActorParent {
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
   * @param   {object} message
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
        const relayPromise = lazy.FirefoxRelay.autocompleteItemsAsync({
          formOrigin: this.formOrigin,
          scenarioName: data.scenarioName,
          hasInput: !!data.searchString?.length,
        });
        const recordsPromise = FormAutofillParent._getRecords(data);
        const [records, externalEntries] = await Promise.all([
          recordsPromise,
          relayPromise,
        ]);
        return { records, externalEntries };
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
      case "PasswordManager:offerRelayIntegration": {
        FirefoxRelayTelemetry.recordRelayOfferedEvent(
          "clicked",
          data.telemetry.flowId,
          data.telemetry.scenarioName
        );
        return this.#offerRelayIntegration();
      }
      case "PasswordManager:generateRelayUsername": {
        FirefoxRelayTelemetry.recordRelayUsernameFilledEvent(
          "clicked",
          data.telemetry.flowId
        );
        return this.#generateRelayUsername();
      }
    }

    return undefined;
  }

  get formOrigin() {
    return lazy.LoginHelper.getLoginOrigin(
      this.manager.documentPrincipal?.originNoSuffix
    );
  }

  getRootBrowser() {
    return this.browsingContext.topFrameElement;
  }

  async #offerRelayIntegration() {
    const browser = this.getRootBrowser();
    return lazy.FirefoxRelay.offerRelayIntegration(browser, this.formOrigin);
  }

  async #generateRelayUsername() {
    const browser = this.getRootBrowser();
    return lazy.FirefoxRelay.generateUsername(browser, this.formOrigin);
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
        console.error(ex);
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
   * @param  {object} data
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
        !String(fieldValue).toLowerCase().startsWith(lcSearchString)
      ) {
        continue;
      }
      records.push(record);
    }

    return records;
  }

  async _onAddressSubmit(address, browser) {
    const storage = lazy.gFormAutofillStorage.addresses;

    // Make sure record is normalized before comparing with records in the storage
    try {
      storage._normalizeRecord(address.record);
    } catch (_e) {
      return false;
    }

    const newAddress = new lazy.AddressComponent(
      address.record,
      // Invalid address fields in the address form will not be captured.
      { ignoreInvalid: true }
    );

    let oldRecord = {};
    let mergeableFields = [];

    // Exams all stored record to determine whether to show the prompt or not.
    for (const record of await storage.getAll()) {
      const savedAddress = new lazy.AddressComponent(record);
      // filter invalid field
      const result = newAddress.compare(savedAddress);

      if (Object.values(result).includes("different")) {
        // If any of the fields in the new address are different from the corresponding fields
        // in the saved address, the two addresses are considered different. For example, if
        // the name, email, country are the same but the street address is different, the two
        // addresses are not considered the same.
        continue;
      } else if (
        // If every field of the new address is either the same or is subset of the corresponding
        // field in the saved address, the new address is duplicated. We don't need capture
        // the new address.
        Object.values(result).every(r => ["same", "subset"].includes(r))
      ) {
        lazy.log.debug(
          "A duplicated address record is found, do not show the prompt"
        );
        storage.notifyUsed(record.guid);
        return false;
      } else {
        // If the new address is neither a duplicate of the saved address nor a different address.
        // There must be at least one field we can merge, show the update doorhanger
        lazy.log.debug(
          "A mergeable address record is found, show the update prompt"
        );
        // If we find multiple mergeable records, choose the record with fewest mergeable fields.
        // TODO: Bug 1830841. Add a testcase
        let fields = Object.entries(result)
          .filter(v => ["superset", "similar"].includes(v[1]))
          .map(v => v[0]);
        if (!mergeableFields.length || mergeableFields.length > fields.length) {
          oldRecord = record;
          mergeableFields = fields;
        }
      }
    }

    if (
      !FormAutofill.isAutofillAddressesCaptureEnabled &&
      !FormAutofill.isAutofillAddressesCaptureV2Enabled
    ) {
      return false;
    }

    return async () => {
      await lazy.FormAutofillPrompter.promptToSaveAddress(
        browser,
        storage,
        address.flowId,
        { oldRecord, newRecord: newAddress.record }
      );
    };
  }

  async _onCreditCardSubmit(creditCard, browser) {
    const storage = lazy.gFormAutofillStorage.creditCards;

    // Make sure record is normalized before comparing with records in the storage
    try {
      storage._normalizeRecord(creditCard.record);
    } catch (_e) {
      return false;
    }

    // If the record alreay exists in the storage, don't bother showing the prompt
    const matchRecord = (
      await storage.getMatchRecords(creditCard.record).next()
    ).value;
    if (matchRecord) {
      storage.notifyUsed(matchRecord.guid);
      return false;
    }

    // Suppress the pending doorhanger from showing up if user disabled credit card in previous doorhanger.
    if (!FormAutofill.isAutofillCreditCardsEnabled) {
      return false;
    }

    return async () => {
      await lazy.FormAutofillPrompter.promptToSaveCreditCard(
        browser,
        storage,
        creditCard.record,
        creditCard.flowId
      );
    };
  }

  async _onFormSubmit(data) {
    let { address, creditCard } = data;

    let browser = this.manager.browsingContext.top.embedderElement;

    // Transmit the telemetry immediately in the meantime form submitted, and handle these pending
    // doorhangers at a later.
    await Promise.all(
      [
        await Promise.all(
          address.map(addrRecord => this._onAddressSubmit(addrRecord, browser))
        ),
        await Promise.all(
          creditCard.map(ccRecord =>
            this._onCreditCardSubmit(ccRecord, browser)
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
}
