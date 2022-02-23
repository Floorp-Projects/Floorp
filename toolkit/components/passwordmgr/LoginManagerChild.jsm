/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Module doing most of the content process work for the password manager.
 */

// Disable use-ownerGlobal since LoginForm doesn't have it.
/* eslint-disable mozilla/use-ownerGlobal */

"use strict";

const EXPORTED_SYMBOLS = ["LoginManagerChild"];

const PASSWORD_INPUT_ADDED_COALESCING_THRESHOLD_MS = 1;
// The amount of time a context menu event supresses showing a
// popup from a focus event in ms. This matches the threshold in
// toolkit/components/satchel/nsFormFillController.cpp
const AUTOCOMPLETE_AFTER_RIGHT_CLICK_THRESHOLD_MS = 400;
const AUTOFILL_STATE = "autofill";

const SUBMIT_FORM_SUBMIT = 1;
const SUBMIT_PAGE_NAVIGATION = 2;
const SUBMIT_FORM_IS_REMOVED = 3;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
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
  "gFormFillService",
  "@mozilla.org/satchel/form-fill-controller;1",
  "nsIFormFillController"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginManagerChild");
  return logger.log.bind(logger);
});

Services.cpmm.addMessageListener("clearRecipeCache", () => {
  LoginRecipesContent._clearRecipeCache();
});

let gLastRightClickTimeStamp = Number.NEGATIVE_INFINITY;

// Input element on which enter keydown event was fired.
let gKeyDownEnterForInput = null;

// Events on pages with Shadow DOM could return the shadow host element
// (aEvent.target) rather than the actual username or password field
// (aEvent.composedTarget).
// Only allow input elements (can be extended later) to avoid false negatives.
class WeakFieldSet extends WeakSet {
  add(value) {
    if (ChromeUtils.getClassName(value) != "HTMLInputElement") {
      throw new Error("Non-field type added to a WeakFieldSet");
    }
    super.add(value);
  }
}

