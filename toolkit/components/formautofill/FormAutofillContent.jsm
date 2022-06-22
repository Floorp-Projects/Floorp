/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Form Autofill content process module.
 */

/* eslint-disable no-use-before-define */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofillContent"];

const Cm = Components.manager;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddressResult: "resource://autofill/ProfileAutoCompleteResult.jsm",
  ComponentUtils: "resource://gre/modules/ComponentUtils.jsm",
  CreditCardResult: "resource://autofill/ProfileAutoCompleteResult.jsm",
  CreditCardTelemetry: "resource://autofill/FormAutofillTelemetryUtils.jsm",
  FormAutofill: "resource://autofill/FormAutofill.jsm",
  FormAutofillHandler: "resource://autofill/FormAutofillHandler.jsm",
  FormAutofillUtils: "resource://autofill/FormAutofillUtils.jsm",
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.jsm",
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DELEGATE_AUTOCOMPLETE",
  "toolkit.autocomplete.delegate",
  false
);

const formFillController = Cc[
  "@mozilla.org/satchel/form-fill-controller;1"
].getService(Ci.nsIFormFillController);
const autocompleteController = Cc[
  "@mozilla.org/autocomplete/controller;1"
].getService(Ci.nsIAutoCompleteController);

XPCOMUtils.defineLazyGetter(
  lazy,
  "ADDRESSES_COLLECTION_NAME",
  () => lazy.FormAutofillUtils.ADDRESSES_COLLECTION_NAME
);
XPCOMUtils.defineLazyGetter(
  lazy,
  "CREDITCARDS_COLLECTION_NAME",
  () => lazy.FormAutofillUtils.CREDITCARDS_COLLECTION_NAME
);
XPCOMUtils.defineLazyGetter(
  lazy,
  "FIELD_STATES",
  () => lazy.FormAutofillUtils.FIELD_STATES
);

function getActorFromWindow(contentWindow, name = "FormAutofill") {
  // In unit tests, contentWindow isn't a real window.
  if (!contentWindow) {
    return null;
  }

  return contentWindow.windowGlobalChild
    ? contentWindow.windowGlobalChild.getActor(name)
    : null;
}

// Register/unregister a constructor as a factory.
function AutocompleteFactory() {}
AutocompleteFactory.prototype = {
  register(targetConstructor) {
    let proto = targetConstructor.prototype;
    this._classID = proto.classID;

    let factory = lazy.ComponentUtils._getFactory(targetConstructor);
    this._factory = factory;

    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(
      proto.classID,
      proto.classDescription,
      proto.contractID,
      factory
    );

    if (proto.classID2) {
      this._classID2 = proto.classID2;
      registrar.registerFactory(
        proto.classID2,
        proto.classDescription,
        proto.contractID2,
        factory
      );
    }
  },

  unregister() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._classID, this._factory);
    if (this._classID2) {
      registrar.unregisterFactory(this._classID2, this._factory);
    }
    this._factory = null;
  },
};

/**
 * @constructor
 *
 * @implements {nsIAutoCompleteSearch}
 */
