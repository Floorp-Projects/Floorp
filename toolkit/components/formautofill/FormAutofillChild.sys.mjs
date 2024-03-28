/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AutoCompleteChild: "resource://gre/actors/AutoCompleteChild.sys.mjs",
  AutofillTelemetry: "resource://gre/modules/shared/AutofillTelemetry.sys.mjs",
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
  FormAutofillContent: "resource://autofill/FormAutofillContent.sys.mjs",
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  FormStateManager: "resource://gre/modules/shared/FormStateManager.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ProfileAutocomplete:
    "resource://autofill/AutofillProfileAutoComplete.sys.mjs",
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

const observer = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    // Only handle pushState/replaceState here.
    if (
      !(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) ||
      !(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_PUSHSTATE)
    ) {
      return;
    }
    const window = aWebProgress.DOMWindow;
    const formAutofillChild = window.windowGlobalChild.getActor("FormAutofill");
    formAutofillChild.onPageNavigation();
  },

  onStateChange(aWebProgress, aRequest, aStateFlags, _aStatus) {
    if (
      // if restoring a previously-rendered presentation (bfcache)
      aStateFlags & Ci.nsIWebProgressListener.STATE_RESTORING &&
      aStateFlags & Ci.nsIWebProgressListener.STATE_STOP
    ) {
      return;
    }

    if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    // We only care about when a page triggered a load, not the user. For example:
    // clicking refresh/back/forward, typing a URL and hitting enter, and loading a bookmark aren't
    // likely to be when a user wants to save a formautofill data.
    let channel = aRequest.QueryInterface(Ci.nsIChannel);
    let triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
    if (
      triggeringPrincipal.isNullPrincipal ||
      triggeringPrincipal.equals(
        Services.scriptSecurityManager.getSystemPrincipal()
      )
    ) {
      return;
    }

    // Don't handle history navigation, reload, or pushState not triggered via chrome UI.
    // e.g. history.go(-1), location.reload(), history.replaceState()
    if (!(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_NORMAL)) {
      return;
    }

    const window = aWebProgress.DOMWindow;
    const formAutofillChild = window.windowGlobalChild.getActor("FormAutofill");
    formAutofillChild.onPageNavigation();
  },
};

/**
 * Handles content's interactions for the frame.
 */
