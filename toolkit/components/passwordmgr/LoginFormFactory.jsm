/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A factory to generate LoginForm objects that represent a set of login fields
 * which aren't necessarily marked up with a <form> element.
 */

"use strict";

const EXPORTED_SYMBOLS = ["LoginFormFactory"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "FormLikeFactory",
  "resource://gre/modules/FormLikeFactory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return LoginHelper.createLogger("LoginFormFactory");
});

this.LoginFormFactory = {
  /**
   * WeakMap of the root element of a LoginForm to the LoginForm representing its fields.
   *
   * This is used to be able to lookup an existing LoginForm for a given root element since multiple
   * calls to LoginFormFactory.createFrom* won't give the exact same object. When batching fills we don't always
   * want to use the most recent list of elements for a LoginForm since we may end up doing multiple
   * fills for the same set of elements when a field gets added between arming and running the
   * DeferredTask.
   *
   * @type {WeakMap}
   */
  _loginFormsByRootElement: new WeakMap(),

  /**
   * Maps all DOM content documents in this content process, including those in
   * frames, to a WeakSet of LoginForm for the document.
   */
  _loginFormRootElementsByDocument: new WeakMap(),

  /**
   * Create a LoginForm object from a <form>.
   *
   * @param {HTMLFormElement} aForm
   * @return {LoginForm}
   * @throws Error if aForm isn't an HTMLFormElement
   */
  createFromForm(aForm) {
    let formLike = FormLikeFactory.createFromForm(aForm);
    formLike.action = LoginHelper.getFormActionOrigin(aForm);

    let rootElementsSet = this.getRootElementsWeakSetForDocument(
      formLike.ownerDocument
    );
    rootElementsSet.add(formLike.rootElement);
    log.debug(
      "adding",
      formLike.rootElement,
      "to root elements for",
      formLike.ownerDocument
    );

    this._loginFormsByRootElement.set(formLike.rootElement, formLike);
    return formLike;
  },

  /**
   * Create a LoginForm object from a password or username field.
   *
   * If the field is in a <form>, construct the LoginForm from the form.
   * Otherwise, create a LoginForm with a rootElement (wrapper) according to
   * heuristics. Currently all <input> not in a <form> are one LoginForm but this
   * shouldn't be relied upon as the heuristics may change to detect multiple
   * "forms" (e.g. registration and login) on one page with a <form>.
   *
   * Note that two LoginForms created from the same field won't return the same LoginForm object.
   * Use the `rootElement` property on the LoginForm as a key instead.
   *
   * @param {HTMLInputElement} aField - a password or username field in a document
   * @return {LoginForm}
   * @throws Error if aField isn't a password or username field in a document
   */
  createFromField(aField) {
    if (
      ChromeUtils.getClassName(aField) !== "HTMLInputElement" ||
      (!aField.hasBeenTypePassword &&
        !LoginHelper.isUsernameFieldType(aField)) ||
      !aField.ownerDocument
    ) {
      throw new Error(
        "createFromField requires a password or username field in a document"
      );
    }

    if (aField.form) {
      return this.createFromForm(aField.form);
    } else if (aField.hasAttribute("form")) {
      log.debug(
        "createFromField: field has form attribute but no form: ",
        aField.getAttribute("form")
      );
    }

    let formLike = FormLikeFactory.createFromField(aField);
    formLike.action = LoginHelper.getLoginOrigin(aField.ownerDocument.baseURI);
    log.debug(
      "Created non-form LoginForm for rootElement:",
      aField.ownerDocument.documentElement
    );

    let rootElementsSet = this.getRootElementsWeakSetForDocument(
      formLike.ownerDocument
    );
    rootElementsSet.add(formLike.rootElement);
    log.debug(
      "adding",
      formLike.rootElement,
      "to root elements for",
      formLike.ownerDocument
    );

    this._loginFormsByRootElement.set(formLike.rootElement, formLike);

    return formLike;
  },

  getRootElementsWeakSetForDocument(aDocument) {
    let rootElementsSet = this._loginFormRootElementsByDocument.get(aDocument);
    if (!rootElementsSet) {
      rootElementsSet = new WeakSet();
      this._loginFormRootElementsByDocument.set(aDocument, rootElementsSet);
    }
    return rootElementsSet;
  },

  getForRootElement(aRootElement) {
    return this._loginFormsByRootElement.get(aRootElement);
  },

  setForRootElement(aRootElement, aLoginForm) {
    return this._loginFormsByRootElement.set(aRootElement, aLoginForm);
  },
};
