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
   * We only show the website address if the origin is not user-readable
   * in the address bar.
   * @returns {boolean} - true if principal is top level and user-readable,
   *                      false otherwise.
   * If the triggering principal is null this method always returns false.
   */
  shouldShowPrincipal() {
    if (!this._principal) {
      return false;
    }

    let topContentPrincipal =
      this._browsingContext?.top.embedderElement?.contentPrincipal;

    let shownScheme = this._browsingContext.currentURI.scheme;
    return (
      !topContentPrincipal ||
      !topContentPrincipal.equals(this._principal) ||
      !["http", "https", "file"].includes(shownScheme)
    );
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

    if (this._principal?.schemeIs("file") && !this.shouldShowPrincipal()) {
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

    if (this.shouldShowPrincipal() && this.userReadablePrincipal) {
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
   * Computes text to show in the prompt that is a user-understandable
   * version of what is asking to open the external protocol.
   * It's usually the prePath of the site that wants to navigate to
   * the external protocol, though we use the OS syntax for paths if
   * the request comes from `file://`.
   * @returns {string|null} - text to show, or null if we can't derive an
   * readable string from the triggering principal.
   */
  get userReadablePrincipal() {
    if (!this._principal) {
      return null;
    }

    // NullPrincipals don't expose a meaningful prePath. Instead use the
    // precursorPrincipal, which the NullPrincipal was derived from.
    let p = this._principal.isNullPrincipal
      ? this._principal.precursorPrincipal
      : this._principal;
    // Files have `file://` and nothing else as the prepath. Showing the
    // full path seems more useful for users:
    if (p?.schemeIs("file")) {
      try {
        let { file } = p.URI.QueryInterface(Ci.nsIFileURL);
        return file.path;
      } catch (_ex) {
        return p.spec;
      }
    }
    return p?.exposablePrePath;
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

    let host = this.userReadablePrincipal;
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
