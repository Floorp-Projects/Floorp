/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddressResult: "resource://autofill/ProfileAutoCompleteResult.sys.mjs",
  AutoCompleteChild: "resource://gre/actors/AutoCompleteChild.sys.mjs",
  AutofillTelemetry: "resource://gre/modules/shared/AutofillTelemetry.sys.mjs",
  CreditCardResult: "resource://autofill/ProfileAutoCompleteResult.sys.mjs",
  GenericAutocompleteItem: "resource://gre/modules/FillHelpers.sys.mjs",
  InsecurePasswordUtils: "resource://gre/modules/InsecurePasswordUtils.sys.mjs",
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
  FormAutofillContent: "resource://autofill/FormAutofillContent.sys.mjs",
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  FormScenarios: "resource://gre/modules/FormScenarios.sys.mjs",
  FormStateManager: "resource://gre/modules/shared/FormStateManager.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  FORM_SUBMISSION_REASON: "resource://gre/actors/FormHandlerChild.sys.mjs",
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

/**
 * Handles content's interactions for the frame.
 */
export class FormAutofillChild extends JSWindowActorChild {
  // Flag indicating whether the form is waiting to be filled by Autofill.
  #autofillPending = false;

  constructor() {
    super();

    this.log = lazy.FormAutofill.defineLogGetter(this, "FormAutofillChild");
    this.debug("init");

    this._nextHandleElement = null;
    this._hasDOMContentLoadedHandler = false;
    this._hasPendingTask = false;

    /**
     * @type {FormAutofillFieldDetailsManager} handling state management of current forms and handlers.
     */
    this._fieldDetailsManager = new lazy.FormStateManager(
      this.formSubmitted.bind(this),
      this.formAutofilled.bind(this)
    );

    lazy.AutoCompleteChild.addPopupStateListener(this);

    /**
     * Tracks whether the last form submission was triggered by a form submit event,
     * if so we'll ignore the page navigation that follows
     */
    this.isFollowingSubmitEvent = false;
  }

  didDestroy() {
    this._fieldDetailsManager.didDestroy();

    lazy.AutoCompleteChild.removePopupStateListener(this);
  }

  popupStateChanged(messageName, _data, _target) {
    if (!lazy.FormAutofill.isAutofillEnabled) {
      return;
    }

    switch (messageName) {
      case "AutoComplete:PopupClosed": {
        this.onPopupClosed();
        break;
      }
      case "AutoComplete:PopupOpened": {
        this.onPopupOpened();
        break;
      }
    }
  }

  /**
   * Identifies and marks each autofill field
   */
  identifyAutofillFields() {
    if (this._hasPendingTask) {
      return;
    }
    this._hasPendingTask = true;

    lazy.setTimeout(() => {
      const element = this._nextHandleElement;
      this.debug(
        `identifyAutofillFields: ${element.ownerDocument.location?.hostname}`
      );

      if (
        lazy.DELEGATE_AUTOCOMPLETE ||
        !lazy.FormAutofillContent.savedFieldNames
      ) {
        this.debug("identifyAutofillFields: savedFieldNames are not known yet");

        // Init can be asynchronous because we don't need anything from the parent
        // at this point.
        this.sendAsyncMessage("FormAutofill:InitStorage");
      }

      const validDetails =
        this._fieldDetailsManager.identifyAutofillFields(element);

      validDetails?.forEach(detail =>
        this._markAsAutofillField(detail.element)
      );
      if (validDetails.length) {
        this.manager
          .getActor("FormHandler")
          .registerFormSubmissionInterest(this, {
            includesFormRemoval: lazy.FormAutofill.captureOnFormRemoval,
            includesPageNavigation: lazy.FormAutofill.captureOnPageNavigation,
          });
      }

      this._hasPendingTask = false;
      this._nextHandleElement = null;
      // This is for testing purpose only which sends a notification to indicate that the
      // form has been identified, and ready to open popup.
      this.sendAsyncMessage("FormAutofill:FieldsIdentified");
      this.updateActiveInput();
    });
  }

  /**
   * We received a form-submission-detected event because
   * the page was navigated.
   */
  onPageNavigation() {
    if (!lazy.FormAutofill.captureOnPageNavigation) {
      return;
    }

    if (this.isFollowingSubmitEvent) {
      // The next page navigation should be handled as form submission again
      this.isFollowingSubmitEvent = false;
      return;
    }
    let weakIdentifiedForms = ChromeUtils.nondeterministicGetWeakMapKeys(
      this._fieldDetailsManager._formsDetails
    );
    const formSubmissionReason = lazy.FORM_SUBMISSION_REASON.PAGE_NAVIGATION;

    for (const form of weakIdentifiedForms) {
      // Disconnected forms are captured by the form removal heuristic
      if (!form.isConnected) {
        continue;
      }
      this.formSubmitted(form, formSubmissionReason);
    }
  }

