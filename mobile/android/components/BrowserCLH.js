/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  DelayedInit: "resource://gre/modules/DelayedInit.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

function BrowserCLH() {
  this.wrappedJSObject = this;
}

BrowserCLH.prototype = {
  /**
   * Register resource://android as the APK root.
   *
   * Consumers can access Android assets using resource://android/assets/FILENAME.
   */
  setResourceSubstitutions: function() {
    let registry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);
    // Like jar:jar:file:///data/app/org.mozilla.fennec-2.apk!/assets/omni.ja!/chrome/chrome/content/aboutHome.xhtml
    let url = registry.convertChromeURL(Services.io.newURI("chrome://browser/content/aboutHome.xhtml")).spec;
    // Like jar:file:///data/app/org.mozilla.fennec-2.apk!/
    url = url.substring(4, url.indexOf("!/") + 2);

    let protocolHandler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    protocolHandler.setSubstitution("android", Services.io.newURI(url));
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "app-startup": {
        this.setResourceSubstitutions();

        Services.obs.addObserver(this, "chrome-document-interactive");
        Services.obs.addObserver(this, "content-document-interactive");

        GeckoViewUtils.addLazyGetter(this, "DownloadNotifications", {
          module: "resource://gre/modules/DownloadNotifications.jsm",
          observers: ["chrome-document-loaded"],
          once: true,
        });

        if (AppConstants.MOZ_WEBRTC) {
          GeckoViewUtils.addLazyGetter(this, "WebrtcUI", {
            module: "resource://gre/modules/WebrtcUI.jsm",
            observers: [
              "getUserMedia:ask-device-permission",
              "getUserMedia:request",
              "PeerConnection:request",
              "recording-device-events",
              "VideoCapture:Paused",
              "VideoCapture:Resumed",
            ],
          });
        }

        GeckoViewUtils.addLazyGetter(this, "SelectHelper", {
          module: "resource://gre/modules/SelectHelper.jsm",
        });
        GeckoViewUtils.addLazyGetter(this, "InputWidgetHelper", {
          module: "resource://gre/modules/InputWidgetHelper.jsm",
        });

        GeckoViewUtils.addLazyGetter(this, "FormAssistant", {
          module: "resource://gre/modules/FormAssistant.jsm",
        });
        Services.obs.addObserver({
          QueryInterface: ChromeUtils.generateQI([
            Ci.nsIObserver, Ci.nsIFormSubmitObserver,
          ]),
          notifyInvalidSubmit: (form, element) => {
            this.FormAssistant.notifyInvalidSubmit(form, element);
          },
        }, "invalidformsubmit");

        GeckoViewUtils.addLazyGetter(this, "LoginManagerParent", {
          module: "resource://gre/modules/LoginManagerParent.jsm",
          mm: [
            // PLEASE KEEP THIS LIST IN SYNC WITH THE DESKTOP LIST IN nsBrowserGlue.js
            "RemoteLogins:findLogins",
            "RemoteLogins:findRecipes",
            "RemoteLogins:onFormSubmit",
            "RemoteLogins:autoCompleteLogins",
            "RemoteLogins:removeLogin",
            "RemoteLogins:insecureLoginFormPresent",
            // PLEASE KEEP THIS LIST IN SYNC WITH THE DESKTOP LIST IN nsBrowserGlue.js
          ],
        });
        GeckoViewUtils.addLazyGetter(this, "LoginManagerContent", {
          module: "resource://gre/modules/LoginManagerContent.jsm",
        });

        GeckoViewUtils.addLazyGetter(this, "ActionBarHandler", {
          module: "resource://gre/modules/ActionBarHandler.jsm",
        });

        // Once the first chrome window is loaded, schedule a list of startup
        // tasks to be performed on idle.
        GeckoViewUtils.addLazyGetter(this, "DelayedStartup", {
          observers: ["chrome-document-loaded"],
          once: true,
          handler: _ => DelayedInit.scheduleList([
            _ => Services.search.init(),
            _ => Services.logins,
          ], 10000 /* 10 seconds maximum wait. */),
        });
        break;
      }

      case "chrome-document-interactive":
      case "content-document-interactive": {
        let contentWin = subject.defaultView;
        let win = GeckoViewUtils.getChromeWindow(contentWin);
        let dispatcher = GeckoViewUtils.getDispatcherForWindow(win);
        if (!win || !dispatcher || win !== contentWin) {
          // Only attach to top-level windows.
          return;
        }

        GeckoViewUtils.addLazyEventListener(win, "click", {
          handler: _ => [this.SelectHelper, this.InputWidgetHelper],
          options: {
            capture: true,
            mozSystemGroup: true,
          },
        });

        GeckoViewUtils.addLazyEventListener(win, [
          "focus", "blur", "click", "input",
        ], {
          handler: event => {
            if (ChromeUtils.getClassName(event.target) === "HTMLInputElement" ||
                ChromeUtils.getClassName(event.target) === "HTMLTextAreaElement" ||
                ChromeUtils.getClassName(event.target) === "HTMLSelectElement" ||
                ChromeUtils.getClassName(event.target) === "HTMLButtonElement") {
              // Only load FormAssistant when the event target is what we care about.
              return this.FormAssistant;
            }
            return null;
          },
          options: {
            capture: true,
            mozSystemGroup: true,
          },
        });

        this._initLoginManagerEvents(win);

        GeckoViewUtils.registerLazyWindowEventListener(win, [
          "TextSelection:Get",
          "TextSelection:Action",
          "TextSelection:End",
        ], {
          scope: this,
          name: "ActionBarHandler",
        });
        GeckoViewUtils.addLazyEventListener(win, ["mozcaretstatechanged"], {
          scope: this,
          name: "ActionBarHandler",
          options: {
            capture: true,
            mozSystemGroup: true,
          },
        });
        break;
      }
    }
  },

  _initLoginManagerEvents: function(aWindow) {
    if (Services.prefs.getBoolPref("reftest.remote", false)) {
      // XXX known incompatibility between reftest harness and form-fill.
      return;
    }

    let options = {
      capture: true,
      mozSystemGroup: true,
    };

    // NOTE: Much of this logic is duplicated in browser/base/content/content.js
    // for desktop.
    aWindow.addEventListener("DOMFormHasPassword", event => {
      this.LoginManagerContent.onDOMFormHasPassword(event, event.target.ownerGlobal.top);
    }, options);

    aWindow.addEventListener("DOMInputPasswordAdded", event => {
      this.LoginManagerContent.onDOMInputPasswordAdded(event, event.target.ownerGlobal.top);
    }, options);

    aWindow.addEventListener("DOMAutoComplete", event => {
      this.LoginManagerContent.onUsernameInput(event);
    }, options);

    aWindow.addEventListener("blur", event => {
      if (ChromeUtils.getClassName(event.target) === "HTMLInputElement") {
        this.LoginManagerContent.onUsernameInput(event);
      }
    }, options);

    aWindow.addEventListener("pageshow", event => {
      // XXXbz what about non-HTML documents??
      if (ChromeUtils.getClassName(event.target) == "HTMLDocument") {
        this.LoginManagerContent.onPageShow(event, event.target.defaultView.top);
      }
    }, options);
  },

  // QI
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  // XPCOMUtils factory
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}")
};

var components = [ BrowserCLH ];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
