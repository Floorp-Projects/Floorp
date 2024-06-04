/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The FormHandler actor pair implements the logic of detecting
 * form submissions and notifies of a form submission by
 * dispatching the event "form-submission-detected"
 */

export const FORM_SUBMISSION_REASON = {
  FORM_SUBMIT_EVENT: "form-submit-event",
  FORM_REMOVAL_AFTER_FETCH: "form-removal-after-fetch",
  IFRAME_PAGEHIDE: "iframe-pagehide",
  PAGE_NAVIGATION: "page-navigation",
  PASSWORD_REMOVAL_AFTER_FETCH: "password-removal-after-fetch",
};

export class FormHandlerChild extends JSWindowActorChild {
  actorCreated() {
    // Whenever a FormHandlerChild is created it's because somebody has registered
    // their interest in form submissions. This step might create FormHandler actors
    // across multiple window contexts. Whenever a FormHandlerChild is created in a
    // process root, we want to make sure that it registers the progress listener
    // in order to listen for form submissions in that process.
    if (this.manager.isProcessRoot) {
      this.registerProgressListener();
    }
  }
  /**
   * Tracks whether an interest in form submissions was registered in this window
   */
  #hasRegisteredFormSubmissionInterest = false;

  /**
   * Tracks the actors that are interested in form or password field removals from DOM
   * If this set is empty, FormHandlerChild can unregister the form removal event listeners
   */
  #actorsListeningForFormRemoval = new Set();

  handleEvent(event) {
    if (!event.isTrusted) {
      return;
    }

    if (!this.#hasRegisteredFormSubmissionInterest) {
      return;
    }

    switch (event.type) {
      case "DOMDocFetchSuccess":
        this.processDOMDocFetchSuccessEvent();
        break;
      case "DOMFormBeforeSubmit":
        this.processDOMFormBeforeSubmitEvent(event);
        break;
      case "DOMFormRemoved":
        this.processDOMFormRemovedEvent(event);
        break;
      case "DOMInputPasswordRemoved": {
        this.processDOMInputPasswordRemovedEvent(event);
        break;
      }
      default:
        throw new Error("Unexpected event type");
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "FormHandler:FormSubmissionByNavigation": {
        this.processPageNavigation();
        break;
      }
      case "FormHandler:EnsureChildExists": {
        // This is just a dummy message to make sure that the
        // FormHandlerChild is created because then the actor
        // starts listening to page navigations
        break;
      }
    }
  }

  /**
   * Process the DOMFormBeforeSubmit event that is dispatched
   * after a form submit event.
   *
   * @param {Event} event DOMFormBeforeSubmit
   */
  processDOMFormBeforeSubmitEvent(event) {
    const form = event.target;
    const formSubmissionReason = FORM_SUBMISSION_REASON.FORM_SUBMIT_EVENT;

    this.#dispatchFormSubmissionEvent(form, formSubmissionReason);
  }

  /**
   * Process the DOMDocFetchSuccess event that is dispatched
   * after a successfull xhr/fetch request and start listening for
   * the events DOMFormRemoved and DOMInputPasswordRemoved
   */
  processDOMDocFetchSuccessEvent() {
    this.document.setNotifyFormOrPasswordRemoved(true);
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

    this.document.setNotifyFetchSuccess(false);
    this.docShell.chromeEventHandler.removeEventListener(
      "DOMDocFetchSuccess",
      this
    );

    this.#dispatchPrepareFormSubmissionEvent();
  }

  /**
   * Process the DOMFormRemoved event that is dispatched
   * after a form was removed from the DOM.
   *
   * @param {Event} event DOMFormRemoved
   */
  processDOMFormRemovedEvent(event) {
    const form = event.target;
    const formSubmissionReason =
      FORM_SUBMISSION_REASON.FORM_REMOVAL_AFTER_FETCH;

    this.#dispatchFormSubmissionEvent(form, formSubmissionReason);
  }

  /**
   * Process the DOMInputPasswordRemoved event that is dispatched
   * after a password input was removed from the DOM.
   *
   * @param {Event} event DOMInputPasswordRemoved
   */
  processDOMInputPasswordRemovedEvent(event) {
    const form = event.target;
    const formSubmissionReason =
      FORM_SUBMISSION_REASON.PASSWORD_REMOVAL_AFTER_FETCH;

    this.#dispatchFormSubmissionEvent(form, formSubmissionReason);
  }