  /**
   * We received a form-submission-detected event because
   * a form was removed from the DOM after a successful
   * xhr/fetch request
   *
   * @param {Event} form form to be submitted
   */
  onFormRemoval(form) {
    if (!lazy.FormAutofill.captureOnFormRemoval) {
      return;
    }

    const formSubmissionReason =
      lazy.FORM_SUBMISSION_REASON.FORM_REMOVAL_AFTER_FETCH;
    this.formSubmitted(form, formSubmissionReason);
    this.manager.getActor("FormHandler").unregisterFormRemovalInterest(this);
  }

  shouldIgnoreFormAutofillEvent(event) {
    let nodePrincipal = event.target.nodePrincipal;
    return nodePrincipal.isSystemPrincipal || nodePrincipal.schemeIs("about");
  }

  handleEvent(evt) {
    if (!evt.isTrusted) {
      return;
    }
    if (this.shouldIgnoreFormAutofillEvent(evt)) {
      return;
    }

    if (!this.windowContext) {
      // !this.windowContext must not be null, because we need the
      // windowContext and/or docShell to (un)register form submission listeners
      return;
    }

    switch (evt.type) {
      case "focusin": {
        if (lazy.FormAutofill.isAutofillEnabled) {
          this.onFocusIn(evt);
        }
        break;
      }
      case "form-submission-detected": {
        if (lazy.FormAutofill.isAutofillEnabled) {
          const formElement = evt.detail.form;
          const formSubmissionReason = evt.detail.reason;
          this.onFormSubmission(formElement, formSubmissionReason);
        }
        break;
      }

      default: {
        throw new Error("Unexpected event type");
      }
    }
  }

  onFocusIn(evt) {
    this.updateActiveInput();

    const element = evt.target;
    if (!lazy.FormAutofillUtils.isCreditCardOrAddressFieldType(element)) {
      return;
    }

    this._nextHandleElement = element;
    const doc = element.ownerDocument;
    if (doc.readyState === "loading") {
      // For auto-focused input, we might receive focus event before document becomes ready.
      // When this happens, run field identification after receiving `DOMContentLoaded` event
      if (!this._hasDOMContentLoadedHandler) {
        this._hasDOMContentLoadedHandler = true;
        doc.addEventListener(
          "DOMContentLoaded",
          () => this.identifyAutofillFields(),
          { once: true }
        );
      }
      return;
    }

    this.identifyAutofillFields();
  }

  /**
   * Handle form-submission-detected event (dispatched by FormHandlerChild)
   *
   * Depending on the heuristic that detected the form submission,
   * the form that is submitted is retrieved differently
   *
   * @param {HTMLFormElement} form that is being submitted
   * @param {string} reason heuristic that detected the form submission
   *                        (see FormHandlerChild.FORM_SUBMISSION_REASON)
   */
  onFormSubmission(form, reason) {
    switch (reason) {
      case lazy.FORM_SUBMISSION_REASON.PAGE_NAVIGATION:
        this.onPageNavigation();
        break;
      case lazy.FORM_SUBMISSION_REASON.FORM_SUBMIT_EVENT:
        this.formSubmitted(form, reason);
        break;
      case lazy.FORM_SUBMISSION_REASON.FORM_REMOVAL_AFTER_FETCH:
        this.onFormRemoval(form);
        break;
    }
  }

  async receiveMessage(message) {
    if (!lazy.FormAutofill.isAutofillEnabled) {
      return;
    }

    switch (message.name) {
      case "FormAutofill:PreviewProfile": {
        this.previewProfile(message.data);
        break;
      }
      case "FormAutofill:ClearForm": {
        this.clearForm();
        break;
      }
      case "FormAutofill:FillForm": {
        await this.autofillFields(message.data);
        break;
      }
    }
  }

  get activeFieldDetail() {
    return this._fieldDetailsManager.activeFieldDetail;
  }

  get activeFormDetails() {
    return this._fieldDetailsManager.activeFormDetails;
  }

  get activeInput() {
    return this._fieldDetailsManager.activeInput;
  }

  get activeHandler() {
    return this._fieldDetailsManager.activeHandler;
  }

  get activeSection() {
    return this._fieldDetailsManager.activeSection;
  }

