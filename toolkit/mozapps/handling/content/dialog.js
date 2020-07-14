/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This dialog builds its content based on arguments passed into it.
 * window.arguments[0]:
 *   The title of the dialog.
 * window.arguments[1]:
 *   The url of the image that appears to the left of the description text
 * window.arguments[2]:
 *   The text of the description that will appear above the choices the user
 *   can choose from.
 * window.arguments[3]:
 *   The text of the label directly above the choices the user can choose from.
 * window.arguments[4]:
 *   This is the text to be placed in the label for the checkbox.  If no text is
 *   passed (ie, it's an empty string), the checkbox will be hidden.
 * window.arguments[5]:
 *   The accesskey for the checkbox
 * window.arguments[6]:
 *   This is the text that is displayed below the checkbox when it is checked.
 * window.arguments[7]:
 *   This is the nsIHandlerInfo that gives us all our precious information.
 * window.arguments[8]:
 *   This is the nsIURI that we are being brought up for in the first place.
 * window.arguments[9]:
 *   This is the nsIPrincipal that has triggered the dialog; may be null.
 * window.arguments[10]:
 *   The browsingContext from which the request originates; may be null.
 */

const { EnableDelayHelper } = ChromeUtils.import(
  "resource://gre/modules/SharedPromptUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
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

var dialog = {
  // Member Variables

  _handlerInfo: null,
  _URI: null,
  _itemChoose: null,
  _okButton: null,
  _browsingContext: null,
  _buttonDisabled: true,

  // Methods

  /**
   * This function initializes the content of the dialog.
   */
  initialize: function initialize() {
    this._handlerInfo = window.arguments[7].QueryInterface(Ci.nsIHandlerInfo);
    this._URI = window.arguments[8].QueryInterface(Ci.nsIURI);
    let principal = window.arguments[9]?.QueryInterface(Ci.nsIPrincipal);
    this._browsingContext = window.arguments[10];
    let usePrivateBrowsing = false;
    if (this._browsingContext) {
      usePrivateBrowsing = this._browsingContext.usePrivateBrowsing;
    }

    this.isPrivate =
      usePrivateBrowsing ||
      (window.opener && PrivateBrowsingUtils.isWindowPrivate(window.opener));

    this._itemChoose = document.getElementById("item-choose");
    this._okButton = document.getElementById("handling").getButton("extra1");

    var description = {
      image: document.getElementById("description-image"),
      text: document.getElementById("description-text"),
    };
    var options = document.getElementById("item-action-text");
    var checkbox = {
      desc: document.getElementById("remember"),
      text: document.getElementById("remember-text"),
    };

    // Setting values
    document.title = window.arguments[0];
    description.image.src = window.arguments[1];
    description.text.textContent = window.arguments[2];
    options.value = window.arguments[3];
    checkbox.desc.label = window.arguments[4];
    checkbox.desc.accessKey = window.arguments[5];
    checkbox.text.textContent = window.arguments[6];

    if (principal && principal.isContentPrincipal) {
      let hostContainer = document.getElementById("originating-host");
      document.l10n.pauseObserving();
      document.l10n.setAttributes(hostContainer, "handler-dialog-host", {
        host: principal.exposablePrePath,
        scheme: this._URI.scheme,
      });
      document.l10n
        .translateElements([hostContainer])
        .then(() => window.sizeToContent());
      document.l10n.resumeObserving();
      hostContainer.parentNode.removeAttribute("hidden");
    }

    // Hide stuff that needs to be hidden
    if (!checkbox.desc.label) {
      checkbox.desc.hidden = true;
    }

    // UI is ready, lets populate our list
    this.populateList();
    // Explicitly not an 'accept' button to avoid having `enter` accept the dialog.
    document.addEventListener("dialogextra1", () => {
      this.onOK();
    });
    document.addEventListener("dialogaccept", e => {
      e.preventDefault();
    });

    this._delayHelper = new EnableDelayHelper({
      disableDialog: () => {
        this._buttonDisabled = true;
        this.updateOKButton();
      },
      enableDialog: () => {
        this._buttonDisabled = false;
        this.updateOKButton();
      },
      focusTarget: window,
    });
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

      if (app instanceof Ci.nsILocalHandlerApp) {
        // See if we have an nsILocalHandlerApp and set the icon
        let uri = Services.io.newFileURI(app.executable);
        elm.setAttribute("image", "moz-icon://" + uri.spec + "?size=32");
      } else if (app instanceof Ci.nsIWebHandlerApp) {
        let uri = Services.io.newURI(app.uriTemplate);
        if (/^https?$/.test(uri.scheme)) {
          // Unfortunately we can't use the favicon service to get the favicon,
          // because the service looks for a record with the exact URL we give
          // it, and users won't have such records for URLs they don't visit,
          // and users won't visit the handler's URL template, they'll only
          // visit URLs derived from that template (i.e. with %s in the template
          // replaced by the URL of the content being handled).
          elm.setAttribute("image", uri.prePath + "/favicon.ico");
        }
        elm.setAttribute("description", uri.prePath);

        // Check for extensions needing private browsing access before
        // creating UI elements.
        if (this.isPrivate) {
          let policy = WebExtensionPolicy.getByURI(uri);
          if (policy && !policy.privateBrowsingAllowed) {
            var bundle = document.getElementById("base-strings");
            var disabledLabel = bundle.getString(
              "privatebrowsing.disabled.label"
            );
            elm.setAttribute("disabled", true);
            elm.setAttribute("description", disabledLabel);
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
      var gioApps = gIOSvc.getAppsForURIScheme(this._URI.scheme);
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
  chooseApplication: function chooseApplication() {
    var bundle = document.getElementById("base-strings");
    var title = bundle.getString("choose.application.title");

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
  onOK: function onOK() {
    if (this._buttonDisabled) {
      return;
    }
    var checkbox = document.getElementById("remember");
    if (!checkbox.hidden) {
      // We need to make sure that the default is properly set now
      if (this.selectedItem.obj) {
        // default OS handler doesn't have this property
        this._handlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
        this._handlerInfo.preferredApplicationHandler = this.selectedItem.obj;
      } else {
        this._handlerInfo.preferredAction = Ci.nsIHandlerInfo.useSystemDefault;
      }
    }
    this._handlerInfo.alwaysAskBeforeHandling = !checkbox.checked;

    var hs = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
      Ci.nsIHandlerService
    );
    hs.store(this._handlerInfo);

    this._handlerInfo.launchWithURI(this._URI, this._browsingContext);
    window.close();
  },

  /**
   * Determines if the OK button should be disabled or not
   */
  updateOKButton: function updateOKButton() {
    this._okButton.disabled = this._itemChoose.selected || this._buttonDisabled;
  },

  /**
   * Updates the UI based on the checkbox being checked or not.
   */
  onCheck: function onCheck() {
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
      this.onOK();
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
    return (document.getElementById("items").selectedItem = aItem);
  },
};