  /**
   * This or the page of a parent browsing context was navigated,
   * so process the page navigation, only when somebody in the current has
   * registered interest for it
   */
  processPageNavigation() {
    if (!this.#hasRegisteredFormSubmissionInterest) {
      // Nobody is interested in the current window
      // so don't bother notifying anyone
      return;
    }
    const formSubmissionReason = FORM_SUBMISSION_REASON.PAGE_NAVIGATION;
    this.#dispatchFormSubmissionEvent(null, formSubmissionReason);
  }

  /**
   * Dispatch the CustomEvent form-submission-detected and transfer
   * the following information:
   *            detail.form   - the form that is being submitted
   *            detail.reason - the heuristic that detected the form submission
   *                            (see FORM_SUBMISSION_REASON)
   *
   * @param {HTMLFormElement} form
   * @param {string} reason
   */
  #dispatchFormSubmissionEvent(form, reason) {
    const formSubmissionEvent = new CustomEvent("form-submission-detected", {
      detail: { form, reason },
      bubbles: true,
    });
    this.document.dispatchEvent(formSubmissionEvent);
  }

  /**
   * Dispatch the before-form-submission event after receiving
   * a DOMDocFetchSuccess event. This gives the listening actors a chance to
   * save observed fields before they are removed from the DOM.
   */
  #dispatchPrepareFormSubmissionEvent() {
    const parepareFormSubmissionEvent = new CustomEvent(
      "before-form-submission",
      {
        bubbles: true,
      }
    );
    this.document.dispatchEvent(parepareFormSubmissionEvent);
  }

  /**
   * A page navigation was observed in this window or in the subtree.
   * If somebody in this window is interested in form submissions, process it here.
   * Additionally, inform the parent of the navigation so that all FormHandler
   * children in the subtree of the navigated browsing context are notified as well.
   *
   * @param {BrowsingContext} navigatedBrowingContext
   */
  onNavigationObserved(navigatedBrowingContext) {
    if (
      this.#hasRegisteredFormSubmissionInterest &&
      this.browsingContext == navigatedBrowingContext
    ) {
      // This is the most probable case, that an interest in form submissions was registered
      // in the navigated browing context, so we call processPageNavigation directly
      // instead of letting the parent notify this actor again to process it.
      this.processPageNavigation();
    }
    this.sendAsyncMessage(
      "FormHandler:NotifyNavigatedSubtree",
      navigatedBrowingContext
    );
  }

  /**
   * Set up needed listeners in order to detect form submissions after an actor indicated their interest
   *
   * 1. Register listeners relevant to form / password input removal heuristic
   *    - Set up 'DOMDocFetchSuccess' event listener (by calling setNotifyFetchSuccess)
   *
   * 2. Set up listeners relevant to page navigation heuristic
   *    - Create the corresponding parent of the current child, because the existence
   *      of the FormHandlerParent is the condition for being notified of a page navigation.
   *      If the current process is not the process root, we create the FormHandlerChild in
   *      the process root. The progress listener is registered after creating the child.
   *      If the current process is in a cross-origin frame, we notify the parent
   *      to register the progress listener also with the top level's process root.
   *
   * @param {JSWindowActorChild} interestedActor
   * @param {boolean} includesFormRemoval
   */
  registerFormSubmissionInterest(
    interestedActor,
    { includesFormRemoval = true, includesPageNavigation = true } = {}
  ) {
    if (includesFormRemoval) {
      if (!this.#actorsListeningForFormRemoval.size) {
        // The list of actors interest in form removals is empty when this is the
        // first time an actor registered to be notified of form removals or when all actors
        // processed their forms previously and unregistered their interest again. In both
        // cases we need to set up the listener for the event 'DOMDocFetchSuccess' here.
        this.document.setNotifyFetchSuccess(true);
        this.docShell.chromeEventHandler.addEventListener(
          "DOMDocFetchSuccess",
          this,
          true
        );
      }
      this.#actorsListeningForFormRemoval.add(interestedActor);
    }

    if (this.#hasRegisteredFormSubmissionInterest) {
      // If an actor in this window has already registered their interest
      // in form submissions, then the page navigation listeners are already set up
      return;
    }

    if (includesPageNavigation) {
      // We use the existence of the FormHandlerParent on the parent side
      // to determine whether to notify the corresponding FormHandleChild
      // when a page is navigated. So we explicitly create the parent actor
      // by sending a dummy message here
      this.sendAsyncMessage("FormHandler:EnsureParentExists");

      if (!this.manager.isProcessRoot) {
        // The progress listener is registered after the
        // FormHandlerChild is created in the process root
        this.document.ownerGlobal.windowRoot.ownerGlobal.windowGlobalChild.getActor(
          "FormHandler"
        );
      }

      if (!this.manager.sameOriginWithTop) {
        // If the top level is navigated, that also effects the current cross-origin frame.
        // So we notify the parent to set up the progress listeners at the top as well.
        this.sendAsyncMessage("FormHandler:RegisterProgressListenerAtTopLevel");
      }
      this.#hasRegisteredFormSubmissionInterest = true;
    }
  }

  /**
   * The actors that are interested in form submissions explicitly unregister their interest
   * in form removals here. This way we can keep track if there is any interested actor left
   * so that we don't remove the form removal event listeners too early, but we also don't
   * listen to the form removal events for too long unnecessarily.
   *
   * @param {JSWindowActorChild} interestedActor
   */
  unregisterFormRemovalInterest(interestedActor) {
    this.#actorsListeningForFormRemoval.delete(interestedActor);

    if (this.#actorsListeningForFormRemoval.size) {
      // Other actors are still interested in form removals
      return;
    }
    this.document.setNotifyFormOrPasswordRemoved(false);
    this.docShell.chromeEventHandler.removeEventListener(
      "DOMFormRemoved",
      this
    );
    this.docShell.chromeEventHandler.removeEventListener(
      "DOMInputPasswordRemoved",
      this
    );
  }

  /**
   * Set up a nsIWebProgressListener that notifies of certain request state
   * changes such as changes of the location and the history stack for this docShell
   * and for the children's same-orign docShells.
   *
   * Note: Registering the listener only in the process root (instead of for
   *       every window) is enough to receive notifications for the whole process,
   *       because the notifications bubble up
   */
  registerProgressListener() {
    const webProgress = this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);

    const flags =
      Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT |
      Ci.nsIWebProgress.NOTIFY_LOCATION;
    try {
      webProgress.addProgressListener(observer, flags);
    } catch (ex) {
      // Ignore NS_ERROR_FAILURE if the progress listener was already added
    }
  }
}