  /**
   * Handle a form submission and early return when:
   * 1. In private browsing mode.
   * 2. Could not map any autofill handler by form element.
   * 3. Number of filled fields is less than autofill threshold
   *
   * @param {HTMLElement} formElement Root element which receives submit event.
   * @param {string} formSubmissionReason Reason for invoking the form submission
   *                 (see options for FORM_SUBMISSION_REASON in FormAutofillUtils))
   * @param {Window} domWin Content window; passed for unit tests and when
   *                 invoked by the FormAutofillSection
   * @param {object} handler FormAutofillHander, if known by caller
   */
  formSubmitted(
    formElement,
    formSubmissionReason,
    domWin = formElement.ownerGlobal,
    handler = undefined
  ) {
    this.debug(`Handling form submission - infered by ${formSubmissionReason}`);

    lazy.AutofillTelemetry.recordFormSubmissionHeuristicCount(
      formSubmissionReason
    );

    if (!lazy.FormAutofill.isAutofillEnabled) {
      this.debug("Form Autofill is disabled");
      return;
    }

    // The `domWin` truthiness test is used by unit tests to bypass this check.
    if (domWin && lazy.PrivateBrowsingUtils.isContentWindowPrivate(domWin)) {
      this.debug("Ignoring submission in a private window");
      return;
    }

    handler = handler || this._fieldDetailsManager._getFormHandler(formElement);
    const records = this._fieldDetailsManager.getRecords(formElement, handler);

    if (!records || !handler) {
      this.debug("Form element could not map to an existing handler");
      return;
    }

    // After a form submit event follows (most likely) a page navigation, so we set this flag
    // to not handle the following one as form submission in order to avoid re-submitting the same form.
    // Ideally, we should keep a record of the last submitted form details and based on that we
    // should decide if we want to submit a form (bug 1895437)
    this.isFollowingSubmitEvent = true;

    [records.address, records.creditCard].forEach((rs, idx) => {
      lazy.AutofillTelemetry.recordSubmittedSectionCount(
        idx == 0
          ? lazy.AutofillTelemetry.ADDRESS
          : lazy.AutofillTelemetry.CREDIT_CARD,
        rs?.length
      );

      rs?.forEach(r => {
        lazy.AutofillTelemetry.recordFormInteractionEvent(
          "submitted",
          r.section,
          {
            record: r,
            form: handler.form,
          }
        );
        delete r.section;
      });
    });

    this.sendAsyncMessage("FormAutofill:OnFormSubmit", records);
  }

