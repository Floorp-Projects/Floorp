/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { EnableDelayHelper } = ChromeUtils.import(
  "resource://gre/modules/SharedPromptUtils.jsm"
);

class MozHandler extends window.MozElements.MozRichlistitem {
  static get markup() {
    return `
    <vbox pack="center">
      <image height="32" width="32"/>
    </vbox>
    <vbox flex="1">
      <label class="name"/>
      <label class="description"/>
    </vbox>
    `;
  }

  connectedCallback() {
    this.textContent = "";
    this.appendChild(this.constructor.fragment);
    this.initializeAttributeInheritance();
  }

  static get inheritedAttributes() {
    return {
      image: "src=image,disabled",
      ".name": "value=name,disabled",
      ".description": "value=description,disabled",
    };
  }

  get label() {
    return `${this.getAttribute("name")} ${this.getAttribute("description")}`;
  }
}

customElements.define("mozapps-handler", MozHandler, {
  extends: "richlistitem",
});

window.addEventListener("DOMContentLoaded", () => dialog.initialize(), {
  once: true,
});

let loadPromise = new Promise(resolve => {
  window.addEventListener("load", resolve, { once: true });
});

let dialog = {
  /**
   * This function initializes the content of the dialog.
   */
  initialize() {
    let args = window.arguments[0].wrappedJSObject || window.arguments[0];
    let { handler, outArgs, usePrivateBrowsing, enableButtonDelay } = args;

    this._handlerInfo = handler.QueryInterface(Ci.nsIHandlerInfo);
    this._outArgs = outArgs;

    this.isPrivate =
      usePrivateBrowsing ||
      (window.opener && PrivateBrowsingUtils.isWindowPrivate(window.opener));

    this._dialog = document.querySelector("dialog");
    this._itemChoose = document.getElementById("item-choose");
    this._rememberCheck = document.getElementById("remember");

    // Register event listener for the checkbox hint.
    this._rememberCheck.addEventListener("change", () => this.onCheck());

    document.addEventListener("dialogaccept", () => {
      this.onAccept();
    });

    // UI is ready, lets populate our list
    this.populateList();

    document.mozSubdialogReady = this.initL10n().then(() => {
      window.sizeToContent();
    });

    if (enableButtonDelay) {
      this._delayHelper = new EnableDelayHelper({
        disableDialog: () => {
          this._acceptBtnDisabled = true;
          this.updateAcceptButton();
        },
        enableDialog: () => {
          this._acceptBtnDisabled = false;
          this.updateAcceptButton();
        },
        focusTarget: window,
      });
    }
  },

  async initL10n() {
    document.l10n.pauseObserving();

    let rememberLabel = document.getElementById("remember-label");
    document.l10n.setAttributes(rememberLabel, "chooser-dialog-remember", {
      scheme: this._handlerInfo.type,
    });

    let description = document.getElementById("description");
    document.l10n.setAttributes(description, "chooser-dialog-description", {
      scheme: this._handlerInfo.type,
    });

    document.l10n.resumeObserving();

    await document.l10n.translateElements([rememberLabel, description]);
    return document.l10n.ready;
  },

  /**
   * Populates the list that a user can choose from.
   */
  populateList: function populateList() {
    var items = document.getElementById("items");
    var possibleHandlers = this._handlerInfo.possibleApplicationHandlers;
    var preferredHandler = this._handlerInfo.preferredApplicationHandler;
    for (let i = possibleHandlers.length - 1; i >= 0; --i) {
      let app = possibleHandlers.queryElementAt(i, Ci.nsIHandlerApp);
      let elm = document.createXULElement("richlistitem", {
        is: "mozapps-handler",
      });
      elm.setAttribute("name", app.name);
      elm.obj = app;

      // We defer loading the favicon so it doesn't delay load. The dialog is
      // opened in a SubDialog which will only show on window load.
      if (app instanceof Ci.nsILocalHandlerApp) {
        // See if we have an nsILocalHandlerApp and set the icon
        let uri = Services.io.newFileURI(app.executable);
        loadPromise.then(() => {
          elm.setAttribute("image", "moz-icon://" + uri.spec + "?size=32");
        });
      } else if (app instanceof Ci.nsIWebHandlerApp) {
        let uri = Services.io.newURI(app.uriTemplate);
        if (/^https?$/.test(uri.scheme)) {
          // Unfortunately we can't use the favicon service to get the favicon,
          // because the service looks for a record with the exact URL we give
          // it, and users won't have such records for URLs they don't visit,
          // and users won't visit the handler's URL template, they'll only
          // visit URLs derived from that template (i.e. with %s in the template
          // replaced by the URL of the content being handled).
          loadPromise.then(() => {
            elm.setAttribute("image", uri.prePath + "/favicon.ico");
          });
        }
        elm.setAttribute("description", uri.prePath);

        // Check for extensions needing private browsing access before
        // creating UI elements.
        if (this.isPrivate) {
          let policy = WebExtensionPolicy.getByURI(uri);
          if (policy && !policy.privateBrowsingAllowed) {
            elm.setAttribute("disabled", true);
            this.getPrivateBrowsingDisabledLabel().then(label => {
              elm.setAttribute("description", label);
            });
            if (app == preferredHandler) {
              preferredHandler = null;
            }
          }
        }
      } else if (app instanceof Ci.nsIDBusHandlerApp) {
        elm.setAttribute("description", app.method);
      } else if (!(app instanceof Ci.nsIGIOMimeApp)) {
        // We support GIO application handler, but no action required there
        throw new Error("unknown handler type");
      }

      items.insertBefore(elm, this._itemChoose);
      if (preferredHandler && app == preferredHandler) {
        this.selectedItem = elm;
      }
    }

    if (this._handlerInfo.hasDefaultHandler) {
      let elm = document.createXULElement("richlistitem", {
        is: "mozapps-handler",
      });
      elm.id = "os-default-handler";
      elm.setAttribute("name", this._handlerInfo.defaultDescription);

      items.insertBefore(elm, items.firstChild);
      if (
        this._handlerInfo.preferredAction == Ci.nsIHandlerInfo.useSystemDefault
      ) {
        this.selectedItem = elm;
      }
    }

    // Add gio handlers
    if (Cc["@mozilla.org/gio-service;1"]) {
      let gIOSvc = Cc["@mozilla.org/gio-service;1"].getService(
        Ci.nsIGIOService
      );
      var gioApps = gIOSvc.getAppsForURIScheme(this._handlerInfo.type);
      for (let handler of gioApps.enumerate(Ci.nsIHandlerApp)) {
        // OS handler share the same name, it's most likely the same app, skipping...
        if (handler.name == this._handlerInfo.defaultDescription) {
          continue;
        }
        // Check if the handler is already in possibleHandlers
        let appAlreadyInHandlers = false;
        for (let i = possibleHandlers.length - 1; i >= 0; --i) {
          let app = possibleHandlers.queryElementAt(i, Ci.nsIHandlerApp);
          // nsGIOMimeApp::Equals is able to compare with nsILocalHandlerApp
          if (handler.equals(app)) {
            appAlreadyInHandlers = true;
            break;
          }
        }
        if (!appAlreadyInHandlers) {
          let elm = document.createXULElement("richlistitem", {
            is: "mozapps-handler",
          });
          elm.setAttribute("name", handler.name);
          elm.obj = handler;
          items.insertBefore(elm, this._itemChoose);
        }
      }
    }

    items.ensureSelectedElementIsVisible();
  },

  /**
   * Brings up a filepicker and allows a user to choose an application.
   */
  async chooseApplication() {
    let title = await this.getChooseAppWindowTitle();

    var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, title, Ci.nsIFilePicker.modeOpen);
    fp.appendFilters(Ci.nsIFilePicker.filterApps);

    fp.open(rv => {
      if (rv == Ci.nsIFilePicker.returnOK && fp.file) {
        let uri = Services.io.newFileURI(fp.file);

        let handlerApp = Cc[
          "@mozilla.org/uriloader/local-handler-app;1"
        ].createInstance(Ci.nsILocalHandlerApp);
        handlerApp.executable = fp.file;

        // if this application is already in the list, select it and don't add it again
        let parent = document.getElementById("items");
        for (let i = 0; i < parent.childNodes.length; ++i) {
          let elm = parent.childNodes[i];
          if (
            elm.obj instanceof Ci.nsILocalHandlerApp &&
            elm.obj.equals(handlerApp)
          ) {
            parent.selectedItem = elm;
            parent.ensureSelectedElementIsVisible();
            return;
          }
        }

        let elm = document.createXULElement("richlistitem", {
          is: "mozapps-handler",
        });
        elm.setAttribute("name", fp.file.leafName);
        elm.setAttribute("image", "moz-icon://" + uri.spec + "?size=32");
        elm.obj = handlerApp;

        parent.selectedItem = parent.insertBefore(elm, parent.firstChild);
        parent.ensureSelectedElementIsVisible();
      }
    });
  },

  /**
   * Function called when the OK button is pressed.
   */
  onAccept() {
    this.updateHandlerData(this._rememberCheck.checked);
    this._outArgs.setProperty("openHandler", true);
  },

  /**
   * Determines if the accept button should be disabled or not
   */
  updateAcceptButton() {
    this._dialog.setAttribute(
      "buttondisabledaccept",
      this._acceptBtnDisabled || this._itemChoose.selected
    );
  },

  /**
   * Update the handler info to reflect the user choice.
   * @param {boolean} skipAsk - Whether we should persist the application
   * choice and skip asking next time.
   */
  updateHandlerData(skipAsk) {
    // We need to make sure that the default is properly set now
    if (this.selectedItem.obj) {
      // default OS handler doesn't have this property
      this._outArgs.setProperty(
        "preferredAction",
        Ci.nsIHandlerInfo.useHelperApp
      );
      this._outArgs.setProperty(
        "preferredApplicationHandler",
        this.selectedItem.obj
      );
    } else {
      this._outArgs.setProperty(
        "preferredAction",
        Ci.nsIHandlerInfo.useSystemDefault
      );
    }
    this._outArgs.setProperty("alwaysAskBeforeHandling", !skipAsk);
  },

  /**
   * Updates the UI based on the checkbox being checked or not.
   */
  onCheck() {
    if (document.getElementById("remember").checked) {
      document.getElementById("remember-text").setAttribute("visible", "true");
    } else {
      document.getElementById("remember-text").removeAttribute("visible");
    }
  },

  /**
   * Function called when the user double clicks on an item of the list
   */
  onDblClick: function onDblClick() {
    if (this.selectedItem == this._itemChoose) {
      this.chooseApplication();
    } else {
      this._dialog.acceptDialog();
    }
  },

  // Getters / Setters

  /**
   * Returns/sets the selected element in the richlistbox
   */
  get selectedItem() {
    return document.getElementById("items").selectedItem;
  },
  set selectedItem(aItem) {
    document.getElementById("items").selectedItem = aItem;
  },

  /**
   *  Lazy l10n getter for the title of the app chooser window
   */
  async getChooseAppWindowTitle() {
    if (!this._chooseAppWindowTitle) {
      this._chooseAppWindowTitle = await document.l10n.formatValues([
        "choose-other-app-window-title",
      ]);
    }
    return this._chooseAppWindowTitle;
  },

  /**
   * Lazy l10n getter for handler menu items which are disabled due to private
   * browsing.
   */
  async getPrivateBrowsingDisabledLabel() {
    if (!this._privateBrowsingDisabledLabel) {
      this._privateBrowsingDisabledLabel = await document.l10n.formatValues([
        "choose-dialog-privatebrowsing-disabled",
      ]);
    }
    return this._privateBrowsingDisabledLabel;
  },
};
