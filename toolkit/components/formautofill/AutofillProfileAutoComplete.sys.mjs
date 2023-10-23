/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Form Autofill content process module.
 */

import { GenericAutocompleteItem } from "resource://gre/modules/FillHelpers.sys.mjs";

/* eslint-disable no-use-before-define */

const Cm = Components.manager;

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddressResult: "resource://autofill/ProfileAutoCompleteResult.sys.mjs",
  ComponentUtils: "resource://gre/modules/ComponentUtils.sys.mjs",
  CreditCardResult: "resource://autofill/ProfileAutoCompleteResult.sys.mjs",
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  FormAutofillContent: "resource://autofill/FormAutofillContent.sys.mjs",
  FormScenarios: "resource://gre/modules/FormScenarios.sys.mjs",
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.sys.mjs",
});

const autocompleteController = Cc[
  "@mozilla.org/autocomplete/controller;1"
].getService(Ci.nsIAutoCompleteController);

ChromeUtils.defineLazyGetter(
  lazy,
  "ADDRESSES_COLLECTION_NAME",
  () => lazy.FormAutofillUtils.ADDRESSES_COLLECTION_NAME
);
ChromeUtils.defineLazyGetter(
  lazy,
  "CREDITCARDS_COLLECTION_NAME",
  () => lazy.FormAutofillUtils.CREDITCARDS_COLLECTION_NAME
);
ChromeUtils.defineLazyGetter(
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

    let factory =
      lazy.ComponentUtils.generateSingletonFactory(targetConstructor);
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
 * @class
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
   * @param {object} previousResult a previous result to use for faster searchinig
   * @param {object} listener the listener to notify when the search is complete
   */
  startSearch(searchString, searchParam, previousResult, listener) {
    let {
      activeInput,
      activeSection,
      activeFieldDetail,
      activeHandler,
      savedFieldNames,
    } = lazy.FormAutofillContent;
    this.forceStop = false;

    let isAddressField = lazy.FormAutofillUtils.isAddressField(
      activeFieldDetail.fieldName
    );
    const isCreditCardField = lazy.FormAutofillUtils.isCreditCardField(
      activeFieldDetail.fieldName
    );
    let isInputAutofilled =
      activeHandler.getFilledStateByElement(activeInput) ==
      lazy.FIELD_STATES.AUTO_FILLED;
    let allFieldNames = activeSection.allFieldNames;
    let filledRecordGUID = activeSection.filledRecordGUID;

    let searchPermitted = isAddressField
      ? lazy.FormAutofill.isAutofillAddressesEnabled
      : lazy.FormAutofill.isAutofillCreditCardsEnabled;
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
        ({ records, externalEntries }) => {
          if (this.forceStop) {
            return null;
          }
          // Sort addresses by timeLastUsed for showing the lastest used address at top.
          records.sort((a, b) => b.timeLastUsed - a.timeLastUsed);

          let adaptedRecords = activeSection.getAdaptedProfiles(records);
          let handler = lazy.FormAutofillContent.activeHandler;
          let isSecure = lazy.InsecurePasswordUtils.isFormSecure(handler.form);

          const result = new AutocompleteResult(
            searchString,
            activeFieldDetail.fieldName,
            allFieldNames,
            adaptedRecords,
            { isSecure, isInputAutofilled }
          );

          result.externalEntries.push(
            ...externalEntries.map(
              entry =>
                new GenericAutocompleteItem(
                  entry.image,
                  entry.title,
                  entry.subtitle,
                  entry.fillMessageName,
                  entry.fillMessageData
                )
            )
          );

          return result;
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
   * @param  {object} input
   *         Input element for autocomplete.
   * @param  {object} data
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
    return actor.sendQuery("FormAutofill:GetRecords", {
      scenarioName:
        "signUpForm" in lazy.FormScenarios.detect({ input })
          ? "SignUpFormScenario"
          : "",
      ...data,
    });
  },
};

export const ProfileAutocomplete = {
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
        if (!lazy.FormAutofillContent.activeInput) {
          // The observer notification is for autocomplete in a different process.
          break;
        }
        lazy.FormAutofillContent.autofillPending = true;
        Services.obs.notifyObservers(null, "autofill-fill-starting");
        await this._fillFromAutocompleteRow(
          lazy.FormAutofillContent.activeInput
        );
        Services.obs.notifyObservers(null, "autofill-fill-complete");
        lazy.FormAutofillContent.autofillPending = false;
        break;
      }
    }
  },

  fillRequestId: 0,

  async sendFillRequestToFormAutofillParent(input, comment) {
    if (!comment) {
      return false;
    }

    if (!input || input != autocompleteController?.input.focusedInput) {
      return false;
    }

    const { fillMessageName, fillMessageData } = JSON.parse(comment ?? "{}");
    if (!fillMessageName) {
      return false;
    }

    this.fillRequestId++;
    const fillRequestId = this.fillRequestId;
    const actor = getActorFromWindow(input.ownerGlobal, "FormAutofill");
    const value = await actor.sendQuery(fillMessageName, fillMessageData ?? {});

    // skip fill if another fill operation started during await
    if (fillRequestId != this.fillRequestId) {
      return false;
    }

    if (typeof value !== "string") {
      return false;
    }

    // If AutoFillParent returned a string to fill, we must do it here because
    // nsAutoCompleteController.cpp already finished it's work before we finished await.
    input.setUserInput(value);
    input.select(value.length, value.length);

    return true;
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
    let formDetails = lazy.FormAutofillContent.activeFormDetails;
    if (!formDetails) {
      // The observer notification is for a different frame.
      return;
    }

    let selectedIndex = this._getSelectedIndex(focusedInput.ownerGlobal);
    const validIndex =
      selectedIndex >= 0 &&
      selectedIndex < this.lastProfileAutoCompleteResult?.matchCount;
    const comment = validIndex
      ? this.lastProfileAutoCompleteResult.getCommentAt(selectedIndex)
      : null;

    if (
      selectedIndex == -1 ||
      !this.lastProfileAutoCompleteResult ||
      this.lastProfileAutoCompleteResult.getStyleAt(selectedIndex) !=
        "autofill-profile"
    ) {
      await this.sendFillRequestToFormAutofillParent(focusedInput, comment);
      return;
    }

    let profile = JSON.parse(comment);

    await lazy.FormAutofillContent.activeHandler.autofillFormFields(profile);
  },

  _clearProfilePreview() {
    if (
      !this.lastProfileAutoCompleteFocusedInput ||
      !lazy.FormAutofillContent.activeSection
    ) {
      return;
    }

    lazy.FormAutofillContent.activeSection.clearPreviewedFormFields();
  },

  _previewSelectedProfile(selectedIndex) {
    if (
      !lazy.FormAutofillContent.activeInput ||
      !lazy.FormAutofillContent.activeFormDetails
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
    lazy.FormAutofillContent.activeSection.previewFormFields(profile);
  },
};