function AutofillProfileAutoCompleteSearch() {
  this.log = lazy.FormAutofill.defineLogGetter(
    this,
    "AutofillProfileAutoCompleteSearch"
  );
}
AutofillProfileAutoCompleteSearch.prototype = {
  classID: Components.ID("4f9f1e4c-7f2c-439e-9c9e-566b68bc187d"),
  contractID: "@mozilla.org/autocomplete/search;1?name=autofill-profiles",
  classDescription: "AutofillProfileAutoCompleteSearch",
  QueryInterface: ChromeUtils.generateQI(["nsIAutoCompleteSearch"]),

  // Begin nsIAutoCompleteSearch implementation

  /**
   * Searches for a given string and notifies a listener (either synchronously
   * or asynchronously) of the result
   *
   * @param {string} searchString the string to search for
   * @param {string} searchParam
   * @param {Object} previousResult a previous result to use for faster searchinig
   * @param {Object} listener the listener to notify when the search is complete
   */
  startSearch(searchString, searchParam, previousResult, listener) {
    let {
      activeInput,
      activeSection,
      activeFieldDetail,
      savedFieldNames,
    } = FormAutofillContent;
    this.forceStop = false;

    let isAddressField = lazy.FormAutofillUtils.isAddressField(
      activeFieldDetail.fieldName
    );
    const isCreditCardField = lazy.FormAutofillUtils.isCreditCardField(
      activeFieldDetail.fieldName
    );
    let isInputAutofilled =
      activeFieldDetail.state == lazy.FIELD_STATES.AUTO_FILLED;
    let allFieldNames = activeSection.allFieldNames;
    let filledRecordGUID = activeSection.filledRecordGUID;

    let creditCardsEnabledAndVisible =
      lazy.FormAutofill.isAutofillCreditCardsEnabled &&
      !lazy.FormAutofill.isAutofillCreditCardsHideUI;
    let searchPermitted = isAddressField
      ? lazy.FormAutofill.isAutofillAddressesEnabled
      : creditCardsEnabledAndVisible;
    let AutocompleteResult = isAddressField
      ? lazy.AddressResult
      : lazy.CreditCardResult;
    let isFormAutofillSearch = true;
    let pendingSearchResult = null;

    ProfileAutocomplete.lastProfileAutoCompleteFocusedInput = activeInput;
    // Fallback to form-history if ...
    //   - specified autofill feature is pref off.
    //   - no profile can fill the currently-focused input.
    //   - the current form has already been populated and the field is not
    //     an empty credit card field.
    //   - (address only) less than 3 inputs are covered by all saved fields in the storage.
    if (
      !searchPermitted ||
      !savedFieldNames.has(activeFieldDetail.fieldName) ||
      (!isInputAutofilled &&
        filledRecordGUID &&
        !(isCreditCardField && activeInput.value === "")) ||
      (isAddressField &&
        allFieldNames.filter(field => savedFieldNames.has(field)).length <
          lazy.FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD)
    ) {
      isFormAutofillSearch = false;
      if (activeInput.autocomplete == "off") {
        // Create a dummy result as an empty search result.
        pendingSearchResult = new AutocompleteResult("", "", [], [], {});
      } else {
        pendingSearchResult = new Promise(resolve => {
          let formHistory = Cc[
            "@mozilla.org/autocomplete/search;1?name=form-history"
          ].createInstance(Ci.nsIAutoCompleteSearch);
          formHistory.startSearch(searchString, searchParam, previousResult, {
            onSearchResult: (_, result) => resolve(result),
          });
        });
      }
    } else if (isInputAutofilled) {
      pendingSearchResult = new AutocompleteResult(searchString, "", [], [], {
        isInputAutofilled,
      });
    } else {
      let infoWithoutElement = { ...activeFieldDetail };
      delete infoWithoutElement.elementWeakRef;

      let data = {
        collectionName: isAddressField
          ? lazy.ADDRESSES_COLLECTION_NAME
          : lazy.CREDITCARDS_COLLECTION_NAME,
        info: infoWithoutElement,
        searchString,
      };

      pendingSearchResult = this._getRecords(activeInput, data).then(
        records => {
          if (this.forceStop) {
            return null;
          }
          // Sort addresses by timeLastUsed for showing the lastest used address at top.
          records.sort((a, b) => b.timeLastUsed - a.timeLastUsed);

          let adaptedRecords = activeSection.getAdaptedProfiles(records);
          let handler = FormAutofillContent.activeHandler;
          let isSecure = lazy.InsecurePasswordUtils.isFormSecure(handler.form);

          return new AutocompleteResult(
            searchString,
            activeFieldDetail.fieldName,
            allFieldNames,
            adaptedRecords,
            { isSecure, isInputAutofilled }
          );
        }
      );
    }

    Promise.resolve(pendingSearchResult).then(result => {
      if (this.forceStop) {
        // If we notify the listener the search result when the search is already
        // cancelled, it corrupts the internal state of the listener. So we only
        // reset the controller's state in this case.
        if (isFormAutofillSearch) {
          autocompleteController.resetInternalState();
        }
        return;
      }

      listener.onSearchResult(this, result);
      // Don't save cache results or reset state when returning non-autofill results such as the
      // form history fallback above.
      if (isFormAutofillSearch) {
        ProfileAutocomplete.lastProfileAutoCompleteResult = result;
        // Reset AutoCompleteController's state at the end of startSearch to ensure that
        // none of form autofill result will be cached in other places and make the
        // result out of sync.
        autocompleteController.resetInternalState();
      } else {
        // Clear the cache so that we don't try to autofill from it after falling
        // back to form history.
        ProfileAutocomplete.lastProfileAutoCompleteResult = null;
      }
    });
  },

  /**
   * Stops an asynchronous search that is in progress
   */
  stopSearch() {
    ProfileAutocomplete.lastProfileAutoCompleteResult = null;
    this.forceStop = true;
  },

  /**
   * Get the records from parent process for AutoComplete result.
   *
   * @private
   * @param  {Object} input
   *         Input element for autocomplete.
   * @param  {Object} data
   *         Parameters for querying the corresponding result.
   * @param  {string} data.collectionName
   *         The name used to specify which collection to retrieve records.
   * @param  {string} data.searchString
   *         The typed string for filtering out the matched records.
   * @param  {string} data.info
   *         The input autocomplete property's information.
   * @returns {Promise}
   *          Promise that resolves when addresses returned from parent process.
   */
  _getRecords(input, data) {
    if (!input) {
      return [];
    }

    let actor = getActorFromWindow(input.ownerGlobal);
    return actor.sendQuery("FormAutofill:GetRecords", data);
  },
};

