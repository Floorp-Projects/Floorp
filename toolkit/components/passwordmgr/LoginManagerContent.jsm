/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "LoginManagerContent",
                          "FormLikeFactory",
                          "UserAutoCompleteResult" ];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
const PASSWORD_INPUT_ADDED_COALESCING_THRESHOLD_MS = 1;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask", "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginRecipesContent",
                                  "resource://gre/modules/LoginRecipes.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gContentSecurityManager",
                                   "@mozilla.org/contentsecuritymanager;1",
                                   "nsIContentSecurityManager");
XPCOMUtils.defineLazyServiceGetter(this, "gScriptSecurityManager",
                                   "@mozilla.org/scriptsecuritymanager;1",
                                   "nsIScriptSecurityManager");
XPCOMUtils.defineLazyServiceGetter(this, "gNetUtil",
                                   "@mozilla.org/network/util;1",
                                   "nsINetUtil");

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerContent");
  return logger.log.bind(logger);
});

// These mirror signon.* prefs.
var gEnabled, gAutofillForms, gStoreWhenAutocompleteOff;

var observer = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsIFormSubmitObserver,
                                          Ci.nsIWebProgressListener,
                                          Ci.nsISupportsWeakReference]),

  // nsIFormSubmitObserver
  notify(formElement, aWindow, actionURI) {
    log("observer notified for form submission.");

    // We're invoked before the content's |onsubmit| handlers, so we
    // can grab form data before it might be modified (see bug 257781).

    try {
      let formLike = FormLikeFactory.createFromForm(formElement);
      LoginManagerContent._onFormSubmit(formLike);
    } catch (e) {
      log("Caught error in onFormSubmit(", e.lineNumber, "):", e.message);
      Cu.reportError(e);
    }

    return true; // Always return true, or form submit will be canceled.
  },

  onPrefChange() {
    gEnabled = Services.prefs.getBoolPref("signon.rememberSignons");
    gAutofillForms = Services.prefs.getBoolPref("signon.autofillForms");
    gStoreWhenAutocompleteOff = Services.prefs.getBoolPref("signon.storeWhenAutocompleteOff");
  },

  // nsIWebProgressListener
  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    // Only handle pushState/replaceState here.
    if (!(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) ||
        !(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_PUSHSTATE)) {
      return;
    }

    log("onLocationChange handled:", aLocation.spec, aWebProgress.DOMWindow.document);

    LoginManagerContent._onNavigation(aWebProgress.DOMWindow.document);
  },

  onStateChange(aWebProgress, aRequest, aState, aStatus) {
    if (!(aState & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    // We only care about when a page triggered a load, not the user. For example:
    // clicking refresh/back/forward, typing a URL and hitting enter, and loading a bookmark aren't
    // likely to be when a user wants to save a login.
    let channel = aRequest.QueryInterface(Ci.nsIChannel);
    let triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
    if (triggeringPrincipal.isNullPrincipal ||
        triggeringPrincipal.equals(Services.scriptSecurityManager.getSystemPrincipal())) {
      return;
    }

    // Don't handle history navigation, reload, or pushState not triggered via chrome UI.
    // e.g. history.go(-1), location.reload(), history.replaceState()
    if (!(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_NORMAL)) {
      log("onStateChange: loadType isn't LOAD_CMD_NORMAL:", aWebProgress.loadType);
      return;
    }

    log("onStateChange handled:", channel);
    LoginManagerContent._onNavigation(aWebProgress.DOMWindow.document);
  },
};

Services.obs.addObserver(observer, "earlyformsubmit", false);
var prefBranch = Services.prefs.getBranch("signon.");
prefBranch.addObserver("", observer.onPrefChange, false);

observer.onPrefChange(); // read initial values


function messageManagerFromWindow(win) {
  return win.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIWebNavigation)
            .QueryInterface(Ci.nsIDocShell)
            .QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIContentFrameMessageManager);
}