const observer = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),

  /**
   * Handle history stack changes (history.replaceState(), history.pushState())
   * on the same document as page navigation
   */
  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    if (
      !(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) ||
      !(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_PUSHSTATE)
    ) {
      return;
    }
    const navigatedWindow = aWebProgress.DOMWindow;

    this.notifyProcessRootOfNavigation(navigatedWindow);
  },

  /*
   * Handle certain state changes of requests as page navigation
   * such as location changes (location.assign(), location.replace())
   * See further comments for more details
   */
  onStateChange(aWebProgress, aRequest, aStateFlags, _aStatus) {
    if (
      aStateFlags & Ci.nsIWebProgressListener.STATE_RESTORING &&
      aStateFlags & Ci.nsIWebProgressListener.STATE_STOP
    ) {
      // a document is restored from bfcache
      return;
    }

    if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    // We only care about when a page triggered a load, not the user. For example:
    // clicking refresh/back/forward, typing a URL and hitting enter, and loading a bookmark aren't
    // likely to be when a user wants to save formautofill data.
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

    // We don't handle history navigation, reloads (e.g. history.go(-1), history.back(), location.reload())
    // Note: History state changes (e.g. history.replaceState(), history.pushState()) are handled in onLocationChange
    if (!(aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_NORMAL)) {
      return;
    }

    const navigatedWindow = aWebProgress.DOMWindow;
    this.notifyProcessRootOfNavigation(navigatedWindow);
  },

  /**
   * Notify the current process root parent of the page navigation
   * and pass on the navigated browsing context
   *
   * @param {Window} navigatedWindow
   */
  notifyProcessRootOfNavigation(navigatedWindow) {
    const processRootWindow = navigatedWindow.windowRoot.ownerGlobal;
    const formHandlerChild =
      processRootWindow.windowGlobalChild.getExistingActor("FormHandler");
    const navigatedBrowsingContext = navigatedWindow.browsingContext;

    formHandlerChild?.onNavigationObserved(navigatedBrowsingContext);
  },
};