  formAutofilled() {
    lazy.FormAutofillContent.showPopup();
  }

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
      return;
    }
    lazy.FormAutofillContent.updateActiveAutofillChild(this);

    this._fieldDetailsManager.updateActiveInput(element);
    this.debug("updateActiveElement: checking for popup-on-focus");
    // We know this element just received focus. If it's a credit card field,
    // open its popup.
    if (this.#autofillPending) {
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
        lazy.FormAutofillContent.showPopup();
      }
    }
  }

  clearForm() {
    if (!this.activeSection) {
      return;
    }

    this.activeSection.clearPopulatedForm();

    let fieldName = this.activeFieldDetail?.fieldName;
    if (lazy.FormAutofillUtils.isCreditCardField(fieldName)) {
      lazy.AutofillTelemetry.recordFormInteractionEvent(
        "cleared",
        this.activeSection,
        { fieldName }
      );
    }
  }

  get lastProfileAutoCompleteResult() {
    return this.manager.getActor("AutoComplete")?.lastProfileAutoCompleteResult;
  }

  get lastProfileAutoCompleteFocusedInput() {
    return this.manager.getActor("AutoComplete")
      ?.lastProfileAutoCompleteFocusedInput;
  }

  previewProfile(profile) {
    if (profile && this.activeSection) {
      const adaptedProfile = this.activeSection.getAdaptedProfiles([
        profile,
      ])[0];
      this.activeSection.previewFormFields(adaptedProfile);
    } else {
      this.activeSection.clearPreviewedFormFields();
    }
  }

  async autofillFields(profile) {
    this.#autofillPending = true;
    Services.obs.notifyObservers(null, "autofill-fill-starting");
    try {
      Services.obs.notifyObservers(null, "autofill-fill-starting");
      await this.activeHandler.autofillFormFields(profile);
      Services.obs.notifyObservers(null, "autofill-fill-complete");
    } finally {
      this.#autofillPending = false;
    }
  }

  onPopupClosed() {
    this.debug("Popup has closed.");
    this.activeSection?.clearPreviewedFormFields();
  }

  onPopupOpened() {
    this.debug(
      "Popup has opened, automatic =",
      formFillController.passwordPopupAutomaticallyOpened
    );

    let fieldName = this.activeFieldDetail?.fieldName;
    if (fieldName && this.activeSection) {
      lazy.AutofillTelemetry.recordFormInteractionEvent(
        "popup_shown",
        this.activeSection,
        { fieldName }
      );
    }
  }

  _markAsAutofillField(field) {
    // Since Form Autofill popup is only for input element, any non-Input
    // element should be excluded here.
    if (!HTMLInputElement.isInstance(field)) {
      return;
    }

    this.manager
      .getActor("AutoComplete")
      ?.markAsAutoCompletableField(field, this);
  }

  get actorName() {
    return "FormAutofill";
  }

  /**
   * Get the search options when searching for autocomplete entries in the parent
   *
   * @param {HTMLInputElement} input - The input element to search for autocompelte entries
   * @returns {object} the search options for the input
   */
  getAutoCompleteSearchOption(input) {
    const fieldDetail = this._fieldDetailsManager
      ._getFormHandler(input)
      ?.getFieldDetailByElement(input);

    const scenarioName = lazy.FormScenarios.detect({ input }).signUpForm
      ? "SignUpFormScenario"
      : "";
    return { fieldName: fieldDetail?.fieldName, scenarioName };
  }

  /**
   * Ask the provider whether it might have autocomplete entry to show
   * for the given input.
   *
   * @param {HTMLInputElement} input - The input element to search for autocompelte entries
   * @returns {boolean} true if we shold search for autocomplete entries
   */
  shouldSearchForAutoComplete(input) {
    const fieldDetail = this._fieldDetailsManager
      ._getFormHandler(input)
      ?.getFieldDetailByElement(input);
    if (!fieldDetail) {
      return false;
    }
    const fieldName = fieldDetail.fieldName;
    const isAddressField = lazy.FormAutofillUtils.isAddressField(fieldName);
    const searchPermitted = isAddressField
      ? lazy.FormAutofill.isAutofillAddressesEnabled
      : lazy.FormAutofill.isAutofillCreditCardsEnabled;
    // If the specified autofill feature is pref off, do not search
    if (!searchPermitted) {
      return false;
    }

    // No profile can fill the currently-focused input.
    if (!lazy.FormAutofillContent.savedFieldNames.has(fieldName)) {
      return false;
    }

    // The current form has already been populated and the field is not
    // an empty credit card field.
    const isCreditCardField =
      lazy.FormAutofillUtils.isCreditCardField(fieldName);
    const isInputAutofilled =
      input.autofillState == lazy.FormAutofillUtils.FIELD_STATES.AUTO_FILLED;
    const filledRecordGUID = this.activeSection.filledRecordGUID;
    if (
      !isInputAutofilled &&
      filledRecordGUID &&
      !(isCreditCardField && this.activeInput.value === "")
    ) {
      return false;
    }

    // (address only) less than 3 inputs are covered by all saved fields in the storage.
    if (
      isAddressField &&
      this.activeSection.allFieldNames.filter(field =>
        lazy.FormAutofillContent.savedFieldNames.has(field)
      ).length < lazy.FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD
    ) {
      return false;
    }

    return true;
  }

  /**
   * Convert the search result to autocomplete results
   *
   * @param {string} searchString - The string to search for
   * @param {HTMLInputElement} input - The input element to search for autocompelte entries
   * @param {Array<object>} records - autocomplete records
   * @returns {AutocompleteResult}
   */
  searchResultToAutoCompleteResult(searchString, input, records) {
    if (!records) {
      return null;
    }

    const entries = records.records;
    const externalEntries = records.externalEntries;

    const fieldDetail = this._fieldDetailsManager
      ._getFormHandler(input)
      ?.getFieldDetailByElement(input);
    if (!fieldDetail) {
      return null;
    }

    const adaptedRecords = this.activeSection.getAdaptedProfiles(entries);
    const isSecure = lazy.InsecurePasswordUtils.isFormSecure(
      this.activeHandler.form
    );
    const isInputAutofilled =
      input.autofillState == lazy.FormAutofillUtils.FIELD_STATES.AUTO_FILLED;
    const allFieldNames = this.activeSection.allFieldNames;

    const AutocompleteResult = lazy.FormAutofillUtils.isAddressField(
      fieldDetail.fieldName
    )
      ? lazy.AddressResult
      : lazy.CreditCardResult;

    const acResult = new AutocompleteResult(
      searchString,
      fieldDetail.fieldName,
      allFieldNames,
      adaptedRecords,
      { isSecure, isInputAutofilled }
    );

    acResult.externalEntries.push(
      ...externalEntries.map(
        entry =>
          new lazy.GenericAutocompleteItem(
            entry.image,
            entry.title,
            entry.subtitle,
            entry.fillMessageName,
            entry.fillMessageData
          )
      )
    );

    return acResult;
  }
}
