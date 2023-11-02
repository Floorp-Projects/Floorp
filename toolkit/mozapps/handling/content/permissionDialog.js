/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { EnableDelayHelper } = ChromeUtils.importESModule(
  "resource://gre/modules/PromptUtils.sys.mjs"
);

let dialog = {
  /**
   * This function initializes the content of the dialog.
   */
  initialize() {
    let args = window.arguments[0].wrappedJSObject || window.arguments[0];
    let {
      handler,
      principal,
      outArgs,
      canPersistPermission,
      preferredHandlerName,
      browsingContext,
    } = args;

    this._handlerInfo = handler.QueryInterface(Ci.nsIHandlerInfo);
    this._principal = principal?.QueryInterface(Ci.nsIPrincipal);
    this._addonPolicy =
      this._principal?.addonPolicy ?? this._principal?.contentScriptAddonPolicy;
    this._browsingContext = browsingContext;
    this._outArgs = outArgs.QueryInterface(Ci.nsIWritablePropertyBag);
    this._preferredHandlerName = preferredHandlerName;

    this._dialog = document.querySelector("dialog");
    this._checkRemember = document.getElementById("remember");
    this._checkRememberContainer = document.getElementById("rememberContainer");

    if (!canPersistPermission) {
      this._checkRememberContainer.hidden = true;
    }

    let changeAppLink = document.getElementById("change-app");

    // allow the user to choose another application if they wish,
    // but don't offer this if the protocol was opened via
    // system principal (URLbar) and there's a preferred handler
    if (this._preferredHandlerName && !this._principal?.isSystemPrincipal) {
      changeAppLink.hidden = false;

      changeAppLink.addEventListener("click", () => this.onChangeApp());
    }
    document.addEventListener("dialogaccept", () => this.onAccept());
    this.initL10n();

    this._delayHelper = new EnableDelayHelper({
      disableDialog: () => {
        this._dialog.setAttribute("buttondisabledaccept", true);
      },
      enableDialog: () => {
        this._dialog.setAttribute("buttondisabledaccept", false);
      },
      focusTarget: window,
    });
  },

  /**
   * Checks whether the principal that triggered this dialog is top level
   * (not embedded in a frame).
   * @returns {boolean} - true if principal is top level, false otherwise.
   * If the triggering principal is null this method always returns false.
   */
  triggeringPrincipalIsTop() {
    if (!this._principal) {
      return false;
    }

    let topContentPrincipal =
      this._browsingContext?.top.embedderElement?.contentPrincipal;
    if (!topContentPrincipal) {
      return false;
    }
    return this._principal.equals(topContentPrincipal);
  },

  /**
   * Determines the l10n ID to use for the dialog description, depending on
   * the triggering principal and the preferred application handler.
   */
  get l10nDescriptionId() {
    if (this._addonPolicy) {
      if (this._preferredHandlerName) {
        return "permission-dialog-description-extension-app";
      }
      return "permission-dialog-description-extension";
    }

    if (this._principal?.schemeIs("file")) {
      if (this._preferredHandlerName) {
        return "permission-dialog-description-file-app";
      }
      return "permission-dialog-description-file";
    }

    if (this._principal?.isSystemPrincipal && this._preferredHandlerName) {
      return "permission-dialog-description-system-app";
    }

    if (this._principal?.isSystemPrincipal && !this._preferredHandlerName) {
      return "permission-dialog-description-system-noapp";
    }

    // We only show the website address if the request didn't come from the top
    // level frame. If we can't get a host to display, fall back to the copy
    // without host.
    if (!this.triggeringPrincipalIsTop() && this.displayPrePath) {
      if (this._preferredHandlerName) {
        return "permission-dialog-description-host-app";
      }
      return "permission-dialog-description-host";
    }

    if (this._preferredHandlerName) {
      return "permission-dialog-description-app";
    }

    return "permission-dialog-description";
  },

  /**
   * Determines the l10n ID to use for the "remember permission" checkbox,
   * depending on the triggering principal and the preferred application
   * handler.
   */
  get l10nCheckboxId() {
    if (!this._principal) {
      return null;
    }

    if (this._addonPolicy) {
      return "permission-dialog-remember-extension";
    }
    if (this._principal.schemeIs("file")) {
      return "permission-dialog-remember-file";
    }
    return "permission-dialog-remember";
  },

  /**
   * Computes the prePath to show in the prompt. It's the prePath of the site
   * that wants to navigate to the external protocol.
   * @returns {string|null} - prePath to show, or null if we can't derive an
   * exposable prePath from the triggering principal.
   */
  get displayPrePath() {
    if (!this._principal) {
      return null;
    }

    // NullPrincipals don't expose a meaningful prePath. Instead use the
    // precursorPrincipal, which the NullPrincipal was derived from.
    if (this._principal.isNullPrincipal) {
      return this._principal.precursorPrincipal?.exposablePrePath;
    }

    return this._principal?.exposablePrePath;
  },

  initL10n() {
    // The UI labels depend on whether we will show the application chooser next
    // or directly open the assigned protocol handler.

    // Fluent id for dialog accept button
    let idAcceptButton;
    let acceptButton = this._dialog.getButton("accept");

    if (this._preferredHandlerName) {
      idAcceptButton = "permission-dialog-btn-open-link";
    } else {
      idAcceptButton = "permission-dialog-btn-choose-app";

      let descriptionExtra = document.getElementById("description-extra");
      descriptionExtra.hidden = false;
      acceptButton.addEventListener("click", () => this.onChangeApp());
    }
    document.l10n.setAttributes(acceptButton, idAcceptButton);

    let description = document.getElementById("description");

    let host = this.displayPrePath;
    let scheme = this._handlerInfo.type;

    document.l10n.setAttributes(description, this.l10nDescriptionId, {
      host,
      scheme,
      extension: this._addonPolicy?.name,
      appName: this._preferredHandlerName,
    });

    if (!this._checkRememberContainer.hidden) {
      let checkboxLabel = document.getElementById("remember-label");
      document.l10n.setAttributes(checkboxLabel, this.l10nCheckboxId, {
        host,
        scheme,
      });
    }
  },

  onAccept() {
    this._outArgs.setProperty("remember", this._checkRemember.checked);
    this._outArgs.setProperty("granted", true);
  },

  onChangeApp() {
    this._outArgs.setProperty("resetHandlerChoice", true);

    // We can't call the dialogs accept handler here. If the accept button is
    // still disabled, it will prevent closing.
    this.onAccept();
    window.close();
  },
};

window.addEventListener("DOMContentLoaded", () => dialog.initialize(), {
  once: true,
});
