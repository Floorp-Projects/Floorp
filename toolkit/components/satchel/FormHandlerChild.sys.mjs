/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The FormHandlerChild is the place to implement logic that is shared
 * by child actors like FormAutofillChild, LoginManagerChild and FormHistoryChild
 * or in general components that deal with form data.
 */

export const FORM_SUBMISSION_REASON = {
  FORM_SUBMIT_EVENT: "form-submit-event",
  FORM_REMOVAL_AFTER_FETCH: "form-removal-after-fetch",
  IFRAME_PAGEHIDE: "iframe-pagehide",
  PAGE_NAVIGATION: "page-navigation",
};

export class FormHandlerChild extends JSWindowActorChild {
  handleEvent(event) {
    if (!event.isTrusted) {
      return;
    }
    switch (event.type) {
      case "DOMFormBeforeSubmit":
        this.processDOMFormBeforeSubmitEvent(event);
        break;
      default:
        throw new Error("Unexpected event type");
    }
  }

  /**
   * Process the DOMFormBeforeSubmitEvent that is dispatched
   * after a form submit event. Extract event data
   * that is relevant to the form submission listeners
   *
   * @param {Event} event DOMFormBeforeSubmit
   */
  processDOMFormBeforeSubmitEvent(event) {
    const form = event.target;
    const formSubmissionReason = FORM_SUBMISSION_REASON.FORM_SUBMIT_EVENT;

    this.#dispatchFormSubmissionEvent(form, formSubmissionReason);
  }

  // handle form-removal-after-fetch
  processFormRemovalAfterFetch(params) {}

  // handle iframe-pagehide
  processIframePagehide(params) {}

  // handle page-navigation
  processPageNavigation(params) {}

  /**
   * Dispatch the CustomEvent form-submission-detected also transfer
   * the information:
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
}
