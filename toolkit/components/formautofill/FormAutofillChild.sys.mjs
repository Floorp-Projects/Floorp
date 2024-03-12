/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AutoCompleteChild: "resource://gre/actors/AutoCompleteChild.sys.mjs",
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
  FormAutofillContent: "resource://autofill/FormAutofillContent.sys.mjs",
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  FORM_SUBMISSION_REASON: "resource://gre/actors/FormHandlerChild.sys.mjs",
});

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

    this._nextHandleElement = null;
    this._alreadyDOMContentLoaded = false;
    this._hasDOMContentLoadedHandler = false;
    this._hasPendingTask = false;
    this.testListener = null;

    lazy.AutoCompleteChild.addPopupStateListener(this);
  }

  didDestroy() {
    lazy.AutoCompleteChild.removePopupStateListener(this);
    lazy.FormAutofillContent.didDestroy();
  }

  popupStateChanged(messageName, data, _target) {
    let docShell;
    try {
      docShell = this.docShell;
    } catch (ex) {
      lazy.AutoCompleteChild.removePopupStateListener(this);
      return;
    }

    if (!lazy.FormAutofill.isAutofillEnabled) {
      return;
    }

    const { chromeEventHandler } = docShell;

    switch (messageName) {
      case "FormAutoComplete:PopupClosed": {
        lazy.FormAutofillContent.onPopupClosed(data.selectedRowStyle);
        Services.tm.dispatchToMainThread(() => {
          chromeEventHandler.removeEventListener(
            "keydown",
            lazy.FormAutofillContent._onKeyDown,
            true
          );
        });

        break;
      }
      case "FormAutoComplete:PopupOpened": {
        lazy.FormAutofillContent.onPopupOpened();
        chromeEventHandler.addEventListener(
          "keydown",
          lazy.FormAutofillContent._onKeyDown,
          true
        );
        break;
      }
    }
  }

  /**
   * Invokes the FormAutofillContent to identify the autofill fields
   * and consider opening the dropdown menu for the focused field
   *
   */
  _doIdentifyAutofillFields() {
    if (this._hasPendingTask) {
      return;
    }
    this._hasPendingTask = true;

    lazy.setTimeout(() => {
      const isAnyFieldIdentified =
        lazy.FormAutofillContent.identifyAutofillFields(
          this._nextHandleElement
        );
      if (isAnyFieldIdentified) {
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
      lazy.FormAutofillContent.updateActiveInput();
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
   * (active window = FormAutofillContent.activeHandler.window)
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

    const activeElement =
      lazy.FormAutofillContent.activeFieldDetail?.elementWeakRef.deref();
    if (!activeElement) {
      return;
    }

    const formSubmissionReason = lazy.FORM_SUBMISSION_REASON.PAGE_NAVIGATION;

    // We only capture the form of the active field right now,
    // this means that we might miss some fields (see bug 1871356)
    lazy.FormAutofillContent.formSubmitted(activeElement, formSubmissionReason);
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
    lazy.FormAutofillContent.updateActiveInput();

    let element = evt.target;
    if (!lazy.FormAutofillUtils.isCreditCardOrAddressFieldType(element)) {
      return;
    }
    this._nextHandleElement = element;

    if (!this._alreadyDOMContentLoaded) {
      let doc = element.ownerDocument;
      if (doc.readyState === "loading") {
        if (!this._hasDOMContentLoadedHandler) {
          this._hasDOMContentLoadedHandler = true;
          doc.addEventListener(
            "DOMContentLoaded",
            () => this._doIdentifyAutofillFields(),
            { once: true }
          );
        }
        return;
      }
      this._alreadyDOMContentLoaded = true;
    }

    this._doIdentifyAutofillFields();
  }

  /**
   * Handle form-submission-detected event (dispatched by FormHandlerChild)
   *
   * @param {CustomEvent} evt form-submission-detected event
   */
  onFormSubmission(evt) {
    const formElement = evt.detail.form;
    const formSubmissionReason = evt.detail.reason;

    lazy.FormAutofillContent.formSubmitted(formElement, formSubmissionReason);
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

    lazy.FormAutofillContent.formSubmitted(evt.target, formSubmissionReason);
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

    const doc = this.document;

    switch (message.name) {
      case "FormAutofill:PreviewProfile": {
        lazy.FormAutofillContent.previewProfile(doc);
        break;
      }
      case "FormAutofill:ClearForm": {
        lazy.FormAutofillContent.clearForm();
        break;
      }
      case "FormAutofill:FillForm": {
        lazy.FormAutofillContent.activeHandler.autofillFormFields(message.data);
        break;
      }
    }
  }
}