let ProfileAutocomplete = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  lastProfileAutoCompleteResult: null,
  lastProfileAutoCompleteFocusedInput: null,
  _registered: false,
  _factory: null,

  ensureRegistered() {
    if (this._registered) {
      return;
    }

    this.log = lazy.FormAutofill.defineLogGetter(this, "ProfileAutocomplete");
    this.debug("ensureRegistered");
    this._factory = new AutocompleteFactory();
    this._factory.register(AutofillProfileAutoCompleteSearch);
    this._registered = true;

    Services.obs.addObserver(this, "autocomplete-will-enter-text");

    this.debug(
      "ensureRegistered. Finished with _registered:",
      this._registered
    );
  },

  ensureUnregistered() {
    if (!this._registered) {
      return;
    }

    this.debug("ensureUnregistered");
    this._factory.unregister();
    this._factory = null;
    this._registered = false;
    this._lastAutoCompleteResult = null;

    Services.obs.removeObserver(this, "autocomplete-will-enter-text");
  },

  async observe(subject, topic, data) {
    switch (topic) {
      case "autocomplete-will-enter-text": {
        if (!FormAutofillContent.activeInput) {
          // The observer notification is for autocomplete in a different process.
          break;
        }
        FormAutofillContent.autofillPending = true;
        Services.obs.notifyObservers(null, "autofill-fill-starting");
        await this._fillFromAutocompleteRow(FormAutofillContent.activeInput);
        Services.obs.notifyObservers(null, "autofill-fill-complete");
        FormAutofillContent.autofillPending = false;
        break;
      }
    }
  },

  _getSelectedIndex(contentWindow) {
    let actor = getActorFromWindow(contentWindow, "AutoComplete");
    if (!actor) {
      throw new Error("Invalid autocomplete selectedIndex");
    }

    return actor.selectedIndex;
  },

  async _fillFromAutocompleteRow(focusedInput) {
    this.debug("_fillFromAutocompleteRow:", focusedInput);
    let formDetails = FormAutofillContent.activeFormDetails;
    if (!formDetails) {
      // The observer notification is for a different frame.
      return;
    }

    let selectedIndex = this._getSelectedIndex(focusedInput.ownerGlobal);
    if (
      selectedIndex == -1 ||
      !this.lastProfileAutoCompleteResult ||
      this.lastProfileAutoCompleteResult.getStyleAt(selectedIndex) !=
        "autofill-profile"
    ) {
      return;
    }

    let profile = JSON.parse(
      this.lastProfileAutoCompleteResult.getCommentAt(selectedIndex)
    );

    await FormAutofillContent.activeHandler.autofillFormFields(profile);
  },

  _clearProfilePreview() {
    if (
      !this.lastProfileAutoCompleteFocusedInput ||
      !FormAutofillContent.activeSection
    ) {
      return;
    }

    FormAutofillContent.activeSection.clearPreviewedFormFields();
  },

  _previewSelectedProfile(selectedIndex) {
    if (
      !FormAutofillContent.activeInput ||
      !FormAutofillContent.activeFormDetails
    ) {
      // The observer notification is for a different process/frame.
      return;
    }

    if (
      !this.lastProfileAutoCompleteResult ||
      this.lastProfileAutoCompleteResult.getStyleAt(selectedIndex) !=
        "autofill-profile"
    ) {
      return;
    }

    let profile = JSON.parse(
      this.lastProfileAutoCompleteResult.getCommentAt(selectedIndex)
    );
    FormAutofillContent.activeSection.previewFormFields(profile);
  },
};

/**
 * Handles content's interactions for the process.
 *
 * NOTE: Declares it by "var" to make it accessible in unit tests.
 */
