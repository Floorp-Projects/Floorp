/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  DelayedInit: "resource://gre/modules/DelayedInit.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

function BrowserCLH() {
  this.wrappedJSObject = this;
}

BrowserCLH.prototype = {
  observe: function(subject, topic, data) {
    switch (topic) {
      case "app-startup": {
        Services.obs.addObserver(this, "chrome-document-interactive");
        Services.obs.addObserver(this, "content-document-interactive");

        ActorManagerParent.flush();

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
        Services.obs.addObserver(
          {
            QueryInterface: ChromeUtils.generateQI([
              Ci.nsIObserver,
              Ci.nsIFormSubmitObserver,
            ]),
            notifyInvalidSubmit: (form, element) => {
              this.FormAssistant.notifyInvalidSubmit(form, element);
            },
          },
          "invalidformsubmit"
        );

        GeckoViewUtils.addLazyGetter(this, "LoginManagerParent", {
          module: "resource://gre/modules/LoginManagerParent.jsm",
          mm: [
            // PLEASE KEEP THIS LIST IN SYNC WITH THE DESKTOP LIST IN
            // BrowserGlue.jsm
            "PasswordManager:findLogins",
            "PasswordManager:findRecipes",
            "PasswordManager:onFormSubmit",
            "PasswordManager:autoCompleteLogins",
            "PasswordManager:removeLogin",
            "PasswordManager:insecureLoginFormPresent",
            // PLEASE KEEP THIS LIST IN SYNC WITH THE DESKTOP LIST IN
            // BrowserGlue.jsm
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
          handler: _ =>
            DelayedInit.scheduleList(
              [_ => Services.search.init(), _ => Services.logins],
              10000 /* 10 seconds maximum wait. */
            ),
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

        GeckoViewUtils.addLazyEventListener(
          win,
          ["focus", "blur", "click", "input"],
          {
            handler: event => {
              if (
                ChromeUtils.getClassName(event.target) === "HTMLInputElement" ||
                ChromeUtils.getClassName(event.target) ===
                  "HTMLTextAreaElement" ||
                ChromeUtils.getClassName(event.target) ===
                  "HTMLSelectElement" ||
                ChromeUtils.getClassName(event.target) === "HTMLButtonElement"
              ) {
                // Only load FormAssistant when the event target is what we care about.
                return this.FormAssistant;
              }
              return null;
            },
            options: {
              capture: true,
              mozSystemGroup: true,
            },
          }
        );

        this._initLoginManagerEvents(win);

        GeckoViewUtils.registerLazyWindowEventListener(
          win,
          ["TextSelection:Get", "TextSelection:Action", "TextSelection:End"],
          {
            scope: this,
            name: "ActionBarHandler",
          }
        );
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

      case "profile-after-change": {
        EventDispatcher.instance.registerListener(
          this,
          "GeckoView:SetDefaultPrefs"
        );
        break;
      }
    }
  },

  onEvent(aEvent, aData, aCallback) {
    switch (aEvent) {
      case "GeckoView:SetDefaultPrefs": {
        // While we want to allow setting certain preferences via GeckoView, we
        // don't want to let it take over completely the management of those
        // preferences. Therefore we don't handle the "ResetUserPrefs" message,
        // and consequently we also apply any pref changes directly, i.e. *not*
        // on the default branch.
        const prefs = new Preferences();
        for (const name of Object.keys(aData)) {
          try {
            prefs.set(name, aData[name]);
          } catch (e) {
            Cu.reportError(`Failed to set preference ${name}: ${e}`);
          }
        }
        break;
      }
    }
  },

  _initLoginManagerEvents: function(aWindow) {
    if (Services.prefs.getBoolPref("reftest.remote", false)) {
      // XXX known incompatibility between reftest harness and form-fill.
      return;
    }

    function shouldIgnoreLoginManagerEvent(event) {
      // If we have a null principal then prevent any more password manager code from running and
      // incorrectly using the document `location`.
      return event.target.nodePrincipal.isNullPrincipal;
    }

    let options = {
      capture: true,
      mozSystemGroup: true,
    };

    // NOTE: Much of this logic is duplicated in browser/base/content/content.js
    // for desktop.
    aWindow.addEventListener("DOMFormBeforeSubmit", event => {
      if (shouldIgnoreLoginManagerEvent(event)) {
        return;
      }
      this.LoginManagerContent.onDOMFormBeforeSubmit(event);
    });
    aWindow.addEventListener(
      "DOMFormHasPassword",
      event => {
        if (shouldIgnoreLoginManagerEvent(event)) {
          return;
        }
        this.LoginManagerContent.onDOMFormHasPassword(event);
      },
      options
    );

    aWindow.addEventListener(
      "DOMInputPasswordAdded",
      event => {
        if (shouldIgnoreLoginManagerEvent(event)) {
          return;
        }
        this.LoginManagerContent.onDOMInputPasswordAdded(
          event,
          event.target.ownerGlobal.top
        );
      },
      options
    );

    aWindow.addEventListener(
      "pageshow",
      event => {
        // XXXbz what about non-HTML documents??
        if (ChromeUtils.getClassName(event.target) == "HTMLDocument") {
          this.LoginManagerContent.onPageShow(event);
        }
      },
      options
    );
  },

  // QI
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  // XPCOMUtils factory
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}"),
};

var components = [BrowserCLH];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