export class FormAutofillChild extends JSWindowActorChild {
  constructor() {
    super();

    this.log = lazy.FormAutofill.defineLogGetter(this, "FormAutofillChild");
    this.debug("init");

    this._nextHandleElement = null;
    this._hasDOMContentLoadedHandler = false;
    this._hasPendingTask = false;

    // Flag indicating whether the form is waiting to be filled by Autofill.
    this._autofillPending = false;

    /**
     * @type {FormAutofillFieldDetailsManager} handling state management of current forms and handlers.
     */
    this._fieldDetailsManager = new lazy.FormStateManager(
      this.formSubmitted.bind(this),
      this.formAutofilled.bind(this)
    );

    lazy.AutoCompleteChild.addPopupStateListener(this);
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
        if (lazy.FormAutofill.captureOnFormRemoval) {
          this.registerDOMDocFetchSuccessEventListener();
        }
        if (lazy.FormAutofill.captureOnPageNavigation) {
          this.registerProgressListener();
        }
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
   * Gets the highest accessible docShell
   *
   * @returns {DocShell} highest accessible docShell
   */
  getHighestDocShell() {
    const window = this.document.defaultView;

    let docShell;
    for (
      let browsingContext = BrowsingContext.getFromWindow(window);
      browsingContext?.docShell;
      browsingContext = browsingContext.parent
    ) {
      docShell = browsingContext.docShell;
    }

    return docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
  }

  /**
   * After being notified of a page navigation, we check whether
   * the navigated window is the active window or one of its parents
   * (active window = activeHandler.window)
   *
   * @returns {boolean} whether the navigation affects the active window
   */
  isActiveWindowNavigation() {
    const activeWindow = lazy.FormAutofillContent.activeHandler?.window;
    const navigatedWindow = this.document.defaultView;

    if (!activeWindow || !navigatedWindow) {
      return false;
    }

    const navigatedBrowsingContext =
      BrowsingContext.getFromWindow(navigatedWindow);

    for (
      let browsingContext = BrowsingContext.getFromWindow(activeWindow);
      browsingContext?.docShell;
      browsingContext = browsingContext.parent
    ) {
      if (navigatedBrowsingContext === browsingContext) {
        return true;
      }
    }
    return false;
  }

  /**
   * Infer a form submission after document is navigated
   */
  onPageNavigation() {
    if (!this.isActiveWindowNavigation()) {
      return;
    }

    // TODO: We should not use FormAutofillContent and let the
    //       parent decides which child to notify
    const activeChild = lazy.FormAutofillContent.activeAutofillChild;
    const activeElement = activeChild.activeFieldDetail?.elementWeakRef.deref();
    if (!activeElement) {
      return;
    }

    const formSubmissionReason = lazy.FORM_SUBMISSION_REASON.PAGE_NAVIGATION;

    // We only capture the form of the active field right now,
    // this means that we might miss some fields (see bug 1871356)
    activeChild.formSubmitted(activeElement, formSubmissionReason);
  }

  /**
   * After a form submission we unregister the
   * nsIWebProgressListener from the top level doc shell
   */
  unregisterProgressListener() {
    const docShell = this.getHighestDocShell();
    try {
      docShell.removeProgressListener(observer);
    } catch (ex) {
      // Ignore NS_ERROR_FAILURE if the progress listener was not registered
    }
  }

  /**
   * After a focusin event and after we identified formautofill fields,
   * we set up a nsIWebProgressListener that notifies of a request state
   * change or window location change in the top level doc shell
   */
  registerProgressListener() {
    const docShell = this.getHighestDocShell();

    const flags =
      Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT |
      Ci.nsIWebProgress.NOTIFY_LOCATION;
    try {
      docShell.addProgressListener(observer, flags);
    } catch (ex) {
      // Ignore NS_ERROR_FAILURE if the progress listener was already added
    }
  }

  /**
   * After a focusin event and after we identify formautofill fields,
   * we set up an event listener for the DOMDocFetchSuccess event
   */
  registerDOMDocFetchSuccessEventListener() {
    this.document.setNotifyFetchSuccess(true);

    // Is removed after a DOMDocFetchSuccess event (bug 1864855)
    /* eslint-disable mozilla/balanced-listeners */
    this.docShell.chromeEventHandler.addEventListener(
      "DOMDocFetchSuccess",
      this,
      true
    );
  }

  /**
   * After a DOMDocFetchSuccess event, we register an event listener for the DOMFormRemoved event
   */
  registerDOMFormRemovedEventListener() {
    this.document.setNotifyFormOrPasswordRemoved(true);

    // Is removed after a DOMFormRemoved event (bug 1864855)
    /* eslint-disable mozilla/balanced-listeners */
    this.docShell.chromeEventHandler.addEventListener(
      "DOMFormRemoved",
      this,
      true
    );
  }

  /**
   * After a DOMDocFetchSuccess event we remove the DOMDocFetchSuccess event listener
   */
  unregisterDOMDocFetchSuccessEventListener() {
    this.document.setNotifyFetchSuccess(false);
    this.docShell.chromeEventHandler.removeEventListener(
      "DOMDocFetchSuccess",
      this
    );
  }

  /**
   * After a DOMFormRemoved event we remove the DOMFormRemoved event listener
   */
  unregisterDOMFormRemovedEventListener() {
    this.document.setNotifyFormOrPasswordRemoved(false);
    this.docShell.chromeEventHandler.removeEventListener(
      "DOMFormRemoved",
      this
    );
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
      case "DOMFormRemoved": {
        this.onDOMFormRemoved(evt);
        break;
      }
      case "DOMDocFetchSuccess": {
        this.onDOMDocFetchSuccess();
        break;
      }
      case "form-submission-detected": {
        if (lazy.FormAutofill.isAutofillEnabled) {
          this.onFormSubmission(evt);
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
   * @param {CustomEvent} evt form-submission-detected event
   */
  onFormSubmission(evt) {
    const formElement = evt.detail.form;
    const formSubmissionReason = evt.detail.reason;

    this.formSubmitted(formElement, formSubmissionReason);
  }

  /**
   * Handle the DOMFormRemoved event.
   *
   * Infers a form submission when the form is removed
   * after a successful fetch or XHR request.
   *
   * @param {Event} evt DOMFormRemoved
   */
  onDOMFormRemoved(evt) {
    const formSubmissionReason =
      lazy.FORM_SUBMISSION_REASON.FORM_REMOVAL_AFTER_FETCH;

    this.formSubmitted(evt.target, formSubmissionReason);
  }

  /**
   * Handle the DOMDocFetchSuccess event.
   *
   * Sets up an event listener for the DOMFormRemoved event
   * and unregisters the event listener for DOMDocFetchSuccess event.
   */
  onDOMDocFetchSuccess() {
    this.registerDOMFormRemovedEventListener();

    this.unregisterDOMDocFetchSuccessEventListener();
  }

  /**
   * Unregister all listeners that notify of a form submission,
   * because we just detected and acted on a form submission
   */
  unregisterFormSubmissionListeners() {
    this.unregisterDOMDocFetchSuccessEventListener();
    this.unregisterDOMFormRemovedEventListener();
    this.unregisterProgressListener();
  }

  receiveMessage(message) {
    if (!lazy.FormAutofill.isAutofillEnabled) {
      return;
    }

    switch (message.name) {
      case "FormAutofill:PreviewProfile": {
        this.previewProfile(message.data.selectedIndex);
        break;
      }
      case "FormAutofill:ClearForm": {
        this.clearForm();
        break;
      }
      case "FormAutofill:FillForm": {
        this.activeHandler.autofillFormFields(message.data);
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

    // Unregister the form submission listeners after handling a form submission
    this.debug("Unregistering form submission listeners");
    this.unregisterFormSubmissionListeners();

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
        lazy.FormAutofillContent.showPopup();
      }
    }
  }

  set autofillPending(flag) {
    this.debug("Setting autofillPending to", flag);
    this._autofillPending = flag;
  }

  clearForm() {
    let focusedInput =
      this.activeInput ||
      lazy.ProfileAutocomplete._lastAutoCompleteFocusedInput;
    if (!focusedInput) {
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

  previewProfile(selectedIndex) {
    let lastAutoCompleteResult =
      lazy.ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = this.activeInput;

    if (
      selectedIndex === -1 ||
      !focusedInput ||
      !lastAutoCompleteResult ||
      lastAutoCompleteResult.getStyleAt(selectedIndex) != "autofill"
    ) {
      lazy.ProfileAutocomplete._clearProfilePreview();
    } else {
      lazy.ProfileAutocomplete._previewSelectedProfile(selectedIndex);
    }
  }

  onPopupClosed() {
    this.debug("Popup has closed.");
    lazy.ProfileAutocomplete._clearProfilePreview();
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

    formFillController.markAsAutofillField(field);
  }
}