const observer = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
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

    LoginManagerChild.forWindow(aWebProgress.DOMWindow)._onNavigation(
      aWebProgress.DOMWindow.document
    );
  },

  onStateChange(aWebProgress, aRequest, aState, aStatus) {
    if (
      aState & Ci.nsIWebProgressListener.STATE_RESTORING &&
      aState & Ci.nsIWebProgressListener.STATE_STOP
    ) {
      // Re-fill a document restored from bfcache since password field values
      // aren't persisted there.
      LoginManagerChild.forWindow(aWebProgress.DOMWindow)._onDocumentRestored(
        aWebProgress.DOMWindow.document
      );
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
    LoginManagerChild.forWindow(aWebProgress.DOMWindow)._onNavigation(
      aWebProgress.DOMWindow.document
    );
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

        let { focusedInput } = gFormFillService;
        if (focusedInput.nodePrincipal.isNullPrincipal) {
          // If we have a null principal then prevent any more password manager code from running and
          // incorrectly using the document `location`.
          return;
        }

        let window = focusedInput.ownerGlobal;
        let loginManagerChild = LoginManagerChild.forWindow(window);

        let style = input.controller.getStyleAt(selectedIndex);
        if (style == "login" || style == "loginWithOrigin") {
          let details = JSON.parse(
            input.controller.getCommentAt(selectedIndex)
          );
          loginManagerChild.onFieldAutoComplete(focusedInput, details.guid);
        } else if (style == "generatedPassword") {
          loginManagerChild._filledWithGeneratedPassword(focusedInput);
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

    let ownerDocument = aEvent.target.ownerDocument;
    let window = ownerDocument.defaultView;
    let docState = LoginManagerChild.forWindow(window).stateForDocument(
      ownerDocument
    );

    switch (aEvent.type) {
      // Used to mask fields with filled generated passwords when blurred.
      case "blur": {
        if (docState.generatedPasswordFields.has(aEvent.composedTarget)) {
          let unmask = false;
          LoginManagerChild.forWindow(window)._togglePasswordFieldMasking(
            aEvent.composedTarget,
            unmask
          );
        }
        break;
      }

      // Used to watch for changes to username and password fields.
      case "change": {
        let formLikeRoot = FormLikeFactory.findRootForField(
          aEvent.composedTarget
        );
        if (!docState.fieldModificationsByRootElement.get(formLikeRoot)) {
          log("Ignoring change event on form that hasn't been user-modified");
          if (aEvent.composedTarget.hasBeenTypePassword) {
            // Send notification that the password field has not been changed.
            // This is used only for testing.
            LoginManagerChild.forWindow(window)._ignorePasswordEdit();
          }
          break;
        }

        // _storeUserInput mutates docstate
        this._storeUserInput(docState, aEvent.composedTarget);
        let detail = {
          possibleValues: {
            usernames: docState.possibleUsernames,
            passwords: docState.possiblePasswords,
          },
        };
        LoginManagerChild.forWindow(window).sendAsyncMessage(
          "PasswordManager:updateDoorhangerSuggestions",
          detail
        );

        if (aEvent.composedTarget.hasBeenTypePassword) {
          let triggeredByFillingGenerated = docState.generatedPasswordFields.has(
            aEvent.composedTarget
          );
          // Autosave generated password initial fills and subsequent edits
          if (triggeredByFillingGenerated) {
            LoginManagerChild.forWindow(window)._passwordEditedOrGenerated(
              aEvent.composedTarget,
              {
                triggeredByFillingGenerated,
              }
            );
          } else {
            // Send a notification that we are not saving the edit to the password field.
            // This is used only for testing.
            LoginManagerChild.forWindow(window)._ignorePasswordEdit();
          }
        }
        break;
      }

      case "input": {
        let field = aEvent.composedTarget;
        let isPasswordType = LoginHelper.isPasswordFieldType(field);
        // React to input into fields filled with generated passwords.
        let loginManagerChild = LoginManagerChild.forWindow(window);
        if (
          docState.generatedPasswordFields.has(field) &&
          // Depending on the edit, we may no longer want to consider
          // the field a generated password field to avoid autosaving.
          loginManagerChild._doesEventClearPrevFieldValue(aEvent)
        ) {
          loginManagerChild._stopTreatingAsGeneratedPasswordField(field);
        }

        if (!isPasswordType && !LoginHelper.isUsernameFieldType(field)) {
          break;
        }

        // React to input into potential username or password fields
        let formLikeRoot = FormLikeFactory.findRootForField(field);

        if (formLikeRoot !== aEvent.currentTarget) {
          break;
        }
        // flag this form as user-modified for the closest form/root ancestor
        let alreadyModified = docState.fieldModificationsByRootElement.get(
          formLikeRoot
        );
        let { login: filledLogin, userTriggered: fillWasUserTriggered } =
          docState.fillsByRootElement.get(formLikeRoot) || {};

        // don't flag as user-modified if the form was autofilled and doesn't appear to have changed
        let isAutofillInput = filledLogin && !fillWasUserTriggered;
        if (!alreadyModified && isAutofillInput) {
          if (isPasswordType && filledLogin.password == field.value) {
            log(
              "Ignoring password input event that doesn't change autofilled values"
            );
            break;
          }
          if (
            !isPasswordType &&
            filledLogin.usernameField &&
            filledLogin.username == field.value
          ) {
            log(
              "Ignoring username input event that doesn't change autofilled values"
            );
            break;
          }
        }
        docState.fieldModificationsByRootElement.set(formLikeRoot, true);
        // Keep track of the modified formless password field to trigger form submission
        // when it is removed from DOM.
        let alreadyModifiedFormLessField = true;
        if (ChromeUtils.getClassName(formLikeRoot) !== "HTMLFormElement") {
          alreadyModifiedFormLessField = docState.formlessModifiedPasswordFields.has(
            field
          );
          if (!alreadyModifiedFormLessField) {
            docState.formlessModifiedPasswordFields.add(field);
          }
        }

        // Infer form submission only when there has been an user interaction on the form
        // or the formless password field.
        if (
          LoginHelper.formRemovalCaptureEnabled &&
          (!alreadyModified || !alreadyModifiedFormLessField)
        ) {
          ownerDocument.setNotifyFetchSuccess(true);
        }

        if (
          // When the password field value is cleared or entirely replaced we don't treat it as
          // an autofilled form any more. We don't do the same for username edits to avoid snooping
          // on the autofilled password in the resulting doorhanger
          isPasswordType &&
          loginManagerChild._doesEventClearPrevFieldValue(aEvent) &&
          // Don't clear last recorded autofill if THIS is an autofilled value. This will be true
          // when filling from the context menu.
          filledLogin &&
          filledLogin.password !== field.value
        ) {
          docState.fillsByRootElement.delete(formLikeRoot);
        }

        if (!LoginHelper.passwordEditCaptureEnabled) {
          break;
        }
        if (field.hasBeenTypePassword) {
          // When a field is filled with a generated password, we also fill a confirm password field
          // if found. To do this, _fillConfirmFieldWithGeneratedPassword calls setUserInput, which fires
          // an "input" event on the confirm password field. _compareAndUpdatePreviouslySentValues will
          // allow that message through due to triggeredByFillingGenerated, so early return here.
          let form = LoginFormFactory.createFromField(field);
          if (
            docState.generatedPasswordFields.has(field) &&
            LoginManagerChild.forWindow(window)._getFormFields(form)
              .confirmPasswordField === field
          ) {
            break;
          }
          // Don't check for triggeredByFillingGenerated, as we do not want to autosave
          // a field marked as a generated password field on every "input" event
          LoginManagerChild.forWindow(window)._passwordEditedOrGenerated(field);
        } else {
          let [usernameField, passwordField] = LoginManagerChild.forWindow(
            window
          ).getUserNameAndPasswordFields(field);
          if (
            usernameField &&
            field == usernameField &&
            passwordField &&
            passwordField.value
          ) {
            LoginManagerChild.forWindow(window)._passwordEditedOrGenerated(
              passwordField,
              {
                triggeredByFillingGenerated: docState.generatedPasswordFields.has(
                  passwordField
                ),
              }
            );
          }
        }
        break;
      }

      case "keydown": {
        if (
          aEvent.composedTarget.value &&
          (aEvent.keyCode == aEvent.DOM_VK_TAB ||
            aEvent.keyCode == aEvent.DOM_VK_RETURN)
        ) {
          const autofillForm =
            LoginHelper.autofillForms &&
            !PrivateBrowsingUtils.isContentWindowPrivate(
              ownerDocument.defaultView
            );

          if (autofillForm) {
            LoginManagerChild.forWindow(window).onUsernameAutocompleted(
              aEvent.composedTarget
            );
          }
        }
        break;
      }

      case "focus": {
        if (
          aEvent.composedTarget.hasBeenTypePassword &&
          docState.generatedPasswordFields.has(aEvent.composedTarget)
        ) {
          // Used to unmask fields with filled generated passwords when focused.
          let unmask = true;
          LoginManagerChild.forWindow(window)._togglePasswordFieldMasking(
            aEvent.composedTarget,
            unmask
          );
          break;
        }

        // Only used for username fields.
        LoginManagerChild.forWindow(window)._onUsernameFocus(aEvent);
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

  _storeUserInput(docState, field) {
    if (
      !Services.prefs.getBoolPref(
        "signon.capture.inputChanges.enabled",
        false
      ) ||
      !field.value
    ) {
      return;
    }

    if (LoginHelper.isPasswordFieldType(field)) {
      docState.possiblePasswords.add(field.value);
    } else if (LoginHelper.isUsernameFieldType(field)) {
      docState.possibleUsernames.add(field.value);
    }
  },
};

// Add this observer once for the process.
Services.obs.addObserver(observer, "autocomplete-did-enter-text");

this.LoginManagerChild = class LoginManagerChild extends JSWindowActorChild {
  constructor() {
    super();

    /**
     * WeakMap of the root element of a LoginForm to the DeferredTask to fill its fields.
     *
     * This is used to be able to throttle fills for a LoginForm since onDOMInputPasswordAdded gets
     * dispatched for each password field added to a document but we only want to fill once per
     * LoginForm when multiple fields are added at once.
     *
     * @type {WeakMap}
     */
    this._deferredPasswordAddedTasksByRootElement = new WeakMap();

    /**
     * WeakMap of a document to the array of callbacks to execute when it becomes visible
     *
     * This is used to defer handling DOMFormHasPassword and onDOMInputPasswordAdded events when the
     * containing document is hidden.
     * When the document first becomes visible, any queued events will be handled as normal.
     *
     * @type {WeakMap}
     */
    this._visibleTasksByDocument = new WeakMap();

    /**
     * Maps all DOM content documents in this content process, including those in
     * frames, to the current state used by the Login Manager.
     */
    this._loginFormStateByDocument = new WeakMap();

    /**
     * Set of fields where the user specifically requested password generation
     * (from the context menu) even if we wouldn't offer it on this field by default.
     */
    this._fieldsWithPasswordGenerationForcedOn = new WeakSet();
  }

  static forWindow(window) {
    let windowGlobal = window.windowGlobalChild;
    if (windowGlobal) {
      return windowGlobal.getActor("LoginManager");
    }

    return null;
  }

  /**
   * Stores passed arguments, and returns whether or not they match the args given the last time
   * this method was called with the same [formLikeRoot]. This is used to avoid sending duplicate
   * messages to the parent.
   *
   * @param {Element} formLikeRoot
   * @param {string} usernameValue
   * @param {string} passwordValue
   * @param {boolean?} [dismissed=false]
   * @param {boolean?} [triggeredByFillingGenerated=false] whether or not this call was triggered by a generated
   *        password being filled into a form-like element.
   *
   * @returns {boolean} true if args match the most recently passed values
   */
  _compareAndUpdatePreviouslySentValues(
    formLikeRoot,
    usernameValue,
    passwordValue,
    dismissed = false,
    triggeredByFillingGenerated = false
  ) {
    let state = this.stateForDocument(formLikeRoot.ownerDocument);
    const lastSentValues = state.lastSubmittedValuesByRootElement.get(
      formLikeRoot
    );
    if (lastSentValues) {
      if (dismissed && !lastSentValues.dismissed) {
        // preserve previous dismissed value if it was false (i.e. shown/open)
        dismissed = false;
      }
      if (
        lastSentValues.username == usernameValue &&
        lastSentValues.password == passwordValue &&
        lastSentValues.dismissed == dismissed &&
        lastSentValues.triggeredByFillingGenerated ==
          triggeredByFillingGenerated
      ) {
        log(
          "_compareAndUpdatePreviouslySentValues: values are equivalent, returning true"
        );
        return true;
      }
    }

    // Save the last submitted values so we don't prompt twice for the same values using
    // different capture methods e.g. a form submit event and upon navigation.
    state.lastSubmittedValuesByRootElement.set(formLikeRoot, {
      username: usernameValue,
      password: passwordValue,
      dismissed,
      triggeredByFillingGenerated,
    });
    log(
      "_compareAndUpdatePreviouslySentValues: values not equivalent, returning false"
    );
    return false;
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "PasswordManager:fillForm": {
        this.fillForm({
          loginFormOrigin: msg.data.loginFormOrigin,
          loginsFound: LoginHelper.vanillaObjectsToLogins(msg.data.logins),
          recipes: msg.data.recipes,
          inputElementIdentifier: msg.data.inputElementIdentifier,
          originMatches: msg.data.originMatches,
          style: msg.data.style,
        });
        break;
      }

      case "PasswordManager:useGeneratedPassword": {
        let inputElement = ContentDOMReference.resolve(
          msg.data.inputElementIdentifier
        );
        if (!inputElement) {
          log("Could not resolve inputElementIdentifier to a living element.");
          break;
        }

        if (inputElement != gFormFillService.focusedInput) {
          log("Could not open popup on input that's no longer focused");
          break;
        }

        this._fieldsWithPasswordGenerationForcedOn.add(inputElement);
        this.repopulateAutocompletePopup();
        break;
      }

      case "PasswordManager:repopulateAutocompletePopup": {
        this.repopulateAutocompletePopup();
        break;
      }

      case "PasswordManager:formIsPending": {
        return this._visibleTasksByDocument.has(this.document);
      }

      case "PasswordManager:formProcessed": {
        this.notifyObserversOfFormProcessed(msg.data.formid);
        break;
      }
    }

    return undefined;
  }

  repopulateAutocompletePopup() {
    // Clear the cache of previous autocomplete results to show new options.
    gFormFillService.QueryInterface(Ci.nsIAutoCompleteInput);
    gFormFillService.controller.resetInternalState();
    gFormFillService.showPopup();
  }

  shouldIgnoreLoginManagerEvent(event) {
    let nodePrincipal = event.target.nodePrincipal;
    // If we have a system or null principal then prevent any more password manager code from running and
    // incorrectly using the document `location`. Also skip password manager for about: pages.
    return (
      nodePrincipal.isSystemPrincipal ||
      nodePrincipal.isNullPrincipal ||
      nodePrincipal.schemeIs("about")
    );
  }

  handleEvent(event) {
    if (
      AppConstants.platform == "android" &&
      Services.prefs.getBoolPref("reftest.remote", false)
    ) {
      // XXX known incompatibility between reftest harness and form-fill. Is this still needed?
      return;
    }

    switch (event.type) {
      case "DOMDocFetchSuccess": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }

        this.onDOMDocFetchSuccess(event);
        break;
      }
      case "DOMFormBeforeSubmit": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }

        this.onDOMFormBeforeSubmit(event);
        break;
      }
      case "DOMFormHasPassword": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }

        this.onDOMFormHasPassword(event);
        let formLike = LoginFormFactory.createFromForm(event.originalTarget);
        InsecurePasswordUtils.reportInsecurePasswords(formLike);
        break;
      }
      case "DOMFormHasPossibleUsername": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }

        this.onDOMFormHasPossibleUsername(event);
        break;
      }
      case "DOMFormRemoved":
      case "DOMInputPasswordRemoved": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }
        this.onDOMFormRemoved(event);
        break;
      }
      case "DOMInputPasswordAdded": {
        if (this.shouldIgnoreLoginManagerEvent(event)) {
          break;
        }

        this.onDOMInputPasswordAdded(event, this.document.defaultView);
        let formLike = LoginFormFactory.createFromField(event.originalTarget);
        InsecurePasswordUtils.reportInsecurePasswords(formLike);
        break;
      }
    }
  }

  notifyObserversOfFormProcessed(formid) {
    Services.obs.notifyObservers(this, "passwordmgr-processed-form", formid);
  }

  /**
   * Get relevant logins and recipes from the parent
   *
   * @param {HTMLFormElement} form - form to get login data for
   * @param {Object} options
   * @param {boolean} options.guid - guid of a login to retrieve
   * @param {boolean} options.showMasterPassword - whether to show a master password prompt
   */
  _getLoginDataFromParent(form, options) {
    let actionOrigin = LoginHelper.getFormActionOrigin(form);

    let messageData = { actionOrigin, options };

    let resultPromise = this.sendQuery(
      "PasswordManager:findLogins",
      messageData
    );
    return resultPromise.then(result => {
      return {
        form,
        importable: result.importable,
        loginsFound: LoginHelper.vanillaObjectsToLogins(result.logins),
        recipes: result.recipes,
      };
    });
  }

  setupProgressListener(window) {
    if (!LoginHelper.formlessCaptureEnabled) {
      return;
    }

    // Get the highest accessible docshell and attach the progress listener to that.
    let docShell;
    for (
      let browsingContext = BrowsingContext.getFromWindow(window);
      browsingContext && browsingContext.docShell;
      browsingContext = browsingContext.parent
    ) {
      docShell = browsingContext.docShell;
    }

    try {
      let webProgress = docShell
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
  }

  /**
   * This method sets up form removal listener for form and password fields that
   * users have interacted with.
   */
  onDOMDocFetchSuccess(event) {
    let document = event.target;
    let docState = this.stateForDocument(document);
    let weakModificationsRootElements = ChromeUtils.nondeterministicGetWeakMapKeys(
      docState.fieldModificationsByRootElement
    );

    log(
      "onDOMDocFetchSuccess: modificationsByRootElement approx size:",
      weakModificationsRootElements.length,
      "document:",
      document
    );
    // Start to listen to form/password removed event after receiving a fetch/xhr
    // complete event.
    document.setNotifyFormOrPasswordRemoved(true);
    this.docShell.chromeEventHandler.addEventListener(
      "DOMFormRemoved",
      this,
      true
    );
    this.docShell.chromeEventHandler.addEventListener(
      "DOMInputPasswordRemoved",
      this,
      true
    );

    for (let rootElement of weakModificationsRootElements) {
      if (ChromeUtils.getClassName(rootElement) === "HTMLFormElement") {
        // If we create formLike when it is removed, we might not have the
        // right elements at that point, so create formLike object now.
        let formLike = LoginFormFactory.createFromForm(rootElement);
        docState.formLikeByObservedNode.set(rootElement, formLike);
      }
    }

    let weakFormlessModifiedPasswordFields = ChromeUtils.nondeterministicGetWeakSetKeys(
      docState.formlessModifiedPasswordFields
    );

    log(
      "onDOMDocFetchSuccess: formlessModifiedPasswordFields approx size:",
      weakFormlessModifiedPasswordFields.length,
      "document:",
      document
    );
    for (let passwordField of weakFormlessModifiedPasswordFields) {
      let formLike = LoginFormFactory.createFromField(passwordField);
      // force elements lazy getter being called.
      if (formLike.elements.length) {
        docState.formLikeByObservedNode.set(passwordField, formLike);
      }
    }

    // Observers have been setted up, removed the listener.
    document.setNotifyFetchSuccess(false);
  }

  /*
   * Trigger capture when a form/formless password is removed from DOM.
   * This method is used to capture logins for cases where form submit events
   * are not used.
   *
   * The heuristic works as follow:
   * 1. Set up 'DOMDocFetchSuccess' event listener when users have interacted
   *    with a form (by calling setNotifyFetchSuccess)
   * 2. After receiving `DOMDocFetchSuccess`, set up form removal event listener
   *    (see onDOMDocFetchSuccess)
   * 3. When a form is removed, onDOMFormRemoved triggers the login capture
   *    code.
   */
  onDOMFormRemoved(event) {
    let document = event.composedTarget.ownerDocument;
    let docState = this.stateForDocument(document);

    let formLike = docState.formLikeByObservedNode.get(event.target);
    if (!formLike) {
      return;
    }

    log("form is removed");
    this._onFormSubmit(formLike, SUBMIT_FORM_IS_REMOVED);

    docState.formLikeByObservedNode.delete(event.target);
    let weakObserveredNodes = ChromeUtils.nondeterministicGetWeakMapKeys(
      docState.formLikeByObservedNode
    );

    if (!weakObserveredNodes.length) {
      document.setNotifyFormOrPasswordRemoved(false);
      this.docShell.chromeEventHandler.removeEventListener(
        "DOMFormRemoved",
        this
      );
      this.docShell.chromeEventHandler.removeEventListener(
        "DOMInputPasswordRemoved",
        this
      );
    }
  }

  onDOMFormBeforeSubmit(event) {
    if (!event.isTrusted) {
      return;
    }

    // We're invoked before the content's |submit| event handlers, so we
    // can grab form data before it might be modified (see bug 257781).
    log("notified before form submission");
    let formLike = LoginFormFactory.createFromForm(event.target);
    this._onFormSubmit(formLike, SUBMIT_FORM_SUBMIT);
  }

  onDocumentVisibilityChange(event) {
    if (!event.isTrusted) {
      return;
    }
    let document = event.target;
    let onVisibleTasks = this._visibleTasksByDocument.get(document);
    if (!onVisibleTasks) {
      return;
    }
    for (let task of onVisibleTasks) {
      log("onDocumentVisibilityChange, executing queued task");
      task();
    }
    this._visibleTasksByDocument.delete(document);
  }

  _deferHandlingEventUntilDocumentVisible(event, document, fn) {
    log(
      `document.visibilityState: ${document.visibilityState}, defer handling ${event.type}`
    );
    let onVisibleTasks = this._visibleTasksByDocument.get(document);
    if (!onVisibleTasks) {
      log(
        `deferHandling, first queued event, register the visibilitychange handler`
      );
      onVisibleTasks = [];
      this._visibleTasksByDocument.set(document, onVisibleTasks);
      document.addEventListener(
        "visibilitychange",
        event => {
          this.onDocumentVisibilityChange(event);
        },
        { once: true }
      );
    }
    onVisibleTasks.push(fn);
  }

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
  }

  _processDOMFormHasPasswordEvent(event) {
    let form = event.target;
    let formLike = LoginFormFactory.createFromForm(form);
    log("_processDOMFormHasPasswordEvent:", form, formLike);
    this._fetchLoginsFromParentAndFillForm(formLike);
  }

  onDOMFormHasPossibleUsername(event) {
    if (!event.isTrusted) {
      return;
    }
    let isMasterPasswordSet = Services.cpmm.sharedData.get(
      "isMasterPasswordSet"
    );
    let document = event.target.ownerDocument;

    log(
      "onDOMFormHasPossibleUsername, visibilityState:",
      document.visibilityState,
      "isMasterPasswordSet:",
      isMasterPasswordSet
    );

    // For simplicity, the result of the telemetry is stacked. This means if a
    // document receives two `DOMFormHasPossibleEvent`, we add one counter to both
    // bucket 1 & 2.
    let docState = this.stateForDocument(document);
    Services.telemetry
      .getHistogramById("PWMGR_NUM_FORM_HAS_POSSIBLE_USERNAME_EVENT_PER_DOC")
      .add(++docState.numFormHasPossibleUsernameEvent);

    if (document.visibilityState == "visible" || isMasterPasswordSet) {
      this._processDOMFormHasPossibleUsernameEvent(event);
    } else {
      // wait until the document becomes visible before handling this event
      this._deferHandlingEventUntilDocumentVisible(event, document, () => {
        this._processDOMFormHasPossibleUsernameEvent(event);
      });
    }
  }

  _processDOMFormHasPossibleUsernameEvent(event) {
    let form = event.target;
    let formLike = LoginFormFactory.createFromForm(form);
    log("_processDOMFormHasPossibleUsernameEvent:", form, formLike);

    // If the form contains a passoword field, `getUsernameFieldFromUsernameOnlyForm` returns
    // null, so we don't trigger autofill for those forms here. In this function,
    // we only care about username-only forms. For forms contain a password, they'll be handled
    // in onDOMFormHasPassword.

    // We specifically set the recipe to empty here to avoid loading site recipes during page loads.
    // This is okay because if we end up finding a username-only form that should be ignore by
    // the site recipe, the form will be skipped while autofilling later.
    let usernameField = this.getUsernameFieldFromUsernameOnlyForm(form, {});
    if (usernameField) {
      // Autofill the username-only form.
      log(
        "_processDOMFormHasPossibleUsernameEvent: A username-only form is found"
      );
      this._fetchLoginsFromParentAndFillForm(formLike);
    }

    Services.telemetry
      .getHistogramById("PWMGR_IS_USERNAME_ONLY_FORM")
      .add(!!usernameField);
  }

  onDOMInputPasswordAdded(event, window) {
    if (!event.isTrusted) {
      return;
    }

    this.setupProgressListener(window);

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
      this._processDOMInputPasswordAddedEvent(event);
    } else {
      // wait until the document becomes visible before handling this event
      this._deferHandlingEventUntilDocumentVisible(event, document, () => {
        this._processDOMInputPasswordAddedEvent(event);
      });
    }
  }

  _processDOMInputPasswordAddedEvent(event) {
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
    } else if (
      ["interactive", "complete"].includes(window.document.readyState)
    ) {
      log(
        "Arming the DeferredTask we just created since document.readyState == 'interactive' or 'complete'"
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
  }

  /**
   * Fetch logins from the parent for a given form and then attempt to fill it.
   *
   * @param {LoginForm} form to fetch the logins for then try autofill.
   */
  _fetchLoginsFromParentAndFillForm(form) {
    if (!LoginHelper.enabled) {
      return;
    }

    // set up input event listeners so we know if the user has interacted with these fields
    // * input: Listen for the field getting blanked (without blurring) or a paste
    // * change: Listen for changes to the field filled with the generated password so we can preserve edits.
    form.rootElement.addEventListener("input", observer, {
      capture: true,
      mozSystemGroup: true,
    });
    form.rootElement.addEventListener("change", observer, {
      capture: true,
      mozSystemGroup: true,
    });

    this._getLoginDataFromParent(form, { showMasterPassword: true })
      .then(this.loginsFound.bind(this))
      .catch(Cu.reportError);
  }

  isPasswordGenerationForcedOn(passwordField) {
    return this._fieldsWithPasswordGenerationForcedOn.has(passwordField);
  }

  /**
   * Retrieves a reference to the state object associated with the given
   * document. This is initialized to an object with default values.
   */
  stateForDocument(document) {
    let loginFormState = this._loginFormStateByDocument.get(document);
    if (!loginFormState) {
      loginFormState = {
        /**
         * Keeps track of filled fields and values.
         */
        fillsByRootElement: new WeakMap(),
        /**
         * Keeps track of fields we've filled with generated passwords
         */
        generatedPasswordFields: new WeakFieldSet(),
        /**
         * Keeps track of logins that were last submitted.
         */
        lastSubmittedValuesByRootElement: new WeakMap(),
        fieldModificationsByRootElement: new WeakMap(),
        loginFormRootElements: new WeakSet(),
        /**
         * Anything entered into an <input> that we think might be a username
         */
        possibleUsernames: new Set(),
        /**
         * Anything entered into an <input> that we think might be a password
         */
        possiblePasswords: new Set(),

        /**
         * Keeps track of the formLike of nodes (form or formless password field)
         * that we are watching when they are removed from DOM.
         */
        formLikeByObservedNode: new WeakMap(),

        /**
         * Keeps track of all formless password fields that have been
         * updated by the user.
         */
        formlessModifiedPasswordFields: new WeakFieldSet(),

        /**
         * Caches the results of the username heuristics
         */
        cachedIsInferredUsernameField: new WeakMap(),
        cachedIsInferredEmailField: new WeakMap(),
        cachedIsInferredLoginForm: new WeakMap(),

        /**
         * Records the mock username field when its associated form is submitted.
         */
        mockUsernameOnlyField: null,

        /**
         * Records the number of possible username event received for this document.
         */
        numFormHasPossibleUsernameEvent: 0,
      };
      this._loginFormStateByDocument.set(document, loginFormState);
    }
    return loginFormState;
  }

  /**
   * Perform a password fill upon user request coming from the parent process.
   * The fill will be in the form previously identified during page navigation.
   *
   * @param An object with the following properties:
   *        {
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
   *          originMatches:
   *            True if the origin of the form matches the page URI.
   *        }
   */
  fillForm({
    loginFormOrigin,
    loginsFound,
    recipes,
    inputElementIdentifier,
    originMatches,
    style,
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

    if (!originMatches) {
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
    if (inputElement.hasBeenTypePassword) {
      clobberUsername = false;
    }

    this._fillForm(form, loginsFound, recipes, {
      inputElement,
      autofillForm: true,
      clobberUsername,
      clobberPassword: true,
      userTriggered: true,
      style,
    });
  }

  loginsFound({ form, importable, loginsFound, recipes }) {
    let doc = form.ownerDocument;
    let autofillForm =
      LoginHelper.autofillForms &&
      !PrivateBrowsingUtils.isContentWindowPrivate(doc.defaultView);

    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    LoginRecipesContent.cacheRecipes(formOrigin, doc.defaultView, recipes);

    this._fillForm(form, loginsFound, recipes, { autofillForm, importable });
  }

  /**
   * Focus event handler for username fields to decide whether to show autocomplete.
   * @param {FocusEvent} event
   */
  _onUsernameFocus(event) {
    let focusedField = event.target;
    if (
      !focusedField.mozIsTextField(true) ||
      focusedField.hasBeenTypePassword ||
      focusedField.readOnly
    ) {
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
    gFormFillService.showPopup();
  }

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
  }

  /**
   * A username field was filled or tabbed away from so try fill in the
   * associated password in the password field.
   */
  onUsernameAutocompleted(acInputField, loginGUID = null) {
    log("onUsernameAutocompleted:", acInputField);

    let acForm = LoginFormFactory.createFromField(acInputField);
    let doc = acForm.ownerDocument;
    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    let recipes = LoginRecipesContent.getRecipes(
      this,
      formOrigin,
      doc.defaultView
    );

    // Make sure the username field fillForm will use is the
    // same field as the autocomplete was activated on.
    let {
      usernameField,
      newPasswordField: passwordField,
    } = this._getFormFields(acForm, false, recipes);
    if (usernameField == acInputField) {
      // Fill the form when a password field is present.
      if (passwordField) {
        this._getLoginDataFromParent(acForm, {
          guid: loginGUID,
          showMasterPassword: false,
        })
          .then(({ form, loginsFound, recipes }) => {
            if (!loginGUID) {
              // not an explicit autocomplete menu selection, filter for exact matches only
              loginsFound = this._filterForExactFormOriginLogins(
                loginsFound,
                acForm
              );
              // filter the list for exact matches with the username
              // NOTE: this could be an empty string which is a valid username
              let searchString = usernameField.value.toLowerCase();
              loginsFound = loginsFound.filter(
                l => l.username.toLowerCase() == searchString
              );
            }

            this._fillForm(form, loginsFound, recipes, {
              autofillForm: true,
              clobberPassword: true,
              userTriggered: true,
            });
          })
          .catch(Cu.reportError);
        // Use `loginGUID !== null` to distinguish whether this is called when the
        // field is filled or tabbed away from. For the latter, don't highlight the field.
      } else if (loginGUID !== null) {
        this._highlightFilledField(usernameField);
      }
    } else {
      // Ignore the event, it's for some input we don't care about.
    }
  }

  /**
   * @param {LoginForm} form - the LoginForm to look for password fields in.
   * @param {Object} options
   * @param {bool} [options.skipEmptyFields=false] - Whether to ignore password fields with no value.
   *                                                 Used at capture time since saving empty values isn't
   *                                                 useful.
   * @param {Object} [options.fieldOverrideRecipe=null] - A relevant field override recipe to use.
   * @return {Array|null} Array of password field elements for the specified form.
   *                      If no pw fields are found, or if more than 5 are found, then null
   *                      is returned.
   */
  _getPasswordFields(
    form,
    {
      fieldOverrideRecipe = null,
      minPasswordLength = 0,
      ignoreConnect = false,
    } = {}
  ) {
    // Locate the password fields in the form.
    let pwFields = [];
    for (let i = 0; i < form.elements.length; i++) {
      let element = form.elements[i];
      if (
        ChromeUtils.getClassName(element) !== "HTMLInputElement" ||
        !element.hasBeenTypePassword ||
        (!element.isConnected && !ignoreConnect)
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
    if (!pwFields.length) {
      log("(form ignored -- no password fields.)");
      return null;
    } else if (pwFields.length > 5) {
      log(
        "(form ignored -- too many password fields. [ got ",
        pwFields.length,
        "])"
      );
      return null;
    }

    return pwFields;
  }

  /**
   * Returns the username and password fields found in the form.
   * Can handle complex forms by trying to figure out what the
   * relevant fields are.
   *
   * @param {LoginForm} form
   * @param {bool} isSubmission
   * @param {Set} recipes
   * @param {Object} options
   * @param {bool} [options.ignoreConnect] - Whether to ignore checking isConnected
   *                                         of the element.
   * @return {Object} {usernameField, newPasswordField, oldPasswordField, confirmPasswordField}
   *
   * usernameField may be null.
   * newPasswordField may be null. If null, this is a username-only form.
   * oldPasswordField may be null. If null, newPasswordField is just
   * "theLoginField". If not null, the form is apparently a
   * change-password field, with oldPasswordField containing the password
   * that is being changed.
   *
   * Note that even though we can create a LoginForm from a text field,
   * this method will only return a non-null usernameField if the
   * LoginForm has a password field.
   */
  _getFormFields(form, isSubmission, recipes, { ignoreConnect = false } = {}) {
    let usernameField = null;
    let newPasswordField = null;
    let oldPasswordField = null;
    let confirmPasswordField = null;
    let emptyResult = {
      usernameField: null,
      newPasswordField: null,
      oldPasswordField: null,
      confirmPasswordField: null,
    };

    let pwFields = null;
    let fieldOverrideRecipe = LoginRecipesContent.getFieldOverrides(
      recipes,
      form
    );
    if (fieldOverrideRecipe) {
      log("Has fieldOverrideRecipe", fieldOverrideRecipe);
      let pwOverrideField = LoginRecipesContent.queryLoginField(
        form,
        fieldOverrideRecipe.passwordSelector
      );
      if (pwOverrideField) {
        log("Has pwOverrideField", pwOverrideField);
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
        ignoreConnect,
      });
    }

    // Check whether this is a username-only form when the form doesn't have
    // a password field. Note that recipes are not supported in username-only
    // forms currently (Bug 1708455).
    if (!pwFields) {
      if (!LoginHelper.usernameOnlyFormEnabled) {
        return emptyResult;
      }

      usernameField = this.getUsernameFieldFromUsernameOnlyForm(
        form.rootElement,
        fieldOverrideRecipe
      );

      if (usernameField) {
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

      return {
        ...emptyResult,
        usernameField,
      };
    }

    if (!usernameField) {
      // Searching backwards from the first password field until we find a field
      // that looks like a "username" field. If no "username" field is found,
      // consider an email-like field a username field, if any.
      // If neither a username-like or an email-like field exists, assume the
      // first text field before the password field is the username.
      // We might not find a username field if the user is already logged in to the site.
      //
      // Note: We only search fields precede the first password field because we
      // don't see sites putting a username field after a password field. We can
      // extend searching to all fields in the form if this turns out not to be the case.

      for (let i = pwFields[0].index - 1; i >= 0; i--) {
        let element = form.elements[i];
        if (!LoginHelper.isUsernameFieldType(element, { ignoreConnect })) {
          continue;
        }

        if (
          fieldOverrideRecipe &&
          fieldOverrideRecipe.notUsernameSelector &&
          element.matches(fieldOverrideRecipe.notUsernameSelector)
        ) {
          continue;
        }

        // Assume the first text field is the username by default.
        // It will be replaced if we find a likely username field afterward.
        if (!usernameField) {
          usernameField = element;
        }

        if (this.isProbablyAUsernameField(element)) {
          // An username field is found, we are done.
          usernameField = element;
          break;
        } else if (this.isProbablyAnEmailField(element)) {
          // An email field is found, consider it a username field but continue
          // to search for an "username" field.
          // In current implementation, if another email field is found during
          // the process, we will use the new one.
          usernameField = element;
        }
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

    let { generatedPasswordFields } = this.stateForDocument(form.ownerDocument);
    let pwGeneratedFields = pwFields.filter(pwField =>
      generatedPasswordFields.has(pwField.element)
    );
    if (pwGeneratedFields.length) {
      // we have at least the newPasswordField
      [newPasswordField, confirmPasswordField] = pwGeneratedFields.map(
        pwField => pwField.element
      );
      // if the user filled a field with a generated password,
      // a field immediately previous to that is most likely the old password field
      let idx = pwFields.findIndex(
        pwField => pwField.element === newPasswordField
      );
      if (idx > 0) {
        oldPasswordField = pwFields[idx - 1].element;
      }
      return {
        ...emptyResult,
        usernameField,
        newPasswordField,
        oldPasswordField: oldPasswordField || null,
        confirmPasswordField: confirmPasswordField || null,
      };
    }

    // If we're not submitting a form (it's a page load), there are no
    // password field values for us to use for identifying fields. So,
    // just assume the first password field is the one to be filled in.
    if (!isSubmission || pwFields.length == 1) {
      let passwordField = pwFields[0].element;
      log("Password field", passwordField, "has name: ", passwordField.name);
      return {
        ...emptyResult,
        usernameField,
        newPasswordField: passwordField,
        oldPasswordField: null,
      };
    }

    // We're looking for both new and old password field
    // Try to figure out what is in the form based on the password values.
    let pw1 = pwFields[0].element.value;
    let pw2 = pwFields[1] ? pwFields[1].element.value : null;
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
        return emptyResult;
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
    return {
      ...emptyResult,
      usernameField,
      newPasswordField,
      oldPasswordField,
    };
  }

  /**
   * @return true if the page requests autocomplete be disabled for the
   *              specified element.
   */
  _isAutocompleteDisabled(element) {
    return element && element.autocomplete == "off";
  }

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
  }

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
      this._onFormSubmit(formLike, SUBMIT_PAGE_NAVIGATION);
    }
  }

  /**
   * Called by our observer when notified of a form submission.
   * [Note that this happens before any DOM onsubmit handlers are invoked.]
   * Looks for a password change in the submitted form, so we can update
   * our stored password.
   *
   * @param {LoginForm} form
   */
  _onFormSubmit(form, reason) {
    log("_onFormSubmit", form);

    this._maybeSendFormInteractionMessage(
      form,
      "PasswordManager:ShowDoorhanger",
      {
        targetField: null,
        isSubmission: true,
        // When this is trigger by inferring from form removal, the form is not
        // connected anymore, skip checking isConnected in this case.
        ignoreConnect: reason == SUBMIT_FORM_IS_REMOVED,
      }
    );
  }

  /**
   * Extracts and validates information from a form-like element on the page.  If validation is
   * successful, sends a message to the parent process requesting that it show a dialog.
   *
   * The validation works are divided into two parts:
   * 1. Whether this is a valid form with a password (validate in this function)
   * 2. Whether the password manager decides to send interaction message for this form
   *    (validate in _maybeSendFormInteractionMessageContinue)
   *
   * When the function is triggered by a form submission event, and the form is valid (pass #1),
   * We still send the message to the parent even the validation of #2 fails. This is because
   * there might be someone who is interested in form submission events regardless of whether
   * the password manager decides to show the doorhanger or not.
   *
   * @param {LoginForm} form
   * @param {string} messageName used to categorize the type of message sent to the parent process.
   * @param {Element?} options.targetField
   * @param {boolean} options.isSubmission if true, this function call was prompted by a form submission.
   * @param {boolean?} options.triggeredByFillingGenerated whether or not this call was triggered by a
   *        generated password being filled into a form-like element.
   * @param {boolean?} options.ignoreConnect Whether to ignore isConnected attribute of a element.
   *
   * @returns {Boolean} whether the message is sent to the parent process.
   */
  _maybeSendFormInteractionMessage(
    form,
    messageName,
    { targetField, isSubmission, triggeredByFillingGenerated, ignoreConnect }
  ) {
    let doc = form.ownerDocument;
    let win = doc.defaultView;
    let logMessagePrefix = isSubmission ? "form submission" : "field edit";
    let passwordField = null;
    if (targetField && targetField.hasBeenTypePassword) {
      passwordField = targetField;
    }

    let origin = LoginHelper.getLoginOrigin(doc.documentURI);
    if (!origin) {
      log(`(${logMessagePrefix} ignored -- invalid origin)`);
      return;
    }

    // Get the appropriate fields from the form.
    let recipes = LoginRecipesContent.getRecipes(this, origin, win);
    let fields = {
      targetField,
      ...this._getFormFields(form, true, recipes, { ignoreConnect }),
    };

    // It's possible the field triggering this message isn't one of those found by _getFormFields' heuristics
    if (
      passwordField &&
      passwordField != fields.newPasswordField &&
      passwordField != fields.oldPasswordField &&
      passwordField != fields.confirmPasswordField
    ) {
      fields.newPasswordField = passwordField;
    }

    // Need at least 1 valid password field to do anything.
    if (fields.newPasswordField == null) {
      if (isSubmission && fields.usernameField) {
        log(
          "_onFormSubmit: username-only form. Record the username field but not sending prompt"
        );
        this.stateForDocument(doc).mockUsernameOnlyField = {
          name: fields.usernameField.name,
          value: fields.usernameField.value,
        };
      }
      return;
    }

    this._maybeSendFormInteractionMessageContinue(form, messageName, {
      ...fields,
      isSubmission,
      triggeredByFillingGenerated,
    });

    if (isSubmission) {
      // Notify `PasswordManager:onFormSubmit` as long as we detect submission event on a
      // valid form with a password field.
      this.sendAsyncMessage(
        "PasswordManager:onFormSubmit",
        {},
        {
          fields,
          isSubmission,
          triggeredByFillingGenerated,
        }
      );
    }
  }

  /**
   * Continues the works that are not done in _maybeSendFormInteractionMessage.
   * See comments in _maybeSendFormInteractionMessage for more details.
   */
  _maybeSendFormInteractionMessageContinue(
    form,
    messageName,
    {
      targetField,
      usernameField,
      newPasswordField,
      oldPasswordField,
      confirmPasswordField,
      isSubmission,
      triggeredByFillingGenerated,
    }
  ) {
    let logMessagePrefix = isSubmission ? "form submission" : "field edit";
    let doc = form.ownerDocument;
    let win = doc.defaultView;

    let detail = { messageSent: false };
    try {
      // when filling a generated password, we do still want to message the parent
      if (
        !triggeredByFillingGenerated &&
        PrivateBrowsingUtils.isContentWindowPrivate(win) &&
        !LoginHelper.privateBrowsingCaptureEnabled
      ) {
        // We won't do anything in private browsing mode anyway,
        // so there's no need to perform further checks.
        log(`(${logMessagePrefix} ignored in private browsing mode)`);
        return;
      }

      // If password saving is disabled globally, bail out now.
      if (!LoginHelper.enabled) {
        return;
      }

      let fullyMungedPattern = /^\*+$|^•+$|^\.+$/;
      // Check `isSubmission` to allow munged passwords in dismissed by default doorhangers (since
      // they are initiated by the user) in case this matches their actual password.
      if (isSubmission && newPasswordField?.value.match(fullyMungedPattern)) {
        log("new password looks munged. Not sending prompt");
        return;
      }

      // When the username field is empty, check whether we have found it previously from
      // a username-only form, if yes, fill in its value.
      // XXX This is not ideal, we only use the previous saved username field when the current
      // form doesn't have one. This means if there is a username field found in the current
      // form, we don't compare it to the saved one, which might be a better choice in some cases.
      // The reason we are not doing it now is because we haven't found a real world example.
      let docState = this.stateForDocument(doc);
      if (!usernameField) {
        if (docState.mockUsernameOnlyField) {
          usernameField = docState.mockUsernameOnlyField;
        }
      }
      if (usernameField && usernameField.value.match(/\.{3,}|\*{3,}|•{3,}/)) {
        log(
          `usernameField.value "${usernameField.value}" looks munged, setting to null`
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
        log(`(${logMessagePrefix} ignored -- autocomplete=off found)`);
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
      // Dismiss prompt if the username field is a credit card number AND
      // if the password field is a three digit number. Also dismiss prompt if
      // the password is a credit card number and the password field has attribute
      // autocomplete="cc-number".
      let dismissedPrompt = !isSubmission;
      let newPasswordFieldValue = newPasswordField.value;
      if (
        (!dismissedPrompt &&
          CreditCard.isValidNumber(usernameValue) &&
          newPasswordFieldValue.trim().match(/^[0-9]{3}$/)) ||
        (CreditCard.isValidNumber(newPasswordFieldValue) &&
          newPasswordField.getAutocompleteInfo().fieldName == "cc-number")
      ) {
        dismissedPrompt = true;
      }

      let fieldsModified = this._formHasModifiedFields(form);
      if (!fieldsModified && LoginHelper.userInputRequiredToCapture) {
        if (targetField) {
          throw new Error("No user input on targetField");
        }
        // we know no fields in this form had user modifications, so don't prompt
        log(
          `(${logMessagePrefix} ignored -- submitting values that are not changed by the user)`
        );
        return;
      }

      if (
        this._compareAndUpdatePreviouslySentValues(
          form.rootElement,
          usernameValue,
          newPasswordField.value,
          dismissedPrompt,
          triggeredByFillingGenerated
        )
      ) {
        log(
          `(${logMessagePrefix} ignored -- already submitted with the same username and password)`
        );
        return;
      }

      let { login: autoFilledLogin } =
        docState.fillsByRootElement.get(form.rootElement) || {};
      let browsingContextId = win.windowGlobalChild.browsingContext.id;
      let formActionOrigin = LoginHelper.getFormActionOrigin(form);

      detail = {
        browsingContextId,
        formActionOrigin,
        autoFilledLoginGuid: autoFilledLogin && autoFilledLogin.guid,
        usernameField: mockUsername,
        newPasswordField: mockPassword,
        oldPasswordField: mockOldPassword,
        dismissedPrompt,
        triggeredByFillingGenerated,
        possibleValues: {
          usernames: docState.possibleUsernames,
          passwords: docState.possiblePasswords,
        },
        messageSent: true,
      };

      this.sendAsyncMessage(messageName, detail);
    } catch (ex) {
      Cu.reportError(ex);
      throw ex;
    } finally {
      detail.form = form;
      const evt = new CustomEvent(messageName, { detail });
      win.windowRoot.dispatchEvent(evt);
    }
  }

  /**
   * Track a form field as has having been filled with a generated password. This adds explicit
   * focus & blur handling to unmask & mask the value, and enables special handling of edits to
   * generated password values (see the observer's input event handler.)
   *
   * @param {HTMLInputElement} passwordField
   */
  _treatAsGeneratedPasswordField(passwordField) {
    let docState = this.stateForDocument(passwordField.ownerDocument);
    docState.generatedPasswordFields.add(passwordField);

    // blur/focus: listen for focus changes to we can mask/unmask generated passwords
    for (let eventType of ["blur", "focus"]) {
      passwordField.addEventListener(eventType, observer, {
        capture: true,
        mozSystemGroup: true,
      });
    }
    if (passwordField.ownerDocument.activeElement == passwordField) {
      // Unmask the password field
      this._togglePasswordFieldMasking(passwordField, true);
    }
  }

  /**
   * Heuristic for whether or not we should consider [field]s value to be 'new' (as opposed to
   * 'changed') after applying [event].
   *
   * @param {HTMLInputElement} event.target input element being changed.
   * @param {string?} event.data new value being input into the field.
   *
   * @returns {boolean}
   */
  _doesEventClearPrevFieldValue({ target, data, inputType }) {
    return (
      !target.value ||
      // We check inputType here as a proxy for the previous field value.
      // If the previous field value was empty, e.g. automatically filling
      // a confirm password field when a new password field is filled with
      // a generated password, there's nothing to replace.
      // We may be able to use the "beforeinput" event instead when that
      // ships (Bug 1609291).
      (data && data == target.value && inputType !== "insertReplacementText")
    );
  }

  _stopTreatingAsGeneratedPasswordField(passwordField) {
    log("_stopTreatingAsGeneratedPasswordField");

    let fields = this.stateForDocument(passwordField.ownerDocument)
      .generatedPasswordFields;
    fields.delete(passwordField);

    // Remove all the event listeners added in _passwordEditedOrGenerated
    for (let eventType of ["blur", "focus"]) {
      passwordField.removeEventListener(eventType, observer, {
        capture: true,
        mozSystemGroup: true,
      });
    }

    // Mask the password field
    this._togglePasswordFieldMasking(passwordField, false);
  }

  /**
   * The password field has been filled with a generated password, ensure the
   * field is handled accordingly.
   * @param {HTMLInputElement} passwordField
   */
  _filledWithGeneratedPassword(passwordField) {
    this._highlightFilledField(passwordField);
    this._passwordEditedOrGenerated(passwordField, {
      triggeredByFillingGenerated: true,
    });
    this._fillConfirmFieldWithGeneratedPassword(passwordField);
  }

  /**
   * Notify the parent that we are ignoring the password edit
   * so that tests can listen for this as opposed to waiting for
   * nothing to happen.
   */
  _ignorePasswordEdit() {
    if (Cu.isInAutomation) {
      this.sendAsyncMessage("PasswordManager:onIgnorePasswordEdit", {});
    }
  }
  /**
   * Notify the parent that a generated password was filled into a field or
   * edited so that it can potentially be saved.
   * @param {HTMLInputElement} passwordField
   */
  _passwordEditedOrGenerated(
    passwordField,
    { triggeredByFillingGenerated = false } = {}
  ) {
    log("_passwordEditedOrGenerated", passwordField);

    if (!LoginHelper.enabled && triggeredByFillingGenerated) {
      throw new Error(
        "A generated password was filled while the password manager was disabled."
      );
    }

    let loginForm = LoginFormFactory.createFromField(passwordField);

    if (triggeredByFillingGenerated) {
      this._highlightFilledField(passwordField);
      this._treatAsGeneratedPasswordField(passwordField);

      // Once the generated password was filled we no longer want to autocomplete
      // saved logins into a non-empty password field (see LoginAutoComplete.startSearch)
      // because it is confusing.
      this._fieldsWithPasswordGenerationForcedOn.delete(passwordField);
    }

    this._maybeSendFormInteractionMessage(
      loginForm,
      "PasswordManager:onPasswordEditedOrGenerated",
      {
        targetField: passwordField,
        isSubmission: false,
        triggeredByFillingGenerated,
      }
    );
  }

  _fillConfirmFieldWithGeneratedPassword(passwordField) {
    // Fill a nearby password input if it looks like a confirm-password field
    let form = LoginFormFactory.createFromField(passwordField);
    let confirmPasswordInput = null;
    let docState = this.stateForDocument(passwordField.ownerDocument);
    // The confirm-password field shouldn't be more than 3 form elements away from the password field we filled
    let MAX_CONFIRM_PASSWORD_DISTANCE = 3;

    let startIndex = form.elements.indexOf(passwordField);
    if (startIndex == -1) {
      throw new Error(
        "Password field is not in the form's elements collection"
      );
    }

    // If we've already filled another field with a generated password,
    // this might be the confirm-password field, so don't try and find another
    let previousGeneratedPasswordField = form.elements.some(
      inp => inp !== passwordField && docState.generatedPasswordFields.has(inp)
    );
    if (previousGeneratedPasswordField) {
      log(
        "_fillConfirmFieldWithGeneratedPassword, previously-filled generated password input found"
      );
      return;
    }

    // Get a list of input fields to search in.
    // Pre-filter type=hidden fields; they don't count against the distance threshold
    let afterFields = form.elements
      .slice(startIndex + 1)
      .filter(elem => elem.type !== "hidden");

    let acFieldName = passwordField.getAutocompleteInfo()?.fieldName;

    // Match same autocomplete values first
    if (acFieldName && acFieldName == "new-password") {
      let matchIndex = afterFields.findIndex(
        elem =>
          LoginHelper.isPasswordFieldType(elem) &&
          elem.getAutocompleteInfo().fieldName == acFieldName &&
          !elem.disabled &&
          !elem.readOnly
      );
      if (matchIndex >= 0 && matchIndex < MAX_CONFIRM_PASSWORD_DISTANCE) {
        confirmPasswordInput = afterFields[matchIndex];
      }
    }
    if (!confirmPasswordInput) {
      for (
        let idx = 0;
        idx < Math.min(MAX_CONFIRM_PASSWORD_DISTANCE, afterFields.length);
        idx++
      ) {
        if (
          LoginHelper.isPasswordFieldType(afterFields[idx]) &&
          !afterFields[idx].disabled &&
          !afterFields[idx].readOnly
        ) {
          confirmPasswordInput = afterFields[idx];
          break;
        }
      }
    }
    if (confirmPasswordInput && !confirmPasswordInput.value) {
      this._treatAsGeneratedPasswordField(confirmPasswordInput);
      confirmPasswordInput.setUserInput(passwordField.value);
      this._highlightFilledField(confirmPasswordInput);
    }
  }

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
  }

  /** Remove login field highlight when its value is cleared or overwritten.
   */
  _removeFillFieldHighlight(event) {
    let winUtils = event.target.ownerGlobal.windowUtils;
    winUtils.removeManuallyManagedState(event.target, AUTOFILL_STATE);
  }

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
  }

  /**
   * Filter logins for exact origin/formActionOrigin and dedupe on usernamematche
   * @param {nsILoginInfo[]} logins an array of nsILoginInfo that could be
   *        used for the form, including ones with a different form action origin
   *        which are only used when the fill is userTriggered
   * @param {LoginForm} form
   */
  _filterForExactFormOriginLogins(logins, form) {
    let loginOrigin = LoginHelper.getLoginOrigin(
      form.ownerDocument.documentURI
    );
    let formActionOrigin = LoginHelper.getFormActionOrigin(form);
    logins = logins.filter(l => {
      let formActionMatches = LoginHelper.isOriginMatching(
        l.formActionOrigin,
        formActionOrigin,
        {
          schemeUpgrades: LoginHelper.schemeUpgrades,
          acceptWildcardMatch: true,
          acceptDifferentSubdomains: true,
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
    logins = LoginHelper.dedupeLogins(
      logins,
      ["username"],
      ["scheme", "timePasswordChanged"],
      loginOrigin,
      formActionOrigin
    );
    return logins;
  }

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
      importable = null,
      clobberUsername = false,
      clobberPassword = false,
      userTriggered = false,
      style = null,
    } = {}
  ) {
    if (ChromeUtils.getClassName(form) === "HTMLFormElement") {
      throw new Error("_fillForm should only be called with LoginForm objects");
    }

    log("_fillForm", form.elements);
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
      TYPE_NO_LONGER_PASSWORD: 12,
      FORM_IN_CROSSORIGIN_SUBFRAME: 13,
      FILLED_USERNAME_ONLY_FORM: 14,
    };

    // Heuristically determine what the user/pass fields are
    // We do this before checking to see if logins are stored,
    // so that the user isn't prompted for a master password
    // without need.
    let {
      usernameField,
      newPasswordField: passwordField,
    } = this._getFormFields(form, false, recipes);

    try {
      // Nothing to do if we have no matching (excluding form action
      // checks) logins available, and there isn't a need to show
      // the insecure form warning.
      if (
        !foundLogins.length &&
        !(importable?.state === "import" && importable?.browsers) &&
        (InsecurePasswordUtils.isFormSecure(form) ||
          !LoginHelper.showInsecureFieldWarning)
      ) {
        // We don't log() here since this is a very common case.
        autofillResult = AUTOFILL_RESULT.NO_SAVED_LOGINS;
        return;
      }

      // If we have a password inputElement parameter and it's not
      // the same as the one heuristically found, use the parameter
      // one instead.
      if (inputElement) {
        if (inputElement.hasBeenTypePassword) {
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

      // Need a valid password or username field to do anything.
      if (passwordField == null && usernameField == null) {
        log("not filling form, no password and username field found");
        autofillResult = AUTOFILL_RESULT.NO_PASSWORD_FIELD;
        return;
      }

      // If the password field is disabled or read-only, there's nothing to do.
      if (passwordField?.disabled || passwordField?.readOnly) {
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
        gFormFillService.markAsLoginManagerField(usernameField);
        usernameField.addEventListener("keydown", observer);
      }

      if (
        !userTriggered &&
        !form.rootElement.ownerGlobal.windowGlobalChild.sameOriginWithTop
      ) {
        log("not filling form; it is in a cross-origin subframe");
        autofillResult = AUTOFILL_RESULT.FORM_IN_CROSSORIGIN_SUBFRAME;
        return;
      }

      if (!userTriggered) {
        // Only autofill logins that match the form's action and origin. In the above code
        // we have attached autocomplete for logins that don't match the form action.
        foundLogins = this._filterForExactFormOriginLogins(foundLogins, form);
      }

      // Nothing to do if we have no matching logins available.
      // Only insecure pages reach this block and logs the same
      // telemetry flag.
      if (!foundLogins.length) {
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
      if (usernameField?.maxLength >= 0) {
        maxUsernameLen = usernameField.maxLength;
      }
      if (passwordField?.maxLength >= 0) {
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

      if (!logins.length) {
        log("form not filled, none of the logins fit in the field");
        autofillResult = AUTOFILL_RESULT.NO_LOGINS_FIT;
        return;
      }

      const passwordACFieldName = passwordField?.getAutocompleteInfo()
        .fieldName;

      if (passwordField) {
        if (!userTriggered && passwordField.type != "password") {
          // We don't want to autofill (without user interaction) into a field
          // that's unmasked.
          log("not autofilling, password field isn't currently type=password");
          autofillResult = AUTOFILL_RESULT.TYPE_NO_LONGER_PASSWORD;
          return;
        }

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
        if (!matchingLogins.length) {
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

      let doc = form.ownerDocument;
      let willAutofill =
        usernameField || passwordField.value != selectedLogin.password;
      if (willAutofill) {
        let autoFilledLogin = {
          guid: selectedLogin.QueryInterface(Ci.nsILoginMetaInfo).guid,
          username: selectedLogin.username,
          usernameField: usernameField
            ? Cu.getWeakReference(usernameField)
            : null,
          password: selectedLogin.password,
          passwordField: passwordField
            ? Cu.getWeakReference(passwordField)
            : null,
        };
        // Ensure the state is updated before setUserInput is called.
        log(
          "Saving autoFilledLogin",
          autoFilledLogin.guid,
          "for",
          form.rootElement
        );
        this.stateForDocument(doc).fillsByRootElement.set(form.rootElement, {
          login: autoFilledLogin,
          userTriggered,
        });
      }
      if (usernameField) {
        // Don't modify the username field because the user wouldn't be able to change it either.
        let disabledOrReadOnly =
          usernameField.disabled || usernameField.readOnly;

        if (selectedLogin.username && !disabledOrReadOnly) {
          let userNameDiffers = selectedLogin.username != usernameField.value;
          // Don't replace the username if it differs only in case, and the user triggered
          // this autocomplete. We assume that if it was user-triggered the entered text
          // is desired.
          let userEnteredDifferentCase =
            userTriggered &&
            userNameDiffers &&
            usernameField.value.toLowerCase() ==
              selectedLogin.username.toLowerCase();

          if (!userEnteredDifferentCase && userNameDiffers) {
            usernameField.setUserInput(selectedLogin.username);
          }
          this._highlightFilledField(usernameField);
        }
      }

      if (passwordField) {
        if (passwordField.value != selectedLogin.password) {
          // Ensure the field gets re-masked in case a generated password was
          // filled into it previously.
          this._stopTreatingAsGeneratedPasswordField(passwordField);

          passwordField.setUserInput(selectedLogin.password);
        }

        this._highlightFilledField(passwordField);
      }

      if (style && style === "generatedPassword") {
        this._filledWithGeneratedPassword(passwordField);
      }

      log("_fillForm succeeded");
      if (passwordField) {
        autofillResult = AUTOFILL_RESULT.FILLED;
      } else if (usernameField) {
        autofillResult = AUTOFILL_RESULT.FILLED_USERNAME_ONLY_FORM;
      }
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
          let focusedElement = gFormFillService.focusedInput;
          if (
            usernameField == focusedElement &&
            ![
              AUTOFILL_RESULT.FILLED,
              AUTOFILL_STATE.FILLED_USERNAME_ONLY_FORM,
            ].includes(autofillResult)
          ) {
            log(
              "_fillForm: Opening username autocomplete popup since the form wasn't autofilled"
            );
            gFormFillService.showPopup();
          }
        }
      }

      if (usernameField) {
        log("_fillForm: Attaching event listeners to usernameField");
        usernameField.addEventListener("focus", observer);
        usernameField.addEventListener("mousedown", observer);
      }

      this.sendAsyncMessage("PasswordManager:formProcessed", {
        formid: form.rootElement.id,
      });
    }
  }

  _formHasModifiedFields(form) {
    let doc = form.rootElement.ownerDocument;
    let userHasInteracted;
    let testOnlyUserHasInteracted =
      LoginHelper.testOnlyUserHasInteractedWithDocument;
    if (Cu.isInAutomation && testOnlyUserHasInteracted !== null) {
      userHasInteracted = testOnlyUserHasInteracted;
    } else {
      userHasInteracted = doc.userHasInteracted;
    }

    log("_formHasModifiedFields, userHasInteracted:", userHasInteracted);

    // If the user hasn't interacted at all with the page, we don't need to check futher
    if (!userHasInteracted) {
      return false;
    }
    let state = this.stateForDocument(doc);
    // check for user inputs to the form fields
    let fieldsModified = state.fieldModificationsByRootElement.get(
      form.rootElement
    );
    // also consider a form modified if there's a difference between fields' .value and .defaultValue
    if (!fieldsModified) {
      fieldsModified = Array.from(form.elements).some(
        field =>
          field.defaultValue !== undefined && field.value !== field.defaultValue
      );
    }
    return fieldsModified;
  }

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
    let { login: filledLogin } =
      this.stateForDocument(
        aUsernameField.ownerDocument
      ).fillsByRootElement.get(formLikeRoot) || {};
    if (!filledLogin) {
      return false;
    }

    // Unpack the weak references.
    let autoFilledUsernameField = filledLogin.usernameField?.get();
    let autoFilledPasswordField = filledLogin.passwordField?.get();

    // Check username and password values match what was filled.
    if (
      !autoFilledUsernameField ||
      autoFilledUsernameField != aUsernameField ||
      autoFilledUsernameField.value != filledLogin.username ||
      (autoFilledPasswordField &&
        autoFilledPasswordField.value != filledLogin.password)
    ) {
      return false;
    }

    return true;
  }

  /**
   * Returns the username and password fields found in the form by input
   * element into form.
   *
   * @param {HTMLInputElement} aField
   *                           A form field
   * @return {Array} [usernameField, newPasswordField, oldPasswordField]
   *
   * Details of these values are the same as _getFormFields.
   */
  getUserNameAndPasswordFields(aField) {
    let noResult = [null, null, null];
    if (ChromeUtils.getClassName(aField) !== "HTMLInputElement") {
      throw new Error("getUserNameAndPasswordFields: input element required");
    }

    if (aField.nodePrincipal.isNullPrincipal || !aField.isConnected) {
      return noResult;
    }

    // If the element is not a login form field, return all null.
    if (
      !aField.hasBeenTypePassword &&
      !LoginHelper.isUsernameFieldType(aField)
    ) {
      return noResult;
    }

    let form = LoginFormFactory.createFromField(aField);
    let doc = aField.ownerDocument;
    let formOrigin = LoginHelper.getLoginOrigin(doc.documentURI);
    let recipes = LoginRecipesContent.getRecipes(
      this,
      formOrigin,
      doc.defaultView
    );
    let {
      usernameField,
      newPasswordField,
      oldPasswordField,
    } = this._getFormFields(form, false, recipes);

    return [usernameField, newPasswordField, oldPasswordField];
  }

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
      (!aField.hasBeenTypePassword &&
        !LoginHelper.isUsernameFieldType(aField)) ||
      aField.nodePrincipal.isNullPrincipal ||
      aField.nodePrincipal.schemeIs("about") ||
      !aField.ownerDocument
    ) {
      return null;
    }
    let { hasBeenTypePassword } = aField;

    // This array provides labels that correspond to the return values from
    // `getUserNameAndPasswordFields` so we can know which one aField is.
    const LOGIN_FIELD_ORDER = ["username", "new-password", "current-password"];
    let usernameAndPasswordFields = this.getUserNameAndPasswordFields(aField);
    let fieldNameHint;
    let indexOfFieldInUsernameAndPasswordFields = usernameAndPasswordFields.indexOf(
      aField
    );
    if (indexOfFieldInUsernameAndPasswordFields == -1) {
      // For fields in the form that are neither username nor password,
      // set fieldNameHint to "other". Right now, in contextmenu, we treat both
      // "username" and "other" field as username fields.
      fieldNameHint = hasBeenTypePassword ? "current-password" : "other";
    } else {
      fieldNameHint =
        LOGIN_FIELD_ORDER[indexOfFieldInUsernameAndPasswordFields];
    }
    let [, newPasswordField] = usernameAndPasswordFields;

    return {
      activeField: {
        disabled: aField.disabled || aField.readOnly,
        fieldNameHint,
      },
      // `passwordField` may be the same as `activeField`.
      passwordField: {
        found: !!newPasswordField,
        disabled:
          newPasswordField &&
          (newPasswordField.disabled || newPasswordField.readOnly),
      },
    };
  }

  /**
   * Returns the username field of the passed form if the form is a
   * username-only form.
   * A form is considered a username-only form only if it meets all the
   * following conditions:
   * 1. Does not have any password field,
   * 2. Only contains one input field whose type is username compatible.
   * 3. The username compatible input field looks like a username field
   *    or the form itself looks like a sign-in or sign-up form.
   *
   * @param {Element} formElement
   *                  the form to check.
   * @param {Object}  recipe=null
   *                  A relevant field override recipe to use.
   * @returns {Element} The username field or null (if the form is not a
   *                    username-only form).
   */
  getUsernameFieldFromUsernameOnlyForm(formElement, recipe = null) {
    if (ChromeUtils.getClassName(formElement) !== "HTMLFormElement") {
      return null;
    }

    let candidate = null;
    for (let element of formElement.elements) {
      // Only care input fields in the form.
      if (ChromeUtils.getClassName(element) !== "HTMLInputElement") {
        continue;
      }

      // We are looking for a username-only form, so if there is a password
      // field in the form, this is NOT a username-only form.
      if (element.hasBeenTypePassword) {
        return null;
      }

      // Ignore input fields whose type are not username compatiable, ex, hidden.
      if (!LoginHelper.isUsernameFieldType(element)) {
        continue;
      }

      if (
        recipe?.notUsernameSelector &&
        element.matches(recipe.notUsernameSelector)
      ) {
        continue;
      }

      // If there are more than two input fields whose type is username
      // compatiable, this is NOT a username-only form.
      if (candidate) {
        return null;
      }
      candidate = element;
    }

    if (
      candidate &&
      this.isProbablyAUsernameLoginForm(formElement, candidate)
    ) {
      return candidate;
    }

    return null;
  }

  /**
   * Returns true if the input field is considered a username field by
   * 'LoginHelper.isInferredUsernameField'. The main purpose of this method
   * is to cache the result because _getFormFields has many call sites and we
   * want to avoid applying the heuristic every time.
   *
   * @param {Element} element the field to check.
   * @returns {boolean} True if the element is likely a username field
   */
  isProbablyAUsernameField(inputElement) {
    let docState = this.stateForDocument(inputElement.ownerDocument);
    let result = docState.cachedIsInferredUsernameField.get(inputElement);
    if (result === undefined) {
      result = LoginHelper.isInferredUsernameField(inputElement);
      docState.cachedIsInferredUsernameField.set(inputElement, result);
    }

    return result;
  }

  /**
   * Returns true if the input field is considered an email field by
   * 'LoginHelper.isInferredEmailField'.
   *
   * @param {Element} element the field to check.
   * @returns {boolean} True if the element is likely an email field
   */
  isProbablyAnEmailField(inputElement) {
    let docState = this.stateForDocument(inputElement.ownerDocument);
    let result = docState.cachedIsInferredEmailField.get(inputElement);
    if (result === undefined) {
      result = LoginHelper.isInferredEmailField(inputElement);
      docState.cachedIsInferredEmailField.set(inputElement, result);
    }

    return result;
  }

  /**
   * Returns true if the form is considered a username login form if
   * 1. The input element looks like a username field or the form looks
   *    like a login form
   * 2. The input field doesn't match keywords that indicate the username
   *    is not used for login (ex, search) or the login form is not use
   *    a username to sign-in (ex, authentication code)
   *
   * @param {Element} element the form to check.
   * @returns {boolean} True if the element is likely a login form
   */
  isProbablyAUsernameLoginForm(formElement, inputElement) {
    let docState = this.stateForDocument(formElement.ownerDocument);
    let result = docState.cachedIsInferredLoginForm.get(formElement);
    if (result === undefined) {
      // We should revisit these rules after we collect more positive or negative
      // cases for username-only forms. Right now, if-else-based rules are good
      // enough to cover the sites we know, but if we find out defining "weight" for each
      // rule is necessary to improve the heuristic, we should consider switching
      // this with Fathom.

      result = false;
      // Check whether the input field looks like a username field or the
      // form looks like a sign-in or sign-up form.
      if (
        this.isProbablyAUsernameField(inputElement) ||
        LoginHelper.isInferredLoginForm(formElement)
      ) {
        // This is where we collect hints that indicate this is not a username
        // login form.
        if (!LoginHelper.isInferredNonUsernameField(inputElement)) {
          result = true;
        }
      }
      docState.cachedIsInferredLoginForm.set(formElement, result);
    }

    return result;
  }
};
