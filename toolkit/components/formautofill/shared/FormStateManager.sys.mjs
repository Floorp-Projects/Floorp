/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.sys.mjs",
  FormAutofillHandler:
    "resource://gre/modules/shared/FormAutofillHandler.sys.mjs",
});

export class FormStateManager {
  /**
   * @type {WeakMap} mapping FormLike root HTML elements to FormAutofillHandler objects.
   */
  #formHandlerByRoot = new WeakMap();

  constructor(onAutofillCallback) {
    /**
     * @type {object} The object where to store the active items, e.g. element,
     * handler, section, and field detail.
     */
    this._activeItems = {};

    this.onAutofillCallback = onAutofillCallback;
  }

  /**
   * Get the active input's information from cache which is created after page
   * identified.
   *
   * @returns {object | null}
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
        let detailElement = detail.element;
        if (detailElement && this.activeInput == detailElement) {
          this._activeItems.fieldDetail = detail;
          break;
        }
      }
    }
    return this._activeItems.fieldDetail;
  }

  /**
   * Get the active form's information from cache which is created after page
   * identified.
   *
   * @returns {Array<object> | null}
   *          Return target form's information from content cache
   *          (or return null if the information is not found in the cache).
   *
   */
  get activeFormDetails() {
    let formHandler = this.activeHandler;
    return formHandler ? formHandler.fieldDetails : null;
  }

  get activeInput() {
    return this._activeItems.elementWeakRef?.deref();
  }

  get activeHandler() {
    const activeInput = this.activeInput;
    if (!activeInput) {
      return null;
    }

    // XXX: We are recomputing the activeHandler every time to avoid keeping a
    // reference on the active element. This might be called quite frequently
    // so if _getFormHandler/findRootForField become more costly, we should
    // look into caching this result (eg by adding a weakmap).
    let handler = this.getFormHandler(activeInput);
    if (handler) {
      handler.focusedInput = activeInput;
    }
    return handler;
  }

  get activeSection() {
    let formHandler = this.activeHandler;
    return formHandler ? formHandler.activeSection : null;
  }

  /**
   * Get the form's handler from cache which is created after page identified.
   *
   * @param {HTMLInputElement} element Focused input which triggered profile searching
   * @returns {Array<object> | null}
   *          Return target form's handler from content cache
   *          (or return null if the information is not found in the cache).
   *
   */
  getFormHandler(element) {
    if (!element) {
      return null;
    }
    const rootElement = lazy.FormLikeFactory.findRootForField(element);
    return this.#formHandlerByRoot.get(rootElement);
  }

  identifyAutofillFields(element) {
    let formHandler = this.getFormHandler(element);
    if (!formHandler) {
      formHandler = new lazy.FormAutofillHandler(
        lazy.FormLikeFactory.createFromField(element),
        this.onAutofillCallback
      );
      this.#formHandlerByRoot.set(formHandler.form.rootElement, formHandler);
    } else if (!formHandler.updateFormIfNeeded(element)) {
      return formHandler.fieldDetails;
    }

    return formHandler.collectFormFields();
  }

  updateActiveInput(element) {
    if (!element) {
      this._activeItems = {};
      return;
    }
    this._activeItems = {
      elementWeakRef: new WeakRef(element),
      fieldDetail: null,
    };
  }

  getRecords(formElement) {
    const handler = this.#formHandlerByRoot.get(formElement);
    if (!handler) {
      return null;
    }

    const records = handler.createRecords();
    if (!records || !Object.values(records).some(record => record.length)) {
      return null;
    }
    return records;
  }

  didDestroy() {
    this._activeItems = {};
  }
}

export default FormStateManager;
