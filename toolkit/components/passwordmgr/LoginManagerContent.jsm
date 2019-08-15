/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Module doing most of the content process work for the password manager.
 */

// Disable use-ownerGlobal since LoginForm doesn't have it.
/* eslint-disable mozilla/use-ownerGlobal */

"use strict";

const EXPORTED_SYMBOLS = ["LoginManagerContent"];

const PASSWORD_INPUT_ADDED_COALESCING_THRESHOLD_MS = 1;
const AUTOCOMPLETE_AFTER_RIGHT_CLICK_THRESHOLD_MS = 400;
const AUTOFILL_STATE = "-moz-autofill";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { CreditCard } = ChromeUtils.import(
  "resource://gre/modules/CreditCard.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormLikeFactory",
  "resource://gre/modules/FormLikeFactory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginFormFactory",
  "resource://gre/modules/LoginFormFactory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginRecipesContent",
  "resource://gre/modules/LoginRecipes.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "InsecurePasswordUtils",
  "resource://gre/modules/InsecurePasswordUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ContentDOMReference",
  "resource://gre/modules/ContentDOMReference.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gNetUtil",
  "@mozilla.org/network/util;1",
  "nsINetUtil"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerContent");
  return logger.log.bind(logger);
});

Services.cpmm.addMessageListener("clearRecipeCache", () => {
  LoginRecipesContent._clearRecipeCache();
});

let gLastRightClickTimeStamp = Number.NEGATIVE_INFINITY;

const observer = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
  ]),

  // nsIWebProgressListener
  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    // Only handle pushState/replaceState here.
    if (
      !(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) ||
      !(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_PUSHSTATE)
    ) {
      return;
    }

    log(
      "onLocationChange handled:",
      aLocation.displaySpec,
      aWebProgress.DOMWindow.document
    );

    LoginManagerContent._onNavigation(aWebProgress.DOMWindow.document);
  },

  onStateChange(aWebProgress, aRequest, aState, aStatus) {
    if (
      aState & Ci.nsIWebProgressListener.STATE_RESTORING &&
      aState & Ci.nsIWebProgressListener.STATE_STOP
    ) {
      // Re-fill a document restored from bfcache since password field values
      // aren't persisted there.
      LoginManagerContent._onDocumentRestored(aWebProgress.DOMWindow.document);
      return;
    }

    if (!(aState & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    // We only care about when a page triggered a load, not the user. For example:
    // clicking refresh/back/forward, typing a URL and hitting enter, and loading a bookmark aren't
    // likely to be when a user wants to save a login.
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
      log(
        "onStateChange: loadType isn't LOAD_CMD_NORMAL:",
        aWebProgress.loadType
      );
      return;
    }

    log("onStateChange handled:", channel);
    LoginManagerContent._onNavigation(aWebProgress.DOMWindow.document);
  },

  // nsIObserver
  observe(subject, topic, data) {
    switch (topic) {
      case "autocomplete-did-enter-text": {
        let input = subject.QueryInterface(Ci.nsIAutoCompleteInput);
        let { selectedIndex } = input.popup;
        if (selectedIndex < 0) {
          break;
        }

        let { focusedInput } = LoginManagerContent._formFillService;
        if (focusedInput.nodePrincipal.isNullPrincipal) {
          // If we have a null principal then prevent any more password manager code from running and
          // incorrectly using the document `location`.
          return;
        }

        let style = input.controller.getStyleAt(selectedIndex);
        if (style == "login" || style == "loginWithOrigin") {
          let details = JSON.parse(
            input.controller.getCommentAt(selectedIndex)
          );
          LoginManagerContent.onFieldAutoComplete(focusedInput, details.guid);
        } else if (style == "generatedPassword") {
          LoginManagerContent._highlightFilledField(focusedInput);
          LoginManagerContent._generatedPasswordFilledOrEdited(focusedInput);
        }
        break;
      }
    }
  },

  // nsIDOMEventListener
  handleEvent(aEvent) {
    if (!aEvent.isTrusted) {
      return;
    }

    if (!LoginHelper.enabled) {
      return;
    }

    switch (aEvent.type) {
      // Used to mask fields with filled generated passwords when blurred.
      case "blur": {
        let unmask = false;
        LoginManagerContent._togglePasswordFieldMasking(aEvent.target, unmask);
        break;
      }

      // Used to watch for changes to fields filled with generated passwords.
      case "change": {
        LoginManagerContent._generatedPasswordFilledOrEdited(aEvent.target);
        break;
      }

      // Used to watch for changes to fields filled with generated passwords.
      case "input": {
        LoginManagerContent._maybeStopTreatingAsGeneratedPasswordField(aEvent);
        break;
      }

      case "keydown": {
        if (
          aEvent.keyCode == aEvent.DOM_VK_TAB ||
          aEvent.keyCode == aEvent.DOM_VK_RETURN
        ) {
          LoginManagerContent.onUsernameAutocompleted(aEvent.target);
        }
        break;
      }

      case "focus": {
        if (aEvent.target.type == "password") {
          // Used to unmask fields with filled generated passwords when focused.
          let unmask = true;
          LoginManagerContent._togglePasswordFieldMasking(
            aEvent.target,
            unmask
          );
          break;
        }

        // Only used for username fields.
        LoginManagerContent._onUsernameFocus(aEvent);
        break;
      }

      case "mousedown": {
        if (aEvent.button == 2) {
          // Date.now() is used instead of event.timeStamp since
          // dom.event.highrestimestamp.enabled isn't true on all channels yet.
          gLastRightClickTimeStamp = Date.now();
        }

        break;
      }

      default: {
        throw new Error("Unexpected event");
      }
    }
  },
};

// Add this observer once for the process.
Services.obs.addObserver(observer, "autocomplete-did-enter-text");