var FormAutofillContent = {
  /**
   * @type {WeakMap} mapping FormLike root HTML elements to FormAutofillHandler objects.
   */
  _formsDetails: new WeakMap(),

  /**
   * @type {Set} Set of the fields with usable values in any saved profile.
   */
  get savedFieldNames() {
    return Services.cpmm.sharedData.get("FormAutofill:savedFieldNames");
  },

  /**
   * @type {Object} The object where to store the active items, e.g. element,
   * handler, section, and field detail.
   */
  _activeItems: {},

  /**
   * @type {boolean} Flag indicating whether a focus action requiring
   * the popup to be active is pending.
   */
  _popupPending: false,

  /**
   * @type {boolean} Flag indicating whether the form is waiting to be
   * filled by Autofill.
   */
  _autofillPending: false,

  init() {
    this.log = lazy.FormAutofill.defineLogGetter(this, "FormAutofillContent");
    this.debug("init");

    // eslint-disable-next-line mozilla/balanced-listeners
    Services.cpmm.sharedData.addEventListener("change", this);

    let autofillEnabled = Services.cpmm.sharedData.get("FormAutofill:enabled");
    // If storage hasn't be initialized yet autofillEnabled is undefined but we need to ensure
    // autocomplete is registered before the focusin so register it in this case as long as the
    // pref is true.
    let shouldEnableAutofill =
      autofillEnabled === undefined &&
      (lazy.FormAutofill.isAutofillAddressesEnabled ||
        lazy.FormAutofill.isAutofillCreditCardsEnabled);
    if (autofillEnabled || shouldEnableAutofill) {
      ProfileAutocomplete.ensureRegistered();
    }
  },

  /**
   * Send the profile to parent for doorhanger and storage saving/updating.
   *
   * @param {Object} profile Submitted form's address/creditcard guid and record.
   * @param {Object} domWin Current content window.
   * @param {int} timeStartedFillingMS Time of form filling started.
   */
  _onFormSubmit(profile, domWin, timeStartedFillingMS) {
    let actor = getActorFromWindow(domWin);
    actor.sendAsyncMessage("FormAutofill:OnFormSubmit", {
      profile,
      timeStartedFillingMS,
    });
  },

  /**
   * Handle a form submission and early return when:
   * 1. In private browsing mode.
   * 2. Could not map any autofill handler by form element.
   * 3. Number of filled fields is less than autofill threshold
   *
   * @param {HTMLElement} formElement Root element which receives submit event.
   * @param {Window} domWin Content window; passed for unit tests and when
   *                 invoked by the FormAutofillSection
   * @param {Object} handler FormAutofillHander, if known by caller
   */
  formSubmitted(
    formElement,
    domWin = formElement.ownerGlobal,
    handler = undefined
  ) {
    this.debug("Handling form submission");

    if (!lazy.FormAutofill.isAutofillEnabled) {
      this.debug("Form Autofill is disabled");
      return;
    }

    // The `domWin` truthiness test is used by unit tests to bypass this check.
    if (domWin && lazy.PrivateBrowsingUtils.isContentWindowPrivate(domWin)) {
      this.debug("Ignoring submission in a private window");
      return;
    }

    handler = handler ?? this._formsDetails.get(formElement);
    if (!handler) {
      this.debug("Form element could not map to an existing handler");
      return;
    }

    let records = handler.createRecords();
    if (!Object.values(records).some(typeRecords => typeRecords.length)) {
      return;
    }

    lazy.CreditCardTelemetry.recordFormSubmitted(
      records,
      handler.form.elements
    );

    this._onFormSubmit(records, domWin, handler.timeStartedFillingMS);
  },

  handleEvent(evt) {
    switch (evt.type) {
      case "change": {
        if (!evt.changedKeys.includes("FormAutofill:enabled")) {
          return;
        }
        if (Services.cpmm.sharedData.get("FormAutofill:enabled")) {
          ProfileAutocomplete.ensureRegistered();
          if (this._popupPending) {
            this._popupPending = false;
            this.debug("handleEvent: Opening deferred popup");
            formFillController.showPopup();
          }
        } else {
          ProfileAutocomplete.ensureUnregistered();
        }
        break;
      }
    }
  },

  /**
   * Get the form's handler from cache which is created after page identified.
   *
   * @param {HTMLInputElement} element Focused input which triggered profile searching
   * @returns {Array<Object>|null}
   *          Return target form's handler from content cache
   *          (or return null if the information is not found in the cache).
   *
   */
  _getFormHandler(element) {
    if (!element) {
      return null;
    }
    let rootElement = lazy.FormLikeFactory.findRootForField(element);
    return this._formsDetails.get(rootElement);
  },

  /**
   * Get the active form's information from cache which is created after page
   * identified.
   *
   * @returns {Array<Object>|null}
   *          Return target form's information from content cache
   *          (or return null if the information is not found in the cache).
   *
   */
  get activeFormDetails() {
    let formHandler = this.activeHandler;
    return formHandler ? formHandler.fieldDetails : null;
  },

  /**
   * All active items should be updated according the active element of
   * `formFillController.focusedInput`. All of them including element,
   * handler, section, and field detail, can be retrieved by their own getters.
   *
   * @param {HTMLElement|null} element The active item should be updated based
   * on this or `formFillController.focusedInput` will be taken.
   */
  updateActiveInput(element) {
    element = element || formFillController.focusedInput;
    if (!element) {
      this.debug("updateActiveElement: no element selected");
      this._activeItems = {};
      return;
    }
    this._activeItems = {
      elementWeakRef: Cu.getWeakReference(element),
      fieldDetail: null,
    };

    this.debug("updateActiveElement: checking for popup-on-focus");
    // We know this element just received focus. If it's a credit card field,
    // open its popup.
    if (this._autofillPending) {
      this.debug("updateActiveElement: skipping check; autofill is imminent");
    } else if (element.value?.length !== 0) {
      this.debug(
        `updateActiveElement: Not opening popup because field is not empty.`
      );
    } else {
      this.debug(
        "updateActiveElement: checking if empty field is cc-*: ",
        this.activeFieldDetail?.fieldName
      );

      if (
        this.activeFieldDetail?.fieldName?.startsWith("cc-") ||
        AppConstants.platform === "android"
      ) {
        if (Services.cpmm.sharedData.get("FormAutofill:enabled")) {
          this.debug("updateActiveElement: opening pop up");
          formFillController.showPopup();
        } else {
          this.debug(
            "updateActiveElement: Deferring pop-up until Autofill is ready"
          );
          this._popupPending = true;
        }
      }
    }
  },

  get activeInput() {
    let elementWeakRef = this._activeItems.elementWeakRef;
    return elementWeakRef ? elementWeakRef.get() : null;
  },

  get activeHandler() {
    const activeInput = this.activeInput;
    if (!activeInput) {
      return null;
    }

    // XXX: We are recomputing the activeHandler every time to avoid keeping a
    // reference on the active element. This might be called quite frequently
    // so if _getFormHandler/findRootForField become more costly, we should
    // look into caching this result (eg by adding a weakmap).
    let handler = this._getFormHandler(activeInput);
    if (handler) {
      handler.focusedInput = activeInput;
    }
    return handler;
  },

  get activeSection() {
    let formHandler = this.activeHandler;
    return formHandler ? formHandler.activeSection : null;
  },

  /**
   * Get the active input's information from cache which is created after page
   * identified.
   *
   * @returns {Object|null}
   *          Return the active input's information that cloned from content cache
   *          (or return null if the information is not found in the cache).
   */
  get activeFieldDetail() {
    if (!this._activeItems.fieldDetail) {
      let formDetails = this.activeFormDetails;
      if (!formDetails) {
        return null;
      }
      for (let detail of formDetails) {
        let detailElement = detail.elementWeakRef.get();
        if (detailElement && this.activeInput == detailElement) {
          this._activeItems.fieldDetail = detail;
          break;
        }
      }
    }
    return this._activeItems.fieldDetail;
  },

  set autofillPending(flag) {
    this.debug("Setting autofillPending to", flag);
    this._autofillPending = flag;
  },

  identifyAutofillFields(element) {
    this.debug(
      `identifyAutofillFields: ${element.ownerDocument.location?.hostname}`
    );

    if (lazy.DELEGATE_AUTOCOMPLETE || !this.savedFieldNames) {
      this.debug("identifyAutofillFields: savedFieldNames are not known yet");
      let actor = getActorFromWindow(element.ownerGlobal);
      if (actor) {
        actor.sendAsyncMessage("FormAutofill:InitStorage");
      }
    }

    let formHandler = this._getFormHandler(element);
    if (!formHandler) {
      let formLike = lazy.FormLikeFactory.createFromField(element);
      formHandler = new lazy.FormAutofillHandler(
        formLike,
        this.formSubmitted.bind(this)
      );
    } else if (!formHandler.updateFormIfNeeded(element)) {
      this.debug("No control is removed or inserted since last collection.");
      return;
    }
    let validDetails = formHandler.collectFormFields();

    this._formsDetails.set(formHandler.form.rootElement, formHandler);

    validDetails.forEach(detail =>
      this._markAsAutofillField(detail.elementWeakRef.get())
    );
  },

  clearForm() {
    let focusedInput =
      this.activeInput || ProfileAutocomplete._lastAutoCompleteFocusedInput;
    if (!focusedInput) {
      return;
    }

    this.activeSection.clearPopulatedForm();

    let fieldName = FormAutofillContent.activeFieldDetail?.fieldName;
    if (lazy.FormAutofillUtils.isCreditCardField(fieldName)) {
      lazy.CreditCardTelemetry.recordFormCleared(
        this.activeSection?.flowId,
        fieldName
      );
    }
  },

  previewProfile(doc) {
    let docWin = doc.ownerGlobal;
    let selectedIndex = ProfileAutocomplete._getSelectedIndex(docWin);
    let lastAutoCompleteResult =
      ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = this.activeInput;
    let actor = getActorFromWindow(docWin);

    if (
      selectedIndex === -1 ||
      !focusedInput ||
      !lastAutoCompleteResult ||
      lastAutoCompleteResult.getStyleAt(selectedIndex) != "autofill-profile"
    ) {
      actor.sendAsyncMessage("FormAutofill:UpdateWarningMessage", {});

      ProfileAutocomplete._clearProfilePreview();
    } else {
      let focusedInputDetails = this.activeFieldDetail;
      let profile = JSON.parse(
        lastAutoCompleteResult.getCommentAt(selectedIndex)
      );
      let allFieldNames = FormAutofillContent.activeSection.allFieldNames;
      let profileFields = allFieldNames.filter(
        fieldName => !!profile[fieldName]
      );

      let focusedCategory = lazy.FormAutofillUtils.getCategoryFromFieldName(
        focusedInputDetails.fieldName
      );
      let categories = lazy.FormAutofillUtils.getCategoriesFromFieldNames(
        profileFields
      );
      actor.sendAsyncMessage("FormAutofill:UpdateWarningMessage", {
        focusedCategory,
        categories,
      });

      ProfileAutocomplete._previewSelectedProfile(selectedIndex);
    }
  },

  onPopupClosed(selectedRowStyle) {
    this.debug("Popup has closed.");
    ProfileAutocomplete._clearProfilePreview();

    let lastAutoCompleteResult =
      ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = FormAutofillContent.activeInput;
    if (
      lastAutoCompleteResult &&
      FormAutofillContent._keyDownEnterForInput &&
      focusedInput === FormAutofillContent._keyDownEnterForInput &&
      focusedInput === ProfileAutocomplete.lastProfileAutoCompleteFocusedInput
    ) {
      if (selectedRowStyle == "autofill-footer") {
        let actor = getActorFromWindow(focusedInput.ownerGlobal);
        actor.sendAsyncMessage("FormAutofill:OpenPreferences");
      } else if (selectedRowStyle == "autofill-clear-button") {
        FormAutofillContent.clearForm();
      }
    }
  },

  onPopupOpened() {
    this.debug(
      "Popup has opened, automatic =",
      formFillController.passwordPopupAutomaticallyOpened
    );

    let fieldName = FormAutofillContent.activeFieldDetail?.fieldName;
    if (lazy.FormAutofillUtils.isCreditCardField(fieldName)) {
      lazy.CreditCardTelemetry.recordPopupShown(
        this.activeSection?.flowId,
        fieldName
      );
    }
  },

  _markAsAutofillField(field) {
    // Since Form Autofill popup is only for input element, any non-Input
    // element should be excluded here.
    if (!HTMLInputElement.isInstance(field)) {
      return;
    }

    formFillController.markAsAutofillField(field);
  },

  _onKeyDown(e) {
    delete FormAutofillContent._keyDownEnterForInput;
    let lastAutoCompleteResult =
      ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = FormAutofillContent.activeInput;
    if (
      e.keyCode != e.DOM_VK_RETURN ||
      !lastAutoCompleteResult ||
      !focusedInput ||
      focusedInput != ProfileAutocomplete.lastProfileAutoCompleteFocusedInput
    ) {
      return;
    }
    FormAutofillContent._keyDownEnterForInput = focusedInput;
  },
};

FormAutofillContent.init();