// This object maps to the "child" process (even in the single-process case).
var LoginManagerContent = {

  __formFillService : null, // FormFillController, for username autocompleting
  get _formFillService() {
    if (!this.__formFillService)
      this.__formFillService =
                      Cc["@mozilla.org/satchel/form-fill-controller;1"].
                      getService(Ci.nsIFormFillController);
    return this.__formFillService;
  },

  _getRandomId() {
    return Cc["@mozilla.org/uuid-generator;1"]
             .getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  },

  _messages: [ "RemoteLogins:loginsFound",
               "RemoteLogins:loginsAutoCompleted" ],

  /**
   * WeakMap of the root element of a FormLike to the FormLike representing its fields.
   *
   * This is used to be able to lookup an existing FormLike for a given root element since multiple
   * calls to FormLikeFactory won't give the exact same object. When batching fills we don't always
   * want to use the most recent list of elements for a FormLike since we may end up doing multiple
   * fills for the same set of elements when a field gets added between arming and running the
   * DeferredTask.
   *
   * @type {WeakMap}
   */
  _formLikeByRootElement: new WeakMap(),

  /**
   * WeakMap of the root element of a WeakMap to the DeferredTask to fill its fields.
   *
   * This is used to be able to throttle fills for a FormLike since onDOMInputPasswordAdded gets
   * dispatched for each password field added to a document but we only want to fill once per
   * FormLike when multiple fields are added at once.
   *
   * @type {WeakMap}
   */
  _deferredPasswordAddedTasksByRootElement: new WeakMap(),

  // Map from form login requests to information about that request.
  _requests: new Map(),

  // Number of outstanding requests to each manager.
  _managers: new Map(),

  _takeRequest(msg) {
    let data = msg.data;
    let request = this._requests.get(data.requestId);

    this._requests.delete(data.requestId);

    let count = this._managers.get(msg.target);
    if (--count === 0) {
      this._managers.delete(msg.target);

      for (let message of this._messages)
        msg.target.removeMessageListener(message, this);
    } else {
      this._managers.set(msg.target, count);
    }

    return request;
  },

  _sendRequest(messageManager, requestData,
                         name, messageData) {
    let count;
    if (!(count = this._managers.get(messageManager))) {
      this._managers.set(messageManager, 1);

      for (let message of this._messages)
        messageManager.addMessageListener(message, this);
    } else {
      this._managers.set(messageManager, ++count);
    }

    let requestId = this._getRandomId();
    messageData.requestId = requestId;

    messageManager.sendAsyncMessage(name, messageData);

    let deferred = Promise.defer();
    requestData.promise = deferred;
    this._requests.set(requestId, requestData);
    return deferred.promise;
  },

  receiveMessage(msg, window) {
    if (msg.name == "RemoteLogins:fillForm") {
      this.fillForm({
        topDocument: window.document,
        loginFormOrigin: msg.data.loginFormOrigin,
        loginsFound: LoginHelper.vanillaObjectsToLogins(msg.data.logins),
        recipes: msg.data.recipes,
        inputElement: msg.objects.inputElement,
      });
      return;
    }

    let request = this._takeRequest(msg);
    switch (msg.name) {
      case "RemoteLogins:loginsFound": {
        let loginsFound = LoginHelper.vanillaObjectsToLogins(msg.data.logins);
        request.promise.resolve({
          form: request.form,
          loginsFound: loginsFound,
          recipes: msg.data.recipes,
        });
        break;
      }

      case "RemoteLogins:loginsAutoCompleted": {
        let loginsFound =
          LoginHelper.vanillaObjectsToLogins(msg.data.logins);
        // If we're in the parent process, don't pass a message manager so our
        // autocomplete result objects know they can remove the login from the
        // login manager directly.
        let messageManager =
          (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT) ?
            msg.target : undefined;
        request.promise.resolve({ logins: loginsFound, messageManager });
        break;
      }
    }
  },

  /**
   * Get relevant logins and recipes from the parent
   *
   * @param {HTMLFormElement} form - form to get login data for
   * @param {Object} options
   * @param {boolean} options.showMasterPassword - whether to show a master password prompt
   */
  _getLoginDataFromParent(form, options) {
    let doc = form.ownerDocument;
    let win = doc.defaultView;

    let formOrigin = LoginUtils._getPasswordOrigin(doc.documentURI);
    if (!formOrigin) {
      return Promise.reject("_getLoginDataFromParent: A form origin is required");
    }
    let actionOrigin = LoginUtils._getActionOrigin(form);

    let messageManager = messageManagerFromWindow(win);

    // XXX Weak??
    let requestData = { form: form };
    let messageData = { formOrigin: formOrigin,
                        actionOrigin: actionOrigin,
                        options: options };

    return this._sendRequest(messageManager, requestData,
                             "RemoteLogins:findLogins",
                             messageData);
  },

  _autoCompleteSearchAsync(aSearchString, aPreviousResult,
                                     aElement, aRect) {
    let doc = aElement.ownerDocument;
    let form = FormLikeFactory.createFromField(aElement);
    let win = doc.defaultView;

    let formOrigin = LoginUtils._getPasswordOrigin(doc.documentURI);
    let actionOrigin = LoginUtils._getActionOrigin(form);

    let messageManager = messageManagerFromWindow(win);

    let remote = (Services.appinfo.processType ===
                  Services.appinfo.PROCESS_TYPE_CONTENT);

    let previousResult = aPreviousResult ?
                           { searchString: aPreviousResult.searchString,
                             logins: LoginHelper.loginsToVanillaObjects(aPreviousResult.logins) } :
                           null;

    let requestData = {};
    let messageData = { formOrigin: formOrigin,
                        actionOrigin: actionOrigin,
                        searchString: aSearchString,
                        previousResult: previousResult,
                        rect: aRect,
                        remote: remote };

    return this._sendRequest(messageManager, requestData,
                             "RemoteLogins:autoCompleteLogins",
                             messageData);
  },

  setupProgressListener(window) {
    if (!LoginHelper.formlessCaptureEnabled) {
      return;
    }

    try {
      let webProgress = window.QueryInterface(Ci.nsIInterfaceRequestor).
                        getInterface(Ci.nsIWebNavigation).
                        QueryInterface(Ci.nsIDocShell).
                        QueryInterface(Ci.nsIInterfaceRequestor).
                        getInterface(Ci.nsIWebProgress);
      webProgress.addProgressListener(observer,
                                      Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT |
                                      Ci.nsIWebProgress.NOTIFY_LOCATION);
    } catch (ex) {
      // Ignore NS_ERROR_FAILURE if the progress listener was already added
    }
  },

  onDOMFormHasPassword(event, window) {
    if (!event.isTrusted) {
      return;
    }

    let form = event.target;
    let formLike = FormLikeFactory.createFromForm(form);
    log("onDOMFormHasPassword:", form, formLike);
    this._fetchLoginsFromParentAndFillForm(formLike, window);
  },

  onDOMInputPasswordAdded(event, window) {
    if (!event.isTrusted) {
      return;
    }

    let pwField = event.target;
    if (pwField.form) {
      // Fill is handled by onDOMFormHasPassword which is already throttled.
      return;
    }

    // Only setup the listener for formless inputs.
    // Capture within a <form> but without a submit event is bug 1287202.
    this.setupProgressListener(window);

    let formLike = FormLikeFactory.createFromField(pwField);
    log("onDOMInputPasswordAdded:", pwField, formLike);

    let deferredTask = this._deferredPasswordAddedTasksByRootElement.get(formLike.rootElement);
    if (!deferredTask) {
      log("Creating a DeferredTask to call _fetchLoginsFromParentAndFillForm soon");
      this._formLikeByRootElement.set(formLike.rootElement, formLike);

      deferredTask = new DeferredTask(function* deferredInputProcessing() {
        // Get the updated formLike instead of the one at the time of creating the DeferredTask via
        // a closure since it could be stale since FormLike.elements isn't live.
        let formLike2 = this._formLikeByRootElement.get(formLike.rootElement);
        log("Running deferred processing of onDOMInputPasswordAdded", formLike2);
        this._deferredPasswordAddedTasksByRootElement.delete(formLike2.rootElement);
        this._fetchLoginsFromParentAndFillForm(formLike2, window);
      }.bind(this), PASSWORD_INPUT_ADDED_COALESCING_THRESHOLD_MS);

      this._deferredPasswordAddedTasksByRootElement.set(formLike.rootElement, deferredTask);
    }

    if (deferredTask.isArmed) {
      log("DeferredTask is already armed so just updating the FormLike");
      // We update the FormLike so it (most important .elements) is fresh when the task eventually
      // runs since changes to the elements could affect our field heuristics.
      this._formLikeByRootElement.set(formLike.rootElement, formLike);
    } else if (window.document.readyState == "complete") {
      log("Arming the DeferredTask we just created since document.readyState == 'complete'");
      deferredTask.arm();
    } else {
      window.addEventListener("DOMContentLoaded", function armPasswordAddedTask() {
        window.removeEventListener("DOMContentLoaded", armPasswordAddedTask);
        log("Arming the onDOMInputPasswordAdded DeferredTask due to DOMContentLoaded");
        deferredTask.arm();
      });
    }
  },

  /**
   * Fetch logins from the parent for a given form and then attempt to fill it.
   *
   * @param {FormLike} form to fetch the logins for then try autofill.
   * @param {Window} window
   */
  _fetchLoginsFromParentAndFillForm(form, window) {
    this._updateLoginFormPresence(window);

    let messageManager = messageManagerFromWindow(window);
    messageManager.sendAsyncMessage("LoginStats:LoginEncountered");

    if (!gEnabled) {
      return;
    }

    this._getLoginDataFromParent(form, { showMasterPassword: true })
        .then(this.loginsFound.bind(this))
        .then(null, Cu.reportError);
  },

  onPageShow(event, window) {
    this._updateLoginFormPresence(window);
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
        loginFormRootElements: new Set(),
      };
      this.loginFormStateByDocument.set(document, loginFormState);
    }
    return loginFormState;
  },

  /**
   * Compute whether there is a login form on any frame of the current page, and
   * notify the parent process. This is one of the factors used to control the
   * visibility of the password fill doorhanger anchor.
   */
  _updateLoginFormPresence(topWindow) {
    log("_updateLoginFormPresence", topWindow.location.href);
    // For the login form presence notification, we currently support only one
    // origin for each browser, so the form origin will always match the origin
    // of the top level document.
    let loginFormOrigin =
        LoginUtils._getPasswordOrigin(topWindow.document.documentURI);

    // Returns the first known loginForm present in this window or in any
    // same-origin subframes. Returns null if no loginForm is currently present.
    let getFirstLoginForm = thisWindow => {
      let loginForms = this.stateForDocument(thisWindow.document).loginFormRootElements;
      if (loginForms.size) {
        return [...loginForms][0];
      }
      for (let i = 0; i < thisWindow.frames.length; i++) {
        let frame = thisWindow.frames[i];
        if (LoginUtils._getPasswordOrigin(frame.document.documentURI) !=
            loginFormOrigin) {
          continue;
        }
        let loginForm = getFirstLoginForm(frame);
        if (loginForm) {
          return loginForm;
        }
      }
      return null;
    };

    // Returns true if this window or any subframes have insecure login forms.
    let hasInsecureLoginForms = (thisWindow, parentIsInsecure) => {
      let doc = thisWindow.document;
      let isInsecure = parentIsInsecure || !this.isDocumentSecure(doc);
      let hasLoginForm = this.stateForDocument(doc).loginFormRootElements.size > 0;
      return (hasLoginForm && isInsecure) ||
             Array.some(thisWindow.frames,
                        frame => hasInsecureLoginForms(frame, isInsecure));
    };

    // Store the actual form to use on the state for the top-level document.
    let topState = this.stateForDocument(topWindow.document);
    topState.loginFormForFill = getFirstLoginForm(topWindow);
    log("_updateLoginFormPresence: topState.loginFormForFill", topState.loginFormForFill);

    // Determine whether to show the anchor icon for the current tab.
    let messageManager = messageManagerFromWindow(topWindow);
    messageManager.sendAsyncMessage("RemoteLogins:updateLoginFormPresence", {
      loginFormOrigin,
      loginFormPresent: !!topState.loginFormForFill,
      hasInsecureLoginForms: hasInsecureLoginForms(topWindow, false),
    });
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
   *          inputElement:
   *            Optional input password element from the form we want to fill.
   *        }
   */
  fillForm({ topDocument, loginFormOrigin, loginsFound, recipes, inputElement }) {
    let topState = this.stateForDocument(topDocument);
    if (!inputElement && !topState.loginFormForFill) {
      log("fillForm: There is no login form anymore. The form may have been",
          "removed or the document may have changed.");
      return;
    }
    if (LoginUtils._getPasswordOrigin(topDocument.documentURI) != loginFormOrigin) {
      if (!inputElement ||
          LoginUtils._getPasswordOrigin(inputElement.ownerDocument.documentURI) != loginFormOrigin) {
        log("fillForm: The requested origin doesn't match the one form the",
            "document. This may mean we navigated to a document from a different",
            "site before we had a chance to indicate this change in the user",
            "interface.");
        return;
      }
    }
    let form = topState.loginFormForFill;
    let clobberUsername = true;
    let options = {
      inputElement,
    };

    // If we have a target input, fills it's form.
    if (inputElement) {
      form = FormLikeFactory.createFromField(inputElement);
      if (inputElement.type == "password") {
        clobberUsername = false;
      }
    }
    this._fillForm(form, true, clobberUsername, true, true, loginsFound, recipes, options);
  },

  loginsFound({ form, loginsFound, recipes }) {
    let doc = form.ownerDocument;
    let autofillForm = gAutofillForms && !PrivateBrowsingUtils.isContentWindowPrivate(doc.defaultView);

    this._fillForm(form, autofillForm, false, false, false, loginsFound, recipes);
  },

  /*
   * onUsernameInput
   *
   * Listens for DOMAutoComplete and blur events on an input field.
   */
  onUsernameInput(event) {
    if (!event.isTrusted)
      return;

    if (!gEnabled)
      return;

    var acInputField = event.target;

    // This is probably a bit over-conservatative.
    if (!(acInputField.ownerDocument instanceof Ci.nsIDOMHTMLDocument))
      return;

    if (!LoginHelper.isUsernameFieldType(acInputField))
      return;

    var acForm = FormLikeFactory.createFromField(acInputField);
    if (!acForm)
      return;

    // If the username is blank, bail out now -- we don't want
    // fillForm() to try filling in a login without a username
    // to filter on (bug 471906).
    if (!acInputField.value)
      return;

    log("onUsernameInput from", event.type);

    let doc = acForm.ownerDocument;
    let messageManager = messageManagerFromWindow(doc.defaultView);
    let recipes = messageManager.sendSyncMessage("RemoteLogins:findRecipes", {
      formOrigin: LoginUtils._getPasswordOrigin(doc.documentURI),
    })[0];

    // Make sure the username field fillForm will use is the
    // same field as the autocomplete was activated on.
    var [usernameField, passwordField, ignored] =
        this._getFormFields(acForm, false, recipes);
    if (usernameField == acInputField && passwordField) {
      this._getLoginDataFromParent(acForm, { showMasterPassword: false })
          .then(({ form, loginsFound, recipes }) => {
            this._fillForm(form, true, false, true, true, loginsFound, recipes);
          })
          .then(null, Cu.reportError);
    } else {
      // Ignore the event, it's for some input we don't care about.
    }
  },

  /**
   * @param {FormLike} form - the FormLike to look for password fields in.
   * @param {bool} [skipEmptyFields=false] - Whether to ignore password fields with no value.
   *                                         Used at capture time since saving empty values isn't
   *                                         useful.
   * @return {Array|null} Array of password field elements for the specified form.
   *                      If no pw fields are found, or if more than 3 are found, then null
   *                      is returned.
   */
  _getPasswordFields(form, skipEmptyFields = false) {
    // Locate the password fields in the form.
    let pwFields = [];
    for (let i = 0; i < form.elements.length; i++) {
      let element = form.elements[i];
      if (!(element instanceof Ci.nsIDOMHTMLInputElement) ||
          element.type != "password") {
        continue;
      }

      if (skipEmptyFields && !element.value) {
        continue;
      }

      pwFields[pwFields.length] = {
                                    index   : i,
                                    element : element
                                  };
    }

    // If too few or too many fields, bail out.
    if (pwFields.length == 0) {
      log("(form ignored -- no password fields.)");
      return null;
    } else if (pwFields.length > 3) {
      log("(form ignored -- too many password fields. [ got ", pwFields.length, "])");
      return null;
    }

    return pwFields;
  },

  /**
   * Returns the username and password fields found in the form.
   * Can handle complex forms by trying to figure out what the
   * relevant fields are.
   *
   * @param {FormLike} form
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
   * Note that even though we can create a FormLike from a text field,
   * this method will only return a non-null usernameField if the
   * FormLike has a password field.
   */
  _getFormFields(form, isSubmission, recipes) {
    var usernameField = null;
    var pwFields = null;
    var fieldOverrideRecipe = LoginRecipesContent.getFieldOverrides(recipes, form);
    if (fieldOverrideRecipe) {
      var pwOverrideField = LoginRecipesContent.queryLoginField(
        form,
        fieldOverrideRecipe.passwordSelector
      );
      if (pwOverrideField) {
        // The field from the password override may be in a different FormLike.
        let formLike = FormLikeFactory.createFromField(pwOverrideField);
        pwFields = [{
          index   : [...formLike.elements].indexOf(pwOverrideField),
          element : pwOverrideField,
        }];
      }

      var usernameOverrideField = LoginRecipesContent.queryLoginField(
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
      pwFields = this._getPasswordFields(form, isSubmission);
    }

    if (!pwFields) {
      return [null, null, null];
    }

    if (!usernameField) {
      // Locate the username field in the form by searching backwards
      // from the first password field, assume the first text field is the
      // username. We might not find a username field if the user is
      // already logged in to the site.
      for (var i = pwFields[0].index - 1; i >= 0; i--) {
        var element = form.elements[i];
        if (!LoginHelper.isUsernameFieldType(element)) {
          continue;
        }

        if (fieldOverrideRecipe && fieldOverrideRecipe.notUsernameSelector &&
            element.matches(fieldOverrideRecipe.notUsernameSelector)) {
          continue;
        }

        usernameField = element;
        break;
      }
    }

    if (!usernameField)
      log("(form -- no username field found)");
    else
      log("Username field ", usernameField, "has name/value:",
          usernameField.name, "/", usernameField.value);

    // If we're not submitting a form (it's a page load), there are no
    // password field values for us to use for identifying fields. So,
    // just assume the first password field is the one to be filled in.
    if (!isSubmission || pwFields.length == 1) {
      var passwordField = pwFields[0].element;
      log("Password field", passwordField, "has name: ", passwordField.name);
      return [usernameField, passwordField, null];
    }


    // Try to figure out WTF is in the form based on the password values.
    var oldPasswordField, newPasswordField;
    var pw1 = pwFields[0].element.value;
    var pw2 = pwFields[1].element.value;
    var pw3 = (pwFields[2] ? pwFields[2].element.value : null);

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
      } else  if (pw1 == pw3) {
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

    log("Password field (new) id/name is: ", newPasswordField.id, " / ", newPasswordField.name);
    if (oldPasswordField) {
      log("Password field (old) id/name is: ", oldPasswordField.id, " / ", oldPasswordField.name);
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
   * Trigger capture on any relevant FormLikes due to a navigation alone (not
   * necessarily due to an actual form submission). This method is used to
   * capture logins for cases where form submit events are not used.
   *
   * To avoid multiple notifications for the same FormLike, this currently
   * avoids capturing when dealing with a real <form> which are ideally already
   * using a submit event.
   *
   * @param {Document} document being navigated
   */
  _onNavigation(aDocument) {
    let state = this.stateForDocument(aDocument);
    let loginFormRootElements = state.loginFormRootElements;
    log("_onNavigation: state:", state, "loginFormRootElements size:", loginFormRootElements.size,
        "document:", aDocument);

    for (let formRoot of state.loginFormRootElements) {
      if (formRoot instanceof Ci.nsIDOMHTMLFormElement) {
        // For now only perform capture upon navigation for FormLike's without
        // a <form> to avoid capture from both an earlyformsubmit and
        // navigation for the same "form".
        log("Ignoring navigation for the form root to avoid multiple prompts " +
            "since it was for a real <form>");
        continue;
      }
      let formLike = this._formLikeByRootElement.get(formRoot);
      this._onFormSubmit(formLike);
    }
  },

  /**
   * Called by our observer when notified of a form submission.
   * [Note that this happens before any DOM onsubmit handlers are invoked.]
   * Looks for a password change in the submitted form, so we can update
   * our stored password.
   *
   * @param {FormLike} form
   */
  _onFormSubmit(form) {
    log("_onFormSubmit", form);
    var doc = form.ownerDocument;
    var win = doc.defaultView;

    if (PrivateBrowsingUtils.isContentWindowPrivate(win)) {
      // We won't do anything in private browsing mode anyway,
      // so there's no need to perform further checks.
      log("(form submission ignored in private browsing mode)");
      return;
    }

    // If password saving is disabled (globally or for host), bail out now.
    if (!gEnabled)
      return;

    var hostname = LoginUtils._getPasswordOrigin(doc.documentURI);
    if (!hostname) {
      log("(form submission ignored -- invalid hostname)");
      return;
    }

    let formSubmitURL = LoginUtils._getActionOrigin(form);
    let messageManager = messageManagerFromWindow(win);

    let recipes = messageManager.sendSyncMessage("RemoteLogins:findRecipes", {
      formOrigin: hostname,
    })[0];

    // Get the appropriate fields from the form.
    var [usernameField, newPasswordField, oldPasswordField] =
          this._getFormFields(form, true, recipes);

    // Need at least 1 valid password field to do anything.
    if (newPasswordField == null)
      return;

    // Check for autocomplete=off attribute. We don't use it to prevent
    // autofilling (for existing logins), but won't save logins when it's
    // present and the storeWhenAutocompleteOff pref is false.
    // XXX spin out a bug that we don't update timeLastUsed in this case?
    if ((this._isAutocompleteDisabled(form) ||
         this._isAutocompleteDisabled(usernameField) ||
         this._isAutocompleteDisabled(newPasswordField) ||
         this._isAutocompleteDisabled(oldPasswordField)) &&
        !gStoreWhenAutocompleteOff) {
      log("(form submission ignored -- autocomplete=off found)");
      return;
    }

    // Don't try to send DOM nodes over IPC.
    let mockUsername = usernameField ?
                         { name: usernameField.name,
                           value: usernameField.value } :
                         null;
    let mockPassword = { name: newPasswordField.name,
                         value: newPasswordField.value };
    let mockOldPassword = oldPasswordField ?
                            { name: oldPasswordField.name,
                              value: oldPasswordField.value } :
                            null;

    // Make sure to pass the opener's top in case it was in a frame.
    let openerTopWindow = win.opener ? win.opener.top : null;

    messageManager.sendAsyncMessage("RemoteLogins:onFormSubmit",
                                    { hostname: hostname,
                                      formSubmitURL: formSubmitURL,
                                      usernameField: mockUsername,
                                      newPasswordField: mockPassword,
                                      oldPasswordField: mockOldPassword },
                                    { openerTopWindow });
  },

  /**
   * Attempt to find the username and password fields in a form, and fill them
   * in using the provided logins and recipes.
   *
   * @param {HTMLFormElement} form
   * @param {bool} autofillForm denotes if we should fill the form in automatically
   * @param {bool} clobberUsername controls if an existing username can be overwritten.
   *                               If this is false and an inputElement of type password
   *                               is also passed, the username field will be ignored.
   *                               If this is false and no inputElement is passed, if the username
   *                               field value is not found in foundLogins, it will not fill the password.
   * @param {bool} clobberPassword controls if an existing password value can be
   *                               overwritten
   * @param {bool} userTriggered is an indication of whether this filling was triggered by
   *                             the user
   * @param {nsILoginInfo[]} foundLogins is an array of nsILoginInfo that could be used for the form
   * @param {Set} recipes that could be used to affect how the form is filled
   * @param {Object} [options = {}] is a list of options for this method.
            - [inputElement] is an optional target input element we want to fill
   */
  _fillForm(form, autofillForm, clobberUsername, clobberPassword,
                        userTriggered, foundLogins, recipes, {inputElement} = {}) {
    log("_fillForm", form.elements);
    let ignoreAutocomplete = true;
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
    };

    function recordAutofillResult(result) {
      if (userTriggered) {
        // Ignore fills as a result of user action.
        return;
      }
      const autofillResultHist = Services.telemetry.getHistogramById("PWMGR_FORM_AUTOFILL_RESULT");
      autofillResultHist.add(result);
    }

    try {
      // Nothing to do if we have no matching logins available.
      if (foundLogins.length == 0) {
        // We don't log() here since this is a very common case.
        recordAutofillResult(AUTOFILL_RESULT.NO_SAVED_LOGINS);
        return;
      }

      // Heuristically determine what the user/pass fields are
      // We do this before checking to see if logins are stored,
      // so that the user isn't prompted for a master password
      // without need.
      var [usernameField, passwordField, ignored] =
            this._getFormFields(form, false, recipes);

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
        recordAutofillResult(AUTOFILL_RESULT.NO_PASSWORD_FIELD);
        return;
      }

      // If the password field is disabled or read-only, there's nothing to do.
      if (passwordField.disabled || passwordField.readOnly) {
        log("not filling form, password field disabled or read-only");
        recordAutofillResult(AUTOFILL_RESULT.PASSWORD_DISABLED_READONLY);
        return;
      }

      var isAutocompleteOff = false;
      if (this._isAutocompleteDisabled(form) ||
          this._isAutocompleteDisabled(usernameField) ||
          this._isAutocompleteDisabled(passwordField)) {
        isAutocompleteOff = true;
      }

      // Discard logins which have username/password values that don't
      // fit into the fields (as specified by the maxlength attribute).
      // The user couldn't enter these values anyway, and it helps
      // with sites that have an extra PIN to be entered (bug 391514)
      var maxUsernameLen = Number.MAX_VALUE;
      var maxPasswordLen = Number.MAX_VALUE;

      // If attribute wasn't set, default is -1.
      if (usernameField && usernameField.maxLength >= 0)
        maxUsernameLen = usernameField.maxLength;
      if (passwordField.maxLength >= 0)
        maxPasswordLen = passwordField.maxLength;

      var logins = foundLogins.filter(function (l) {
        var fit = (l.username.length <= maxUsernameLen &&
                   l.password.length <= maxPasswordLen);
        if (!fit)
          log("Ignored", l.username, "login: won't fit");

        return fit;
      }, this);

      if (logins.length == 0) {
        log("form not filled, none of the logins fit in the field");
        recordAutofillResult(AUTOFILL_RESULT.NO_LOGINS_FIT);
        return;
      }

      // Attach autocomplete stuff to the username field, if we have
      // one. This is normally used to select from multiple accounts,
      // but even with one account we should refill if the user edits.
      if (usernameField)
        this._formFillService.markAsLoginManagerField(usernameField);

      // Don't clobber an existing password.
      if (passwordField.value && !clobberPassword) {
        log("form not filled, the password field was already filled");
        recordAutofillResult(AUTOFILL_RESULT.EXISTING_PASSWORD);
        return;
      }

      // Select a login to use for filling in the form.
      var selectedLogin;
      if (!clobberUsername && usernameField && (usernameField.value ||
                                                usernameField.disabled ||
                                                usernameField.readOnly)) {
        // If username was specified in the field, it's disabled or it's readOnly, only fill in the
        // password if we find a matching login.
        var username = usernameField.value.toLowerCase();

        let matchingLogins = logins.filter(l =>
                                           l.username.toLowerCase() == username);
        if (matchingLogins.length == 0) {
          log("Password not filled. None of the stored logins match the username already present.");
          recordAutofillResult(AUTOFILL_RESULT.EXISTING_USERNAME);
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
        if (usernameField)
          matchingLogins = logins.filter(l => l.username);
        else
          matchingLogins = logins.filter(l => !l.username);

        if (matchingLogins.length != 1) {
          log("Multiple logins for form, so not filling any.");
          recordAutofillResult(AUTOFILL_RESULT.MULTIPLE_LOGINS);
          return;
        }

        selectedLogin = matchingLogins[0];
      }

      // We will always have a selectedLogin at this point.

      if (!autofillForm) {
        log("autofillForms=false but form can be filled");
        recordAutofillResult(AUTOFILL_RESULT.NO_AUTOFILL_FORMS);
        return;
      }

      if (isAutocompleteOff && !ignoreAutocomplete) {
        log("Not filling the login because we're respecting autocomplete=off");
        recordAutofillResult(AUTOFILL_RESULT.AUTOCOMPLETE_OFF);
        return;
      }

      // Fill the form

      if (usernameField) {
      // Don't modify the username field if it's disabled or readOnly so we preserve its case.
        let disabledOrReadOnly = usernameField.disabled || usernameField.readOnly;

        let userNameDiffers = selectedLogin.username != usernameField.value;
        // Don't replace the username if it differs only in case, and the user triggered
        // this autocomplete. We assume that if it was user-triggered the entered text
        // is desired.
        let userEnteredDifferentCase = userTriggered && userNameDiffers &&
               usernameField.value.toLowerCase() == selectedLogin.username.toLowerCase();

        if (!disabledOrReadOnly && !userEnteredDifferentCase && userNameDiffers) {
          usernameField.setUserInput(selectedLogin.username);
        }
      }
      if (passwordField.value != selectedLogin.password) {
        passwordField.setUserInput(selectedLogin.password);
      }

      log("_fillForm succeeded");
      recordAutofillResult(AUTOFILL_RESULT.FILLED);
      let doc = form.ownerDocument;
      let win = doc.defaultView;
      let messageManager = messageManagerFromWindow(win);
      messageManager.sendAsyncMessage("LoginStats:LoginFillSuccessful");
    } finally {
      Services.obs.notifyObservers(form.rootElement, "passwordmgr-processed-form", null);
    }
  },

  /**
   * Verify if a field is a valid login form field and
   * returns some information about it's FormLike.
   *
   * @param {Element} aField
   *                  A form field we want to verify.
   *
   * @returns {Object} an object with information about the
   *                   FormLike username and password field
   *                   or null if the passed field is invalid.
   */
  getFieldContext(aField) {
    // If the element is not a proper form field, return null.
    if (!(aField instanceof Ci.nsIDOMHTMLInputElement) ||
        (aField.type != "password" && !LoginHelper.isUsernameFieldType(aField)) ||
        !aField.ownerDocument) {
      return null;
    }
    let form = FormLikeFactory.createFromField(aField);

    let doc = aField.ownerDocument;
    let messageManager = messageManagerFromWindow(doc.defaultView);
    let recipes = messageManager.sendSyncMessage("RemoteLogins:findRecipes", {
      formOrigin: LoginUtils._getPasswordOrigin(doc.documentURI),
    })[0];

    let [usernameField, newPasswordField, oldPasswordField] =
          this._getFormFields(form, false, recipes);

    // If we are not verifying a password field, we want
    // to use aField as the username field.
    if (aField.type != "password") {
      usernameField = aField;
    }

    return {
      usernameField: {
        found: !!usernameField,
        disabled: usernameField && (usernameField.disabled || usernameField.readOnly),
      },
      passwordField: {
        found: !!newPasswordField,
        disabled: newPasswordField && (newPasswordField.disabled || newPasswordField.readOnly),
      },
    };
  },

  /**
   * Returns true if the provided document principal and URI are such that the
   * page using them can safely host password fields.
   *
   * This means the page can't be easily tampered with because it is sent over
   * an encrypted channel or is a local resource that never hits the network.
   *
   * The system principal, codebase principals with secure schemes like "https",
   * and local schemes like "resource" and "file" are all considered secure. If
   * the page is sandboxed, then the document URI is checked instead.
   *
   * @param document
   *        The document whose principal and URI are to be considered.
   */
  isDocumentSecure(document) {
    let principal = document.nodePrincipal;
    if (principal.isSystemPrincipal) {
      return true;
    }

    // Fall back to the document URI for sandboxed documents that do not have
    // the allow-same-origin flag, as they have a null principal instead of a
    // codebase principal. Here there are still some cases that are considered
    // insecure while they are secure, for example sandboxed documents created
    // using a "javascript:" or "data:" URI from an HTTPS page. See bug 1162772
    // for defining "window.isSecureContext", that may help in these cases.
    if (!principal.isCodebasePrincipal) {
      principal =
        gScriptSecurityManager.getCodebasePrincipal(document.documentURIObject);
    }

    // These checks include "file", "resource", HTTPS, and HTTP to "localhost".
    return gContentSecurityManager.isOriginPotentiallyTrustworthy(principal);
  },
};

var LoginUtils = {
  /**
   * Get the parts of the URL we want for identification.
   * Strip out things like the userPass portion
   */
  _getPasswordOrigin(uriString, allowJS) {
    var realm = "";
    try {
      var uri = Services.io.newURI(uriString, null, null);

      if (allowJS && uri.scheme == "javascript")
        return "javascript:";

      // Build this manually instead of using prePath to avoid including the userPass portion.
      realm = uri.scheme + "://" + uri.hostPort;
    } catch (e) {
      // bug 159484 - disallow url types that don't support a hostPort.
      // (although we handle "javascript:..." as a special case above.)
      log("Couldn't parse origin for", uriString, e);
      realm = null;
    }

    return realm;
  },

  _getActionOrigin(form) {
    var uriString = form.action;

    // A blank or missing action submits to where it came from.
    if (uriString == "")
      uriString = form.baseURI; // ala bug 297761

    return this._getPasswordOrigin(uriString, true);
  },
};

// nsIAutoCompleteResult implementation
function UserAutoCompleteResult (aSearchString, matchingLogins, messageManager) {
  function loginSort(a, b) {
    var userA = a.username.toLowerCase();
    var userB = b.username.toLowerCase();

    if (userA < userB)
      return -1;

    if (userA > userB)
      return  1;

    return 0;
  }

  this.searchString = aSearchString;
  this.logins = matchingLogins.sort(loginSort);
  this.matchCount = matchingLogins.length;
  this._messageManager = messageManager;

  if (this.matchCount > 0) {
    this.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
    this.defaultIndex = 0;
  }
}

UserAutoCompleteResult.prototype = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIAutoCompleteResult,
                                          Ci.nsISupportsWeakReference]),

  // private
  logins : null,

  // Allow autoCompleteSearch to get at the JS object so it can
  // modify some readonly properties for internal use.
  get wrappedJSObject() {
    return this;
  },

  // Interfaces from idl...
  searchString : null,
  searchResult : Ci.nsIAutoCompleteResult.RESULT_NOMATCH,
  defaultIndex : -1,
  errorDescription : "",
  matchCount : 0,

  getValueAt(index) {
    if (index < 0 || index >= this.logins.length)
      throw new Error("Index out of range.");

    return this.logins[index].username;
  },

  getLabelAt(index) {
    return this.getValueAt(index);
  },

  getCommentAt(index) {
    return "";
  },

  getStyleAt(index) {
    return "";
  },

  getImageAt(index) {
    return "";
  },

  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  },

  removeValueAt(index, removeFromDB) {
    if (index < 0 || index >= this.logins.length)
        throw new Error("Index out of range.");

    var [removedLogin] = this.logins.splice(index, 1);

    this.matchCount--;
    if (this.defaultIndex > this.logins.length)
      this.defaultIndex--;

    if (removeFromDB) {
      if (this._messageManager) {
        let vanilla = LoginHelper.loginToVanillaObject(removedLogin);
        this._messageManager.sendAsyncMessage("RemoteLogins:removeLogin",
                                              { login: vanilla });
      } else {
        Services.logins.removeLogin(removedLogin);
      }
    }
  }
};