// This object maps to the "child" process (even in the single-process case).
this.LoginManagerContent = {
  __formFillService: null, // FormFillController, for username autocompleting
  get _formFillService() {
    if (!this.__formFillService) {
      this.__formFillService = Cc[
        "@mozilla.org/satchel/form-fill-controller;1"
      ].getService(Ci.nsIFormFillController);
    }
    return this.__formFillService;
  },

  _getRandomId() {
    return Cc["@mozilla.org/uuid-generator;1"]
      .getService(Ci.nsIUUIDGenerator)
      .generateUUID()
      .toString();
  },

  _messages: [
    "PasswordManager:loginsFound",
    "PasswordManager:loginsAutoCompleted",
  ],

  /**
   * WeakMap of the root element of a LoginForm to the DeferredTask to fill its fields.
   *
   * This is used to be able to throttle fills for a LoginForm since onDOMInputPasswordAdded gets
   * dispatched for each password field added to a document but we only want to fill once per
   * LoginForm when multiple fields are added at once.
   *
   * @type {WeakMap}
   */
  _deferredPasswordAddedTasksByRootElement: new WeakMap(),

  /**
   * WeakMap of a document to the array of callbacks to execute when it becomes visible
   *
   * This is used to defer handling DOMFormHasPassword and onDOMInputPasswordAdded events when the
   * containing document is hidden.
   * When the document first becomes visible, any queued events will be handled as normal.
   *
   * @type {WeakMap}
   */
  _onVisibleTasksByDocument: new WeakMap(),

  // Map from form login requests to information about that request.
  _requests: new Map(),

  // Number of outstanding requests to each manager.
  _managers: new Map(),

  // Input element on which enter keydown event was fired.
  _keyDownEnterForInput: null,

  _takeRequest(msg) {
    let data = msg.data;
    let request = this._requests.get(data.requestId);

    this._requests.delete(data.requestId);

    let count = this._managers.get(msg.target);
    if (--count === 0) {
      this._managers.delete(msg.target);

      for (let message of this._messages) {
        msg.target.removeMessageListener(message, this);
      }
    } else {
      this._managers.set(msg.target, count);
    }

    return request;
  },

  _sendRequest(messageManager, requestData, name, messageData) {
    let count;
    if (!(count = this._managers.get(messageManager))) {
      this._managers.set(messageManager, 1);

      for (let message of this._messages) {
        messageManager.addMessageListener(message, this);
      }
    } else {
      this._managers.set(messageManager, ++count);
    }

    let requestId = this._getRandomId();
    messageData.requestId = requestId;

    messageManager.sendAsyncMessage(name, messageData);

    let deferred = PromiseUtils.defer();
    requestData.promise = deferred;
    this._requests.set(requestId, requestData);
    return deferred.promise;
  },

  _onKeyDown(event) {
    let focusedElement = LoginManagerContent._formFillService.focusedInput;
    if (
      event.keyCode != event.DOM_VK_RETURN ||
      focusedElement != event.target
    ) {
      this._keyDownEnterForInput = null;
      return;
    }
    LoginManagerContent._keyDownEnterForInput = focusedElement;
  },

  _onPopupClosed(selectedRowStyle, mm) {
    let focusedElement = LoginManagerContent._formFillService.focusedInput;
    let eventTarget = LoginManagerContent._keyDownEnterForInput;
    if (
      !eventTarget ||
      eventTarget !== focusedElement ||
      selectedRowStyle != "loginsFooter"
    ) {
      this._keyDownEnterForInput = null;
      return;
    }
    let hostname = eventTarget.ownerDocument.documentURIObject.host;
    mm.sendAsyncMessage("PasswordManager:OpenPreferences", {
      hostname,
      entryPoint: "autocomplete",
    });
  },

  receiveMessage(msg, topWindow) {
    if (msg.name == "PasswordManager:fillForm") {
      this.fillForm({
        topDocument: topWindow.document,
        loginFormOrigin: msg.data.loginFormOrigin,
        loginsFound: LoginHelper.vanillaObjectsToLogins(msg.data.logins),
        recipes: msg.data.recipes,
        inputElementIdentifier: msg.data.inputElementIdentifier,
      });
      return;
    }

    switch (msg.name) {
      case "PasswordManager:loginsFound": {
        let loginsFound = LoginHelper.vanillaObjectsToLogins(msg.data.logins);
        let request = this._takeRequest(msg);
        request.promise.resolve({
          form: request.form,
          loginsFound,
          recipes: msg.data.recipes,
        });
        break;
      }

      case "PasswordManager:loginsAutoCompleted": {
        let loginsFound = LoginHelper.vanillaObjectsToLogins(msg.data.logins);
        let messageManager = msg.target;
        let request = this._takeRequest(msg);
        request.promise.resolve({
          generatedPassword: msg.data.generatedPassword,
          logins: loginsFound,
          messageManager,
        });
        break;
      }

      case "PasswordManager:fillGeneratedPassword": {
        // make a login for the password
        let generatedLogin = Cc[
          "@mozilla.org/login-manager/loginInfo;1"
        ].createInstance(Ci.nsILoginInfo);
        generatedLogin.init(
          msg.data.origin,
          "", // empty formActionOrigin
          null, // no realm
          "", // empty username
          msg.data.password
        );
        this.fillForm({
          topDocument: topWindow.document,
          loginFormOrigin: msg.data.origin,
          loginsFound: [generatedLogin],
          recipes: msg.data.recipes,
          inputElementIdentifier: msg.data.inputElementIdentifier,
        });
        let inputElement = ContentDOMReference.resolve(
          msg.data.inputElementIdentifier
        );
        if (inputElement) {
          this._generatedPasswordFilledOrEdited(inputElement);
        } else {
          log("Could not resolve inputElementIdentifier to a living element.");
        }
        break;
      }

      case "FormAutoComplete:PopupOpened": {
        let { chromeEventHandler } = msg.target.docShell;
        chromeEventHandler.addEventListener("keydown", this._onKeyDown, true);
        break;
      }

      case "FormAutoComplete:PopupClosed": {
        this._onPopupClosed(msg.data.selectedRowStyle, msg.target);
        let { chromeEventHandler } = msg.target.docShell;
        chromeEventHandler.removeEventListener(
          "keydown",
          this._onKeyDown,
          true
        );
        break;
      }
    }
  },

  /**
   * Get relevant logins and recipes from the parent
   *
   * @param {HTMLFormElement} form - form to get login data for
   * @param {Object} options
   * @param {boolean} options.guid - guid of a login to retrieve
   * @param {boolean} options.showMasterPassword - whether to show a master password prompt
   */
  _getLoginDataFromParent(form, options) {
    let doc = form.ownerDocument;
    let win = doc.defaultView;

    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    if (!formOrigin) {
      return Promise.reject(
        "_getLoginDataFromParent: A form origin is required"
      );
    }
    let actionOrigin = LoginHelper.getFormActionOrigin(form);

    let messageManager = win.docShell.messageManager;

    // XXX Weak??
    let requestData = { form };
    let messageData = { formOrigin, actionOrigin, options };

    return this._sendRequest(
      messageManager,
      requestData,
      "PasswordManager:findLogins",
      messageData
    );
  },

  _autoCompleteSearchAsync(aSearchString, aPreviousResult, aElement) {
    let doc = aElement.ownerDocument;
    let form = LoginFormFactory.createFromField(aElement);
    let win = doc.defaultView;

    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    let actionOrigin = LoginHelper.getFormActionOrigin(form);
    let autocompleteInfo = aElement.getAutocompleteInfo();

    let messageManager = win.docShell.messageManager;

    let previousResult = aPreviousResult
      ? {
          searchString: aPreviousResult.searchString,
          logins: LoginHelper.loginsToVanillaObjects(aPreviousResult.logins),
        }
      : null;

    let requestData = {};
    let messageData = {
      autocompleteInfo,
      browsingContextId: win.docShell.browsingContext.id,
      formOrigin,
      actionOrigin,
      searchString: aSearchString,
      previousResult,
      isSecure: InsecurePasswordUtils.isFormSecure(form),
      isPasswordField: aElement.type == "password",
    };

    if (LoginHelper.showAutoCompleteFooter) {
      messageManager.addMessageListener("FormAutoComplete:PopupOpened", this);
      messageManager.addMessageListener("FormAutoComplete:PopupClosed", this);
    }

    return this._sendRequest(
      messageManager,
      requestData,
      "PasswordManager:autoCompleteLogins",
      messageData
    );
  },

  setupEventListeners(global) {
    global.addEventListener("pageshow", event => {
      this.onPageShow(event);
    });
  },

  setupProgressListener(window) {
    if (!LoginHelper.formlessCaptureEnabled) {
      return;
    }

    try {
      let webProgress = window.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebProgress);
      webProgress.addProgressListener(
        observer,
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT |
          Ci.nsIWebProgress.NOTIFY_LOCATION
      );
    } catch (ex) {
      // Ignore NS_ERROR_FAILURE if the progress listener was already added
    }
  },

  onDOMFormBeforeSubmit(event) {
    if (!event.isTrusted) {
      return;
    }

    // We're invoked before the content's |submit| event handlers, so we
    // can grab form data before it might be modified (see bug 257781).
    log("notified before form submission");
    let formLike = LoginFormFactory.createFromForm(event.target);
    LoginManagerContent._onFormSubmit(formLike);
  },

  onDocumentVisibilityChange(event) {
    if (!event.isTrusted) {
      return;
    }
    let document = event.target;
    let onVisibleTasks = this._onVisibleTasksByDocument.get(document);
    if (!onVisibleTasks) {
      return;
    }
    for (let task of onVisibleTasks) {
      log("onDocumentVisibilityChange, executing queued task");
      task();
    }
    this._onVisibleTasksByDocument.delete(document);
  },

  _deferHandlingEventUntilDocumentVisible(event, document, fn) {
    log(
      `document.visibilityState: ${document.visibilityState}, defer handling ${
        event.type
      }`
    );
    let onVisibleTasks = this._onVisibleTasksByDocument.get(document);
    if (!onVisibleTasks) {
      log(
        `deferHandling, first queued event, register the visibilitychange handler`
      );
      onVisibleTasks = [];
      this._onVisibleTasksByDocument.set(document, onVisibleTasks);
      document.addEventListener(
        "visibilitychange",
        event => {
          this.onDocumentVisibilityChange(event);
        },
        { once: true }
      );
    }
    onVisibleTasks.push(fn);
  },

  onDOMFormHasPassword(event) {
    if (!event.isTrusted) {
      return;
    }
    let isMasterPasswordSet = Services.cpmm.sharedData.get(
      "isMasterPasswordSet"
    );
    let document = event.target.ownerDocument;

    // don't attempt to defer handling when a master password is set
    // Showing the MP modal as soon as possible minimizes its interference with tab interactions
    // See bug 1539091 and bug 1538460.
    log(
      "onDOMFormHasPassword, visibilityState:",
      document.visibilityState,
      "isMasterPasswordSet:",
      isMasterPasswordSet
    );
    if (document.visibilityState == "visible" || isMasterPasswordSet) {
      this._processDOMFormHasPasswordEvent(event);
    } else {
      // wait until the document becomes visible before handling this event
      this._deferHandlingEventUntilDocumentVisible(event, document, () => {
        this._processDOMFormHasPasswordEvent(event);
      });
    }
  },

  _processDOMFormHasPasswordEvent(event) {
    let form = event.target;
    let formLike = LoginFormFactory.createFromForm(form);
    log("_processDOMFormHasPasswordEvent:", form, formLike);
    this._fetchLoginsFromParentAndFillForm(formLike);
  },

  onDOMInputPasswordAdded(event, topWindow) {
    if (!event.isTrusted) {
      return;
    }

    this.setupProgressListener(topWindow);

    let pwField = event.originalTarget;
    if (pwField.form) {
      // Fill is handled by onDOMFormHasPassword which is already throttled.
      return;
    }

    let document = pwField.ownerDocument;
    let isMasterPasswordSet = Services.cpmm.sharedData.get(
      "isMasterPasswordSet"
    );
    log(
      "onDOMInputPasswordAdded, visibilityState:",
      document.visibilityState,
      "isMasterPasswordSet:",
      isMasterPasswordSet
    );

    // don't attempt to defer handling when a master password is set
    // Showing the MP modal as soon as possible minimizes its interference with tab interactions
    // See bug 1539091 and bug 1538460.
    if (document.visibilityState == "visible" || isMasterPasswordSet) {
      this._processDOMInputPasswordAddedEvent(event, topWindow);
    } else {
      // wait until the document becomes visible before handling this event
      this._deferHandlingEventUntilDocumentVisible(event, document, () => {
        this._processDOMInputPasswordAddedEvent(event, topWindow);
      });
    }
  },

  _processDOMInputPasswordAddedEvent(event, topWindow) {
    let pwField = event.originalTarget;

    let formLike = LoginFormFactory.createFromField(pwField);
    log(" _processDOMInputPasswordAddedEvent:", pwField, formLike);

    let deferredTask = this._deferredPasswordAddedTasksByRootElement.get(
      formLike.rootElement
    );
    if (!deferredTask) {
      log(
        "Creating a DeferredTask to call _fetchLoginsFromParentAndFillForm soon"
      );
      LoginFormFactory.setForRootElement(formLike.rootElement, formLike);

      deferredTask = new DeferredTask(
        () => {
          // Get the updated LoginForm instead of the one at the time of creating the DeferredTask via
          // a closure since it could be stale since LoginForm.elements isn't live.
          let formLike2 = LoginFormFactory.getForRootElement(
            formLike.rootElement
          );
          log(
            "Running deferred processing of onDOMInputPasswordAdded",
            formLike2
          );
          this._deferredPasswordAddedTasksByRootElement.delete(
            formLike2.rootElement
          );
          this._fetchLoginsFromParentAndFillForm(formLike2);
        },
        PASSWORD_INPUT_ADDED_COALESCING_THRESHOLD_MS,
        0
      );

      this._deferredPasswordAddedTasksByRootElement.set(
        formLike.rootElement,
        deferredTask
      );
    }

    let window = pwField.ownerGlobal;
    if (deferredTask.isArmed) {
      log("DeferredTask is already armed so just updating the LoginForm");
      // We update the LoginForm so it (most important .elements) is fresh when the task eventually
      // runs since changes to the elements could affect our field heuristics.
      LoginFormFactory.setForRootElement(formLike.rootElement, formLike);
    } else if (window.document.readyState == "complete") {
      log(
        "Arming the DeferredTask we just created since document.readyState == 'complete'"
      );
      deferredTask.arm();
    } else {
      window.addEventListener(
        "DOMContentLoaded",
        function() {
          log(
            "Arming the onDOMInputPasswordAdded DeferredTask due to DOMContentLoaded"
          );
          deferredTask.arm();
        },
        { once: true }
      );
    }
  },

  /**
   * Fetch logins from the parent for a given form and then attempt to fill it.
   *
   * @param {LoginForm} form to fetch the logins for then try autofill.
   */
  _fetchLoginsFromParentAndFillForm(form) {
    let window = form.ownerDocument.defaultView;
    this._detectInsecureFormLikes(window.top);

    let messageManager = window.docShell.messageManager;
    messageManager.sendAsyncMessage("LoginStats:LoginEncountered");

    if (!LoginHelper.enabled) {
      return;
    }

    this._getLoginDataFromParent(form, { showMasterPassword: true })
      .then(this.loginsFound.bind(this))
      .catch(Cu.reportError);
  },

  onPageShow(event) {
    let window = event.target.ownerGlobal;
    this._detectInsecureFormLikes(window);
  },

  /**
   * Maps all DOM content documents in this content process, including those in
   * frames, to the current state used by the Login Manager.
   */
  loginFormStateByDocument: new WeakMap(),

  /**
   * Retrieves a reference to the state object associated with the given
   * document. This is initialized to an object with default values.
   */
  stateForDocument(document) {
    let loginFormState = this.loginFormStateByDocument.get(document);
    if (!loginFormState) {
      loginFormState = {
        /**
         * Keeps track of filled fields and values.
         */
        fillsByRootElement: new WeakMap(),
        /**
         * Keeps track of logins that were last submitted.
         */
        lastSubmittedValuesByRootElement: new WeakMap(),
        loginFormRootElements: new WeakSet(),
      };
      this.loginFormStateByDocument.set(document, loginFormState);
    }
    return loginFormState;
  },

  /**
   * Compute whether there is an insecure login form on any frame of the current page, and
   * notify the parent process. This is used to control whether insecure password UI appears.
   */
  _detectInsecureFormLikes(topWindow) {
    log("_detectInsecureFormLikes", topWindow.location.href);

    // Returns true if this window or any subframes have insecure login forms.
    let hasInsecureLoginForms = thisWindow => {
      let doc = thisWindow.document;
      let rootElsWeakSet = LoginFormFactory.getRootElementsWeakSetForDocument(
        doc
      );
      let hasLoginForm =
        ChromeUtils.nondeterministicGetWeakSetKeys(rootElsWeakSet).filter(
          el => el.isConnected
        ).length > 0;
      return (
        (hasLoginForm && !thisWindow.isSecureContext) ||
        Array.prototype.some.call(thisWindow.frames, frame =>
          hasInsecureLoginForms(frame)
        )
      );
    };

    let messageManager = topWindow.docShell.messageManager;
    messageManager.sendAsyncMessage(
      "PasswordManager:insecureLoginFormPresent",
      {
        hasInsecureLoginForms: hasInsecureLoginForms(topWindow),
      }
    );
  },

  /**
   * Perform a password fill upon user request coming from the parent process.
   * The fill will be in the form previously identified during page navigation.
   *
   * @param An object with the following properties:
   *        {
   *          topDocument:
   *            DOM document currently associated to the the top-level window
   *            for which the fill is requested. This may be different from the
   *            document that originally caused the login UI to be displayed.
   *          loginFormOrigin:
   *            String with the origin for which the login UI was displayed.
   *            This must match the origin of the form used for the fill.
   *          loginsFound:
   *            Array containing the login to fill. While other messages may
   *            have more logins, for this use case this is expected to have
   *            exactly one element. The origin of the login may be different
   *            from the origin of the form used for the fill.
   *          recipes:
   *            Fill recipes transmitted together with the original message.
   *          inputElementIdentifier:
   *            An identifier generated for the input element via ContentDOMReference.
   *        }
   */
  fillForm({
    topDocument,
    loginFormOrigin,
    loginsFound,
    recipes,
    inputElementIdentifier,
  }) {
    if (!inputElementIdentifier) {
      log("fillForm: No input element specified");
      return;
    }

    let inputElement = ContentDOMReference.resolve(inputElementIdentifier);
    if (!inputElement) {
      log(
        "fillForm: Could not resolve inputElementIdentifier to a living element."
      );
      return;
    }

    if (
      LoginHelper.getLoginOrigin(topDocument.documentURI) != loginFormOrigin
    ) {
      if (
        !inputElement ||
        LoginHelper.getLoginOrigin(inputElement.ownerDocument.documentURI) !=
          loginFormOrigin
      ) {
        log(
          "fillForm: The requested origin doesn't match the one from the",
          "document. This may mean we navigated to a document from a different",
          "site before we had a chance to indicate this change in the user",
          "interface."
        );
        return;
      }
    }

    let clobberUsername = true;
    let form = LoginFormFactory.createFromField(inputElement);
    if (inputElement.type == "password") {
      clobberUsername = false;
    }

    this._fillForm(form, loginsFound, recipes, {
      inputElement,
      autofillForm: true,
      clobberUsername,
      clobberPassword: true,
      userTriggered: true,
    });
  },

  loginsFound({ form, loginsFound, recipes }) {
    let doc = form.ownerDocument;
    let autofillForm =
      LoginHelper.autofillForms &&
      !PrivateBrowsingUtils.isContentWindowPrivate(doc.defaultView);

    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    LoginRecipesContent.cacheRecipes(formOrigin, doc.defaultView, recipes);

    this._fillForm(form, loginsFound, recipes, { autofillForm });
  },

  /**
   * Focus event handler for username fields to decide whether to show autocomplete.
   * @param {FocusEvent} event
   */
  _onUsernameFocus(event) {
    let focusedField = event.target;
    if (!focusedField.mozIsTextField(true) || focusedField.readOnly) {
      return;
    }

    if (this._isLoginAlreadyFilled(focusedField)) {
      log("_onUsernameFocus: Already filled");
      return;
    }

    /*
     * A `mousedown` event is fired before the `focus` event if the user right clicks into an
     * unfocused field. In that case we don't want to show both autocomplete and a context menu
     * overlapping so we check against the timestamp that was set by the `mousedown` event if the
     * button code indicated a right click.
     * We use a timestamp instead of a bool to avoid complexity when dealing with multiple input
     * forms and the fact that a mousedown into an already focused field does not trigger another focus.
     * Date.now() is used instead of event.timeStamp since dom.event.highrestimestamp.enabled isn't
     * true on all channels yet.
     */
    let timeDiff = Date.now() - gLastRightClickTimeStamp;
    if (timeDiff < AUTOCOMPLETE_AFTER_RIGHT_CLICK_THRESHOLD_MS) {
      log(
        "Not opening autocomplete after focus since a context menu was opened within",
        timeDiff,
        "ms"
      );
      return;
    }

    log("maybeOpenAutocompleteAfterFocus: Opening the autocomplete popup");
    this._formFillService.showPopup();
  },

  /**
   * A username or password was autocompleted into a field.
   */
  onFieldAutoComplete(acInputField, loginGUID) {
    if (!LoginHelper.enabled) {
      return;
    }

    // This is probably a bit over-conservatative.
    if (
      ChromeUtils.getClassName(acInputField.ownerDocument) != "HTMLDocument"
    ) {
      return;
    }

    if (!LoginFormFactory.createFromField(acInputField)) {
      return;
    }

    if (LoginHelper.isUsernameFieldType(acInputField)) {
      this.onUsernameAutocompleted(acInputField, loginGUID);
    } else if (acInputField.hasBeenTypePassword) {
      // Ensure the field gets re-masked and edits don't overwrite the generated
      // password in case a generated password was filled into it previously.
      this._stopTreatingAsGeneratedPasswordField(acInputField);
      this._highlightFilledField(acInputField);
    }
  },

  /**
   * A username field was filled or tabbed away from so try fill in the
   * associated password in the password field.
   */
  onUsernameAutocompleted(acInputField, loginGUID = null) {
    log("onUsernameAutocompleted:", acInputField);

    let acForm = LoginFormFactory.createFromField(acInputField);
    let doc = acForm.ownerDocument;
    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    let recipes = LoginRecipesContent.getRecipes(formOrigin, doc.defaultView);

    // Make sure the username field fillForm will use is the
    // same field as the autocomplete was activated on.
    let [usernameField, passwordField, ignored] = this._getFormFields(
      acForm,
      false,
      recipes
    );
    if (usernameField == acInputField && passwordField) {
      this._getLoginDataFromParent(acForm, {
        guid: loginGUID,
        showMasterPassword: false,
      })
        .then(({ form, loginsFound, recipes }) => {
          this._fillForm(form, loginsFound, recipes, {
            autofillForm: true,
            clobberPassword: true,
            userTriggered: true,
          });
        })
        .catch(Cu.reportError);
    } else {
      // Ignore the event, it's for some input we don't care about.
    }
  },

  /**
   * @param {LoginForm} form - the LoginForm to look for password fields in.
   * @param {Object} options
   * @param {bool} [options.skipEmptyFields=false] - Whether to ignore password fields with no value.
   *                                                 Used at capture time since saving empty values isn't
   *                                                 useful.
   * @param {Object} [options.fieldOverrideRecipe=null] - A relevant field override recipe to use.
   * @return {Array|null} Array of password field elements for the specified form.
   *                      If no pw fields are found, or if more than 3 are found, then null
   *                      is returned.
   */
  _getPasswordFields(
    form,
    { fieldOverrideRecipe = null, minPasswordLength = 0 } = {}
  ) {
    // Locate the password fields in the form.
    let pwFields = [];
    for (let i = 0; i < form.elements.length; i++) {
      let element = form.elements[i];
      if (
        ChromeUtils.getClassName(element) !== "HTMLInputElement" ||
        element.type != "password" ||
        !element.isConnected
      ) {
        continue;
      }

      // Exclude ones matching a `notPasswordSelector`, if specified.
      if (
        fieldOverrideRecipe &&
        fieldOverrideRecipe.notPasswordSelector &&
        element.matches(fieldOverrideRecipe.notPasswordSelector)
      ) {
        log(
          "skipping password field (id/name is",
          element.id,
          " / ",
          element.name + ") due to recipe:",
          fieldOverrideRecipe
        );
        continue;
      }

      // XXX: Bug 780449 tracks our handling of emoji and multi-code-point characters in
      // password fields. To avoid surprises, we should be consistent with the visual
      // representation of the masked password
      if (
        minPasswordLength &&
        element.value.trim().length < minPasswordLength
      ) {
        log(
          "skipping password field (id/name is",
          element.id,
          " / ",
          element.name + ") as value is too short:",
          element.value.trim().length
        );
        continue; // Ignore empty or too-short passwords fields
      }

      pwFields[pwFields.length] = {
        index: i,
        element,
      };
    }

    // If too few or too many fields, bail out.
    if (pwFields.length == 0) {
      log("(form ignored -- no password fields.)");
      return null;
    } else if (pwFields.length > 3) {
      log(
        "(form ignored -- too many password fields. [ got ",
        pwFields.length,
        "])"
      );
      return null;
    }

    return pwFields;
  },

  /**
   * Returns the username and password fields found in the form.
   * Can handle complex forms by trying to figure out what the
   * relevant fields are.
   *
   * @param {LoginForm} form
   * @param {bool} isSubmission
   * @param {Set} recipes
   * @return {Array} [usernameField, newPasswordField, oldPasswordField]
   *
   * usernameField may be null.
   * newPasswordField will always be non-null.
   * oldPasswordField may be null. If null, newPasswordField is just
   * "theLoginField". If not null, the form is apparently a
   * change-password field, with oldPasswordField containing the password
   * that is being changed.
   *
   * Note that even though we can create a LoginForm from a text field,
   * this method will only return a non-null usernameField if the
   * LoginForm has a password field.
   */
  _getFormFields(form, isSubmission, recipes) {
    let usernameField = null;
    let pwFields = null;
    let fieldOverrideRecipe = LoginRecipesContent.getFieldOverrides(
      recipes,
      form
    );
    if (fieldOverrideRecipe) {
      let pwOverrideField = LoginRecipesContent.queryLoginField(
        form,
        fieldOverrideRecipe.passwordSelector
      );
      if (pwOverrideField) {
        // The field from the password override may be in a different LoginForm.
        let formLike = LoginFormFactory.createFromField(pwOverrideField);
        pwFields = [
          {
            index: [...formLike.elements].indexOf(pwOverrideField),
            element: pwOverrideField,
          },
        ];
      }

      let usernameOverrideField = LoginRecipesContent.queryLoginField(
        form,
        fieldOverrideRecipe.usernameSelector
      );
      if (usernameOverrideField) {
        usernameField = usernameOverrideField;
      }
    }

    if (!pwFields) {
      // Locate the password field(s) in the form. Up to 3 supported.
      // If there's no password field, there's nothing for us to do.
      const minSubmitPasswordLength = 2;
      pwFields = this._getPasswordFields(form, {
        fieldOverrideRecipe,
        minPasswordLength: isSubmission ? minSubmitPasswordLength : 0,
      });
    }

    if (!pwFields) {
      return [null, null, null];
    }

    if (!usernameField) {
      // Locate the username field in the form by searching backwards
      // from the first password field, assume the first text field is the
      // username. We might not find a username field if the user is
      // already logged in to the site.

      for (let i = pwFields[0].index - 1; i >= 0; i--) {
        let element = form.elements[i];
        if (!LoginHelper.isUsernameFieldType(element)) {
          continue;
        }

        if (
          fieldOverrideRecipe &&
          fieldOverrideRecipe.notUsernameSelector &&
          element.matches(fieldOverrideRecipe.notUsernameSelector)
        ) {
          continue;
        }

        usernameField = element;
        break;
      }
    }

    if (!usernameField) {
      log("(form -- no username field found)");
    } else {
      let acFieldName = usernameField.getAutocompleteInfo().fieldName;
      log(
        "Username field ",
        usernameField,
        "has name/value/autocomplete:",
        usernameField.name,
        "/",
        usernameField.value,
        "/",
        acFieldName
      );
    }
    // If we're not submitting a form (it's a page load), there are no
    // password field values for us to use for identifying fields. So,
    // just assume the first password field is the one to be filled in.
    if (!isSubmission || pwFields.length == 1) {
      let passwordField = pwFields[0].element;
      log("Password field", passwordField, "has name: ", passwordField.name);
      return [usernameField, passwordField, null];
    }

    // Try to figure out WTF is in the form based on the password values.
    let oldPasswordField, newPasswordField;
    let pw1 = pwFields[0].element.value;
    let pw2 = pwFields[1].element.value;
    let pw3 = pwFields[2] ? pwFields[2].element.value : null;

    if (pwFields.length == 3) {
      // Look for two identical passwords, that's the new password

      if (pw1 == pw2 && pw2 == pw3) {
        // All 3 passwords the same? Weird! Treat as if 1 pw field.
        newPasswordField = pwFields[0].element;
        oldPasswordField = null;
      } else if (pw1 == pw2) {
        newPasswordField = pwFields[0].element;
        oldPasswordField = pwFields[2].element;
      } else if (pw2 == pw3) {
        oldPasswordField = pwFields[0].element;
        newPasswordField = pwFields[2].element;
      } else if (pw1 == pw3) {
        // A bit odd, but could make sense with the right page layout.
        newPasswordField = pwFields[0].element;
        oldPasswordField = pwFields[1].element;
      } else {
        // We can't tell which of the 3 passwords should be saved.
        log("(form ignored -- all 3 pw fields differ)");
        return [null, null, null];
      }
    } else if (pw1 == pw2) {
      // pwFields.length == 2
      // Treat as if 1 pw field
      newPasswordField = pwFields[0].element;
      oldPasswordField = null;
    } else {
      // Just assume that the 2nd password is the new password
      oldPasswordField = pwFields[0].element;
      newPasswordField = pwFields[1].element;
    }

    log(
      "Password field (new) id/name is: ",
      newPasswordField.id,
      " / ",
      newPasswordField.name
    );
    if (oldPasswordField) {
      log(
        "Password field (old) id/name is: ",
        oldPasswordField.id,
        " / ",
        oldPasswordField.name
      );
    } else {
      log("Password field (old):", oldPasswordField);
    }
    return [usernameField, newPasswordField, oldPasswordField];
  },

  /**
   * @return true if the page requests autocomplete be disabled for the
   *              specified element.
   */
  _isAutocompleteDisabled(element) {
    return element && element.autocomplete == "off";
  },

  /**
   * Fill a page that was restored from bfcache since we wouldn't receive
   * DOMInputPasswordAdded or DOMFormHasPassword events for it.
   * @param {Document} aDocument that was restored from bfcache.
   */
  _onDocumentRestored(aDocument) {
    let rootElsWeakSet = LoginFormFactory.getRootElementsWeakSetForDocument(
      aDocument
    );
    let weakLoginFormRootElements = ChromeUtils.nondeterministicGetWeakSetKeys(
      rootElsWeakSet
    );

    log(
      "_onDocumentRestored: loginFormRootElements approx size:",
      weakLoginFormRootElements.length,
      "document:",
      aDocument
    );

    for (let formRoot of weakLoginFormRootElements) {
      if (!formRoot.isConnected) {
        continue;
      }

      let formLike = LoginFormFactory.getForRootElement(formRoot);
      this._fetchLoginsFromParentAndFillForm(formLike);
    }
  },

  /**
   * Trigger capture on any relevant FormLikes due to a navigation alone (not
   * necessarily due to an actual form submission). This method is used to
   * capture logins for cases where form submit events are not used.
   *
   * To avoid multiple notifications for the same LoginForm, this currently
   * avoids capturing when dealing with a real <form> which are ideally already
   * using a submit event.
   *
   * @param {Document} document being navigated
   */
  _onNavigation(aDocument) {
    let rootElsWeakSet = LoginFormFactory.getRootElementsWeakSetForDocument(
      aDocument
    );
    let weakLoginFormRootElements = ChromeUtils.nondeterministicGetWeakSetKeys(
      rootElsWeakSet
    );

    log(
      "_onNavigation: root elements approx size:",
      weakLoginFormRootElements.length,
      "document:",
      aDocument
    );

    for (let formRoot of weakLoginFormRootElements) {
      if (!formRoot.isConnected) {
        continue;
      }

      let formLike = LoginFormFactory.getForRootElement(formRoot);
      this._onFormSubmit(formLike);
    }
  },

  /**
   * Called by our observer when notified of a form submission.
   * [Note that this happens before any DOM onsubmit handlers are invoked.]
   * Looks for a password change in the submitted form, so we can update
   * our stored password.
   *
   * @param {LoginForm} form
   */
  _onFormSubmit(form) {
    log("_onFormSubmit", form);
    let doc = form.ownerDocument;
    let win = doc.defaultView;

    if (
      PrivateBrowsingUtils.isContentWindowPrivate(win) &&
      !LoginHelper.privateBrowsingCaptureEnabled
    ) {
      // We won't do anything in private browsing mode anyway,
      // so there's no need to perform further checks.
      log("(form submission ignored in private browsing mode)");
      return;
    }

    // If password saving is disabled globally, bail out now.
    if (!LoginHelper.enabled) {
      return;
    }

    let origin = LoginHelper.getLoginOrigin(doc.documentURI);
    if (!origin) {
      log("(form submission ignored -- invalid origin)");
      return;
    }

    let formActionOrigin = LoginHelper.getFormActionOrigin(form);
    let messageManager = win.docShell.messageManager;

    let recipes = LoginRecipesContent.getRecipes(origin, win);

    // Get the appropriate fields from the form.
    let [
      usernameField,
      newPasswordField,
      oldPasswordField,
    ] = this._getFormFields(form, true, recipes);

    // Need at least 1 valid password field to do anything.
    if (newPasswordField == null) {
      return;
    }

    if (usernameField && usernameField.value.match(/[•\*]{3,}/)) {
      log(
        `usernameField.value "${
          usernameField.value
        }" looks munged, setting to null`
      );
      usernameField = null;
    }

    // Check for autocomplete=off attribute. We don't use it to prevent
    // autofilling (for existing logins), but won't save logins when it's
    // present and the storeWhenAutocompleteOff pref is false.
    // XXX spin out a bug that we don't update timeLastUsed in this case?
    if (
      (this._isAutocompleteDisabled(form) ||
        this._isAutocompleteDisabled(usernameField) ||
        this._isAutocompleteDisabled(newPasswordField) ||
        this._isAutocompleteDisabled(oldPasswordField)) &&
      !LoginHelper.storeWhenAutocompleteOff
    ) {
      log("(form submission ignored -- autocomplete=off found)");
      return;
    }

    // Don't try to send DOM nodes over IPC.
    let mockUsername = usernameField
      ? { name: usernameField.name, value: usernameField.value }
      : null;
    let mockPassword = {
      name: newPasswordField.name,
      value: newPasswordField.value,
    };
    let mockOldPassword = oldPasswordField
      ? { name: oldPasswordField.name, value: oldPasswordField.value }
      : null;

    let usernameValue = usernameField ? usernameField.value : null;
    let formLikeRoot = FormLikeFactory.findRootForField(newPasswordField);
    let state = this.stateForDocument(doc);
    let lastSubmittedValues = state.lastSubmittedValuesByRootElement.get(
      formLikeRoot
    );
    if (lastSubmittedValues) {
      if (
        lastSubmittedValues.username == usernameValue &&
        lastSubmittedValues.password == newPasswordField.value
      ) {
        log(
          "(form submission ignored -- already submitted with the same username and password)"
        );
        return;
      }
    }

    // Save the last submitted values so we don't prompt twice for the same values using
    // different capture methods e.g. a form submit event and upon navigation.
    state.lastSubmittedValuesByRootElement.set(formLikeRoot, {
      username: usernameValue,
      password: newPasswordField.value,
    });

    // Make sure to pass the opener's top ID in case it was in a frame.
    let openerTopWindowID = null;
    if (win.opener) {
      openerTopWindowID = win.opener.top.windowUtils.outerWindowID;
    }

    // Dismiss prompt if the username field is a credit card number AND
    // if the password field is a three digit number. Also dismiss prompt if
    // the password is a credit card number and the password field has attribute
    // autocomplete="cc-number".
    let dismissedPrompt = false;
    let newPasswordFieldValue = newPasswordField.value;
    if (
      (CreditCard.isValidNumber(usernameValue) &&
        newPasswordFieldValue.trim().match(/^[0-9]{3}$/)) ||
      (CreditCard.isValidNumber(newPasswordFieldValue) &&
        newPasswordField.getAutocompleteInfo().fieldName == "cc-number")
    ) {
      dismissedPrompt = true;
    }

    let autoFilledLogin = this.stateForDocument(doc).fillsByRootElement.get(
      form.rootElement
    );
    messageManager.sendAsyncMessage("PasswordManager:onFormSubmit", {
      origin,
      formActionOrigin,
      autoFilledLoginGuid: autoFilledLogin && autoFilledLogin.guid,
      usernameField: mockUsername,
      newPasswordField: mockPassword,
      oldPasswordField: mockOldPassword,
      openerTopWindowID,
      dismissedPrompt,
    });
  },

  _maybeStopTreatingAsGeneratedPasswordField(event) {
    let passwordField = event.target;
    let { value } = passwordField;

    // If the field is now empty or the inserted text replaced the whole value
    // then stop treating it as a generated password field.
    if (!value || (event.data && event.data == value)) {
      this._stopTreatingAsGeneratedPasswordField(passwordField);
    }
  },

  _stopTreatingAsGeneratedPasswordField(passwordField) {
    log("_stopTreatingAsGeneratedPasswordField");

    // Remove all the event listeners added in _generatedPasswordFilledOrEdited
    for (let eventType of ["blur", "change", "focus", "input"]) {
      passwordField.removeEventListener(eventType, observer, {
        capture: true,
        mozSystemGroup: true,
      });
    }

    // Mask the password field
    this._togglePasswordFieldMasking(passwordField, false);
  },

  /**
   * Notify the parent that a generated password was filled into a field or
   * edited so that it can potentially be saved.
   * @param {HTMLInputElement} passwordField
   */
  _generatedPasswordFilledOrEdited(passwordField) {
    log("_generatedPasswordFilledOrEdited", passwordField);

    if (!LoginHelper.enabled) {
      throw new Error(
        "A generated password was filled while the password manager was disabled."
      );
    }

    let win = passwordField.ownerGlobal;

    this._highlightFilledField(passwordField);

    // change: Listen for changes to the field filled with the generated password so we can preserve edits.
    // input: Listen for the field getting blanked (without blurring) or a paste
    for (let eventType of ["blur", "change", "focus", "input"]) {
      passwordField.addEventListener(eventType, observer, {
        capture: true,
        mozSystemGroup: true,
      });
    }
    // Unmask the password field
    this._togglePasswordFieldMasking(passwordField, true);

    if (PrivateBrowsingUtils.isContentWindowPrivate(win)) {
      log(
        "_generatedPasswordFilledOrEdited: not automatically saving the password in private browsing mode"
      );
      return;
    }

    let loginForm = LoginFormFactory.createFromField(passwordField);
    let formActionOrigin = LoginHelper.getFormActionOrigin(loginForm);
    let origin = LoginHelper.getLoginOrigin(
      passwordField.ownerDocument.documentURI
    );
    let recipes = LoginRecipesContent.getRecipes(origin, win);
    let [usernameField] = this._getFormFields(loginForm, false, recipes);
    let openerTopWindowID = null;
    if (win.opener) {
      openerTopWindowID = win.opener.top.windowUtils.outerWindowID;
    }
    let messageManager = win.docShell.messageManager;
    messageManager.sendAsyncMessage(
      "PasswordManager:onGeneratedPasswordFilledOrEdited",
      {
        browsingContextId: win.docShell.browsingContext.id,
        formActionOrigin,
        openerTopWindowID,
        password: passwordField.value,
        username: (usernameField && usernameField.value) || "",
      }
    );
  },

  _togglePasswordFieldMasking(passwordField, unmask) {
    let { editor } = passwordField;

    if (passwordField.type != "password") {
      // The type may have been changed by the website.
      log("_togglePasswordFieldMasking: Field isn't type=password");
      return;
    }

    if (!unmask && !editor) {
      // It hasn't been created yet but the default is to be masked anyways.
      return;
    }

    if (unmask) {
      editor.unmask(0);
      return;
    }

    if (editor.autoMaskingEnabled) {
      return;
    }
    editor.mask();
  },

  /** Remove login field highlight when its value is cleared or overwritten.
   */
  _removeFillFieldHighlight(event) {
    let winUtils = event.target.ownerGlobal.windowUtils;
    winUtils.removeManuallyManagedState(event.target, AUTOFILL_STATE);
  },

  /**
   * Highlight login fields on autocomplete or autofill on page load.
   * @param {Node} element that needs highlighting.
   */
  _highlightFilledField(element) {
    let winUtils = element.ownerGlobal.windowUtils;

    winUtils.addManuallyManagedState(element, AUTOFILL_STATE);
    // Remove highlighting when the field is changed.
    element.addEventListener("input", this._removeFillFieldHighlight, {
      mozSystemGroup: true,
      once: true,
    });
  },

  /**
   * Attempt to find the username and password fields in a form, and fill them
   * in using the provided logins and recipes.
   *
   * @param {LoginForm} form
   * @param {nsILoginInfo[]} foundLogins an array of nsILoginInfo that could be
   *        used for the form, including ones with a different form action origin
   *        which are only used when the fill is userTriggered
   * @param {Set} recipes a set of recipes that could be used to affect how the
   *        form is filled
   * @param {Object} [options = {}] a list of options for this method
   * @param {HTMLInputElement} [options.inputElement = null] an optional target
   *        input element we want to fill
   * @param {bool} [options.autofillForm = false] denotes if we should fill the
   *        form in automatically
   * @param {bool} [options.clobberUsername = false] controls if an existing
   *        username can be overwritten. If this is false and an inputElement
   *        of type password is also passed, the username field will be ignored.
   *        If this is false and no inputElement is passed, if the username
   *        field value is not found in foundLogins, it will not fill the
   *        password.
   * @param {bool} [options.clobberPassword = false] controls if an existing
   *        password value can be overwritten
   * @param {bool} [options.userTriggered = false] an indication of whether
   *        this filling was triggered by the user
   */
  // eslint-disable-next-line complexity
  _fillForm(
    form,
    foundLogins,
    recipes,
    {
      inputElement = null,
      autofillForm = false,
      clobberUsername = false,
      clobberPassword = false,
      userTriggered = false,
    } = {}
  ) {
    if (ChromeUtils.getClassName(form) === "HTMLFormElement") {
      throw new Error("_fillForm should only be called with LoginForm objects");
    }

    log("_fillForm", form.elements);
    let usernameField;
    // Will be set to one of AUTOFILL_RESULT in the `try` block.
    let autofillResult = -1;
    const AUTOFILL_RESULT = {
      FILLED: 0,
      NO_PASSWORD_FIELD: 1,
      PASSWORD_DISABLED_READONLY: 2,
      NO_LOGINS_FIT: 3,
      NO_SAVED_LOGINS: 4,
      EXISTING_PASSWORD: 5,
      EXISTING_USERNAME: 6,
      MULTIPLE_LOGINS: 7,
      NO_AUTOFILL_FORMS: 8,
      AUTOCOMPLETE_OFF: 9,
      INSECURE: 10,
      PASSWORD_AUTOCOMPLETE_NEW_PASSWORD: 11,
    };

    try {
      // Nothing to do if we have no matching (excluding form action
      // checks) logins available, and there isn't a need to show
      // the insecure form warning.
      if (
        foundLogins.length == 0 &&
        (InsecurePasswordUtils.isFormSecure(form) ||
          !LoginHelper.showInsecureFieldWarning)
      ) {
        // We don't log() here since this is a very common case.
        autofillResult = AUTOFILL_RESULT.NO_SAVED_LOGINS;
        return;
      }

      // Heuristically determine what the user/pass fields are
      // We do this before checking to see if logins are stored,
      // so that the user isn't prompted for a master password
      // without need.
      let passwordField;
      [usernameField, passwordField] = this._getFormFields(
        form,
        false,
        recipes
      );

      // If we have a password inputElement parameter and it's not
      // the same as the one heuristically found, use the parameter
      // one instead.
      if (inputElement) {
        if (inputElement.type == "password") {
          passwordField = inputElement;
          if (!clobberUsername) {
            usernameField = null;
          }
        } else if (LoginHelper.isUsernameFieldType(inputElement)) {
          usernameField = inputElement;
        } else {
          throw new Error("Unexpected input element type.");
        }
      }

      // Need a valid password field to do anything.
      if (passwordField == null) {
        log("not filling form, no password field found");
        autofillResult = AUTOFILL_RESULT.NO_PASSWORD_FIELD;
        return;
      }

      // If the password field is disabled or read-only, there's nothing to do.
      if (passwordField.disabled || passwordField.readOnly) {
        log("not filling form, password field disabled or read-only");
        autofillResult = AUTOFILL_RESULT.PASSWORD_DISABLED_READONLY;
        return;
      }

      // Attach autocomplete stuff to the username field, if we have
      // one. This is normally used to select from multiple accounts,
      // but even with one account we should refill if the user edits.
      // We would also need this attached to show the insecure login
      // warning, regardless of saved login.
      if (usernameField) {
        this._formFillService.markAsLoginManagerField(usernameField);
        usernameField.addEventListener("keydown", observer);
      }

      if (!userTriggered) {
        // Only autofill logins that match the form's action and origin. In the above code
        // we have attached autocomplete for logins that don't match the form action.
        let loginOrigin = LoginHelper.getLoginOrigin(
          form.ownerDocument.documentURI
        );
        let formActionOrigin = LoginHelper.getFormActionOrigin(form);
        foundLogins = foundLogins.filter(l => {
          let formActionMatches = LoginHelper.isOriginMatching(
            l.formActionOrigin,
            formActionOrigin,
            {
              schemeUpgrades: LoginHelper.schemeUpgrades,
              acceptWildcardMatch: true,
              acceptDifferentSubdomains: false,
            }
          );
          let formOriginMatches = LoginHelper.isOriginMatching(
            l.origin,
            loginOrigin,
            {
              schemeUpgrades: LoginHelper.schemeUpgrades,
              acceptWildcardMatch: true,
              acceptDifferentSubdomains: false,
            }
          );
          return formActionMatches && formOriginMatches;
        });

        // Since the logins are already filtered now to only match the origin and formAction,
        // dedupe to just the username since remaining logins may have different schemes.
        foundLogins = LoginHelper.dedupeLogins(
          foundLogins,
          ["username"],
          ["scheme", "timePasswordChanged"],
          loginOrigin,
          formActionOrigin
        );
      }

      // Nothing to do if we have no matching logins available.
      // Only insecure pages reach this block and logs the same
      // telemetry flag.
      if (foundLogins.length == 0) {
        // We don't log() here since this is a very common case.
        autofillResult = AUTOFILL_RESULT.NO_SAVED_LOGINS;
        return;
      }

      // Prevent autofilling insecure forms.
      if (
        !userTriggered &&
        !LoginHelper.insecureAutofill &&
        !InsecurePasswordUtils.isFormSecure(form)
      ) {
        log("not filling form since it's insecure");
        autofillResult = AUTOFILL_RESULT.INSECURE;
        return;
      }

      // Discard logins which have username/password values that don't
      // fit into the fields (as specified by the maxlength attribute).
      // The user couldn't enter these values anyway, and it helps
      // with sites that have an extra PIN to be entered (bug 391514)
      let maxUsernameLen = Number.MAX_VALUE;
      let maxPasswordLen = Number.MAX_VALUE;

      // If attribute wasn't set, default is -1.
      if (usernameField && usernameField.maxLength >= 0) {
        maxUsernameLen = usernameField.maxLength;
      }
      if (passwordField.maxLength >= 0) {
        maxPasswordLen = passwordField.maxLength;
      }

      let logins = foundLogins.filter(function(l) {
        let fit =
          l.username.length <= maxUsernameLen &&
          l.password.length <= maxPasswordLen;
        if (!fit) {
          log("Ignored", l.username, "login: won't fit");
        }

        return fit;
      }, this);

      if (logins.length == 0) {
        log("form not filled, none of the logins fit in the field");
        autofillResult = AUTOFILL_RESULT.NO_LOGINS_FIT;
        return;
      }

      const passwordACFieldName = passwordField.getAutocompleteInfo().fieldName;

      // If the password field has the autocomplete value of "new-password"
      // and we're autofilling without user interaction, there's nothing to do.
      if (!userTriggered && passwordACFieldName == "new-password") {
        log(
          "not filling form, password field has the autocomplete new-password value"
        );
        autofillResult = AUTOFILL_RESULT.PASSWORD_AUTOCOMPLETE_NEW_PASSWORD;
        return;
      }

      // Don't clobber an existing password.
      if (passwordField.value && !clobberPassword) {
        log("form not filled, the password field was already filled");
        autofillResult = AUTOFILL_RESULT.EXISTING_PASSWORD;
        return;
      }

      // Select a login to use for filling in the form.
      let selectedLogin;
      if (
        !clobberUsername &&
        usernameField &&
        (usernameField.value ||
          usernameField.disabled ||
          usernameField.readOnly)
      ) {
        // If username was specified in the field, it's disabled or it's readOnly, only fill in the
        // password if we find a matching login.
        let username = usernameField.value.toLowerCase();

        let matchingLogins = logins.filter(
          l => l.username.toLowerCase() == username
        );
        if (matchingLogins.length == 0) {
          log(
            "Password not filled. None of the stored logins match the username already present."
          );
          autofillResult = AUTOFILL_RESULT.EXISTING_USERNAME;
          return;
        }

        // If there are multiple, and one matches case, use it
        for (let l of matchingLogins) {
          if (l.username == usernameField.value) {
            selectedLogin = l;
          }
        }
        // Otherwise just use the first
        if (!selectedLogin) {
          selectedLogin = matchingLogins[0];
        }
      } else if (logins.length == 1) {
        selectedLogin = logins[0];
      } else {
        // We have multiple logins. Handle a special case here, for sites
        // which have a normal user+pass login *and* a password-only login
        // (eg, a PIN). Prefer the login that matches the type of the form
        // (user+pass or pass-only) when there's exactly one that matches.
        let matchingLogins;
        if (usernameField) {
          matchingLogins = logins.filter(l => l.username);
        } else {
          matchingLogins = logins.filter(l => !l.username);
        }

        if (matchingLogins.length != 1) {
          log("Multiple logins for form, so not filling any.");
          autofillResult = AUTOFILL_RESULT.MULTIPLE_LOGINS;
          return;
        }

        selectedLogin = matchingLogins[0];
      }

      // We will always have a selectedLogin at this point.

      if (!autofillForm) {
        log("autofillForms=false but form can be filled");
        autofillResult = AUTOFILL_RESULT.NO_AUTOFILL_FORMS;
        return;
      }

      if (
        !userTriggered &&
        passwordACFieldName == "off" &&
        !LoginHelper.autofillAutocompleteOff
      ) {
        log(
          "Not autofilling the login because we're respecting autocomplete=off"
        );
        autofillResult = AUTOFILL_RESULT.AUTOCOMPLETE_OFF;
        return;
      }

      // Fill the form

      if (usernameField) {
        // Don't modify the username field if it's disabled or readOnly so we preserve its case.
        let disabledOrReadOnly =
          usernameField.disabled || usernameField.readOnly;

        let userNameDiffers = selectedLogin.username != usernameField.value;
        // Don't replace the username if it differs only in case, and the user triggered
        // this autocomplete. We assume that if it was user-triggered the entered text
        // is desired.
        let userEnteredDifferentCase =
          userTriggered &&
          userNameDiffers &&
          usernameField.value.toLowerCase() ==
            selectedLogin.username.toLowerCase();

        if (!disabledOrReadOnly) {
          if (!userEnteredDifferentCase && userNameDiffers) {
            usernameField.setUserInput(selectedLogin.username);
          }

          this._highlightFilledField(usernameField);
        }
      }

      let doc = form.ownerDocument;
      if (passwordField.value != selectedLogin.password) {
        // Ensure the field gets re-masked in case a generated password was
        // filled into it previously.
        this._stopTreatingAsGeneratedPasswordField(passwordField);
        passwordField.setUserInput(selectedLogin.password);
        let autoFilledLogin = {
          guid: selectedLogin.QueryInterface(Ci.nsILoginMetaInfo).guid,
          username: selectedLogin.username,
          usernameField: usernameField
            ? Cu.getWeakReference(usernameField)
            : null,
          password: selectedLogin.password,
          passwordField: Cu.getWeakReference(passwordField),
        };
        log(
          "Saving autoFilledLogin",
          autoFilledLogin.guid,
          "for",
          form.rootElement
        );
        this.stateForDocument(doc).fillsByRootElement.set(
          form.rootElement,
          autoFilledLogin
        );
      }

      this._highlightFilledField(passwordField);

      log("_fillForm succeeded");
      autofillResult = AUTOFILL_RESULT.FILLED;

      let win = doc.defaultView;
      let messageManager = win.docShell.messageManager;
      messageManager.sendAsyncMessage("LoginStats:LoginFillSuccessful");
    } catch (ex) {
      Cu.reportError(ex);
      throw ex;
    } finally {
      if (autofillResult == -1) {
        // eslint-disable-next-line no-unsafe-finally
        throw new Error("_fillForm: autofillResult must be specified");
      }

      if (!userTriggered) {
        // Ignore fills as a result of user action for this probe.
        Services.telemetry
          .getHistogramById("PWMGR_FORM_AUTOFILL_RESULT")
          .add(autofillResult);

        if (usernameField) {
          let focusedElement = this._formFillService.focusedInput;
          if (
            usernameField == focusedElement &&
            autofillResult !== AUTOFILL_RESULT.FILLED
          ) {
            log(
              "_fillForm: Opening username autocomplete popup since the form wasn't autofilled"
            );
            this._formFillService.showPopup();
          }
        }
      }

      if (usernameField) {
        log("_fillForm: Attaching event listeners to usernameField");
        usernameField.addEventListener("focus", observer);
        usernameField.addEventListener("mousedown", observer);
      }

      Services.obs.notifyObservers(
        form.rootElement,
        "passwordmgr-processed-form"
      );
    }
  },

  /**
   * Given a field, determine whether that field was last filled as a username
   * field AND whether the username is still filled in with the username AND
   * whether the associated password field has the matching password.
   *
   * @note This could possibly be unified with getFieldContext but they have
   * slightly different use cases. getFieldContext looks up recipes whereas this
   * method doesn't need to since it's only returning a boolean based upon the
   * recipes used for the last fill (in _fillForm).
   *
   * @param {HTMLInputElement} aUsernameField element contained in a LoginForm
   *                                          cached in LoginFormFactory.
   * @returns {Boolean} whether the username and password fields still have the
   *                    last-filled values, if previously filled.
   */
  _isLoginAlreadyFilled(aUsernameField) {
    let formLikeRoot = FormLikeFactory.findRootForField(aUsernameField);
    // Look for the existing LoginForm.
    let existingLoginForm = LoginFormFactory.getForRootElement(formLikeRoot);
    if (!existingLoginForm) {
      throw new Error(
        "_isLoginAlreadyFilled called with a username field with " +
          "no rootElement LoginForm"
      );
    }

    log("_isLoginAlreadyFilled: existingLoginForm", existingLoginForm);
    let filledLogin = this.stateForDocument(
      aUsernameField.ownerDocument
    ).fillsByRootElement.get(formLikeRoot);
    if (!filledLogin) {
      return false;
    }

    // Unpack the weak references.
    let autoFilledUsernameField = filledLogin.usernameField
      ? filledLogin.usernameField.get()
      : null;
    let autoFilledPasswordField = filledLogin.passwordField.get();

    // Check username and password values match what was filled.
    if (
      !autoFilledUsernameField ||
      autoFilledUsernameField != aUsernameField ||
      autoFilledUsernameField.value != filledLogin.username ||
      !autoFilledPasswordField ||
      autoFilledPasswordField.value != filledLogin.password
    ) {
      return false;
    }

    return true;
  },

  /**
   * Returns the username and password fields found in the form by input
   * element into form.
   *
   * @param {HTMLInputElement} aField
   *                           A form field into form.
   * @return {Array} [usernameField, newPasswordField, oldPasswordField]
   *
   * More detail of these values is same as _getFormFields.
   */
  getUserNameAndPasswordFields(aField) {
    // If the element is not a proper form field, return null.
    if (
      ChromeUtils.getClassName(aField) !== "HTMLInputElement" ||
      (aField.type != "password" && !LoginHelper.isUsernameFieldType(aField)) ||
      aField.nodePrincipal.isNullPrincipal ||
      !aField.ownerDocument
    ) {
      return [null, null, null];
    }
    let form = LoginFormFactory.createFromField(aField);

    let doc = aField.ownerDocument;
    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    let recipes = LoginRecipesContent.getRecipes(formOrigin, doc.defaultView);

    return this._getFormFields(form, false, recipes);
  },

  /**
   * Verify if a field is a valid login form field and
   * returns some information about it's LoginForm.
   *
   * @param {Element} aField
   *                  A form field we want to verify.
   *
   * @returns {Object} an object with information about the
   *                   LoginForm username and password field
   *                   or null if the passed field is invalid.
   */
  getFieldContext(aField) {
    // If the element is not a proper form field, return null.
    if (
      ChromeUtils.getClassName(aField) !== "HTMLInputElement" ||
      (aField.type != "password" && !LoginHelper.isUsernameFieldType(aField)) ||
      aField.nodePrincipal.isNullPrincipal ||
      !aField.ownerDocument
    ) {
      return null;
    }

    let [usernameField, newPasswordField] = this.getUserNameAndPasswordFields(
      aField
    );

    // If we are not verifying a password field, we want
    // to use aField as the username field.
    if (aField.type != "password") {
      usernameField = aField;
    }

    return {
      usernameField: {
        found: !!usernameField,
        disabled:
          usernameField && (usernameField.disabled || usernameField.readOnly),
      },
      passwordField: {
        found: !!newPasswordField,
        disabled:
          newPasswordField &&
          (newPasswordField.disabled || newPasswordField.readOnly),
      },
    };
  },
};