/**
 * A factory to generate FormLike objects that represent a set of login fields
 * which aren't necessarily marked up with a <form> element.
 */
var FormLikeFactory = {
  _propsFromForm: [
    "autocomplete",
    "ownerDocument",
  ],

  /**
   * Create a FormLike object from a <form>.
   *
   * @param {HTMLFormElement} aForm
   * @return {FormLike}
   * @throws Error if aForm isn't an HTMLFormElement
   */
  createFromForm(aForm) {
    if (!(aForm instanceof Ci.nsIDOMHTMLFormElement)) {
      throw new Error("createFromForm: aForm must be a nsIDOMHTMLFormElement");
    }

    let formLike = {
      action: LoginUtils._getActionOrigin(aForm),
      elements: [...aForm.elements],
      rootElement: aForm,
    };

    for (let prop of this._propsFromForm) {
      formLike[prop] = aForm[prop];
    }

    this._addToJSONProperty(formLike);

    let state = LoginManagerContent.stateForDocument(formLike.ownerDocument);
    state.loginFormRootElements.add(formLike.rootElement);
    log("adding", formLike.rootElement, "to loginFormRootElements for", formLike.ownerDocument);

    LoginManagerContent._formLikeByRootElement.set(formLike.rootElement, formLike);
    return formLike;
  },

  /**
   * Create a FormLike object from a password or username field.
   *
   * If the field is in a <form>, construct the FormLike from the form.
   * Otherwise, create a FormLike with a rootElement (wrapper) according to
   * heuristics. Currently all <input> not in a <form> are one FormLike but this
   * shouldn't be relied upon as the heuristics may change to detect multiple
   * "forms" (e.g. registration and login) on one page with a <form>.
   *
   * Note that two FormLikes created from the same field won't return the same FormLike object.
   * Use the `rootElement` property on the FormLike as a key instead.
   *
   * @param {HTMLInputElement} aField - a password or username field in a document
   * @return {FormLike}
   * @throws Error if aField isn't a password or username field in a document
   */
  createFromField(aField) {
    if (!(aField instanceof Ci.nsIDOMHTMLInputElement) ||
        (aField.type != "password" && !LoginHelper.isUsernameFieldType(aField)) ||
        !aField.ownerDocument) {
      throw new Error("createFromField requires a password or username field in a document");
    }

    if (aField.form) {
      return this.createFromForm(aField.form);
    }

    let doc = aField.ownerDocument;
    log("Created non-form FormLike for rootElement:", doc.documentElement);
    let elements = [];
    for (let el of doc.documentElement.querySelectorAll("input")) {
      if (!el.form) {
        elements.push(el);
      }
    }
    let formLike = {
      action: LoginUtils._getPasswordOrigin(doc.baseURI),
      autocomplete: "on",
      // Exclude elements inside the rootElement that are already in a <form> as
      // they will be handled by their own FormLike.
      elements,
      ownerDocument: doc,
      rootElement: doc.documentElement,
    };

    let state = LoginManagerContent.stateForDocument(formLike.ownerDocument);
    state.loginFormRootElements.add(formLike.rootElement);
    log("adding", formLike.rootElement, "to loginFormRootElements for", formLike.ownerDocument);


    LoginManagerContent._formLikeByRootElement.set(formLike.rootElement, formLike);

    this._addToJSONProperty(formLike);
    return formLike;
  },

  /**
   * Add a `toJSON` property to a FormLike so logging which ends up going
   * through dump doesn't include usless garbage from DOM objects.
   */
  _addToJSONProperty(aFormLike) {
    function prettyElementOutput(aElement) {
      let idText = aElement.id ? "#" + aElement.id : "";
      let classText = "";
      for (let className of aElement.classList) {
        classText += "." + className;
      }
      return `<${aElement.nodeName + idText + classText}>`;
    }

    Object.defineProperty(aFormLike, "toJSON", {
      value: () => {
        let cleansed = {};
        for (let key of Object.keys(aFormLike)) {
          let value = aFormLike[key];
          let cleansedValue = value;

          switch (key) {
            case "elements": {
              cleansedValue = [];
              for (let element of value) {
                cleansedValue.push(prettyElementOutput(element));
              }
              break;
            }

            case "ownerDocument": {
              cleansedValue = {
                location: {
                  href: value.location.href,
                },
              };
              break;
            }

            case "rootElement": {
              cleansedValue = prettyElementOutput(value);
              break;
            }
          }

          cleansed[key] = cleansedValue;
        }
        return cleansed;
      }
    });
  },
};
