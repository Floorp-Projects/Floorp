/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewStartup"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const { debug, warn } = GeckoViewUtils.initLogging("Startup");

const JSWINDOWACTORS = {
  LoadURIDelegate: {
    child: {
      moduleURI: "resource:///actors/LoadURIDelegateChild.jsm",
    },
  },
  GeckoViewPrompt: {
    child: {
      moduleURI: "resource:///actors/GeckoViewPromptChild.jsm",
      events: {
        click: { capture: false, mozSystemGroup: true },
        contextmenu: { capture: false, mozSystemGroup: true },
        DOMPopupBlocked: { capture: false, mozSystemGroup: true },
      },
    },
    allFrames: true,
  },
};

class GeckoViewStartup {
  /* ----------  nsIObserver  ---------- */
  observe(aSubject, aTopic, aData) {
    debug`observe: ${aTopic}`;
    switch (aTopic) {
      case "app-startup": {
        // Parent and content process.
        GeckoViewUtils.addLazyGetter(this, "GeckoViewPermission", {
          service: "@mozilla.org/content-permission/prompt;1",
          observers: [
            "getUserMedia:ask-device-permission",
            "getUserMedia:request",
            "PeerConnection:request",
          ],
          ppmm: ["GeckoView:AddCameraPermission"],
        });

        GeckoViewUtils.addLazyGetter(this, "GeckoViewRecordingMedia", {
          module: "resource://gre/modules/GeckoViewMedia.jsm",
          observers: ["recording-device-events"],
        });

        GeckoViewUtils.addLazyGetter(this, "GeckoViewConsole", {
          module: "resource://gre/modules/GeckoViewConsole.jsm",
        });

        GeckoViewUtils.addLazyGetter(this, "GeckoViewWebExtension", {
          module: "resource://gre/modules/GeckoViewWebExtension.jsm",
          ged: [
            "GeckoView:ActionDelegate:Attached",
            "GeckoView:BrowserAction:Click",
            "GeckoView:PageAction:Click",
            "GeckoView:RegisterWebExtension",
            "GeckoView:UnregisterWebExtension",
            "GeckoView:WebExtension:CancelInstall",
            "GeckoView:WebExtension:Disable",
            "GeckoView:WebExtension:Enable",
            "GeckoView:WebExtension:EnsureBuiltIn",
            "GeckoView:WebExtension:Get",
            "GeckoView:WebExtension:Install",
            "GeckoView:WebExtension:InstallBuiltIn",
            "GeckoView:WebExtension:List",
            "GeckoView:WebExtension:PortDisconnect",
            "GeckoView:WebExtension:PortMessageFromApp",
            "GeckoView:WebExtension:SetPBAllowed",
            "GeckoView:WebExtension:Uninstall",
            "GeckoView:WebExtension:Update",
          ],
          observers: [
            "devtools-installed-addon",
            "testing-installed-addon",
            "testing-uninstalled-addon",
          ],
        });

        GeckoViewUtils.addLazyGetter(this, "GeckoViewStorageController", {
          module: "resource://gre/modules/GeckoViewStorageController.jsm",
          ged: [
            "GeckoView:ClearData",
            "GeckoView:ClearSessionContextData",
            "GeckoView:ClearHostData",
            "GeckoView:GetAllPermissions",
            "GeckoView:GetPermissionsByURI",
          ],
        });

        GeckoViewUtils.addLazyGetter(this, "GeckoViewPushController", {
          module: "resource://gre/modules/GeckoViewPushController.jsm",
          ged: ["GeckoView:PushEvent", "GeckoView:PushSubscriptionChanged"],
        });

        GeckoViewUtils.addLazyGetter(
          this,
          "GeckoViewContentBlockingController",
          {
            module:
              "resource://gre/modules/GeckoViewContentBlockingController.jsm",
            ged: [
              "ContentBlocking:AddException",
              "ContentBlocking:RemoveException",
              "ContentBlocking:RemoveExceptionByPrincipal",
              "ContentBlocking:CheckException",
              "ContentBlocking:SaveList",
              "ContentBlocking:RestoreList",
              "ContentBlocking:ClearList",
            ],
          }
        );

        GeckoViewUtils.addLazyPrefObserver(
          {
            name: "geckoview.console.enabled",
            default: false,
          },
          {
            handler: _ => this.GeckoViewConsole,
          }
        );

        // Handle invalid form submission. If we don't hook up to this,
        // invalid forms are allowed to be submitted!
        Services.obs.addObserver(
          {
            QueryInterface: ChromeUtils.generateQI([
              "nsIObserver",
              "nsIFormSubmitObserver",
            ]),
            notifyInvalidSubmit: (form, element) => {
              // We should show the validation message here, bug 1510450.
            },
          },
          "invalidformsubmit"
        );

        if (
          Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT
        ) {
          ActorManagerParent.addJSWindowActors(JSWINDOWACTORS);

          Services.mm.loadFrameScript(
            "chrome://geckoview/content/GeckoViewPromptChild.js",
            true
          );

          GeckoViewUtils.addLazyGetter(this, "ContentCrashHandler", {
            module: "resource://gre/modules/ContentCrashHandler.jsm",
            observers: ["ipc:content-shutdown"],
          });
        }
        break;
      }

      case "profile-after-change": {
        // Parent process only.
        // ContentPrefServiceParent is needed for e10s file picker.
        GeckoViewUtils.addLazyGetter(this, "ContentPrefServiceParent", {
          module: "resource://gre/modules/ContentPrefServiceParent.jsm",
          init: cpsp => cpsp.alwaysInit(),
          ppmm: [
            "ContentPrefs:FunctionCall",
            "ContentPrefs:AddObserverForName",
            "ContentPrefs:RemoveObserverForName",
          ],
        });

        GeckoViewUtils.addLazyGetter(this, "GeckoViewRemoteDebugger", {
          module: "resource://gre/modules/GeckoViewRemoteDebugger.jsm",
          init: gvrd => gvrd.onInit(),
        });

        GeckoViewUtils.addLazyPrefObserver(
          {
            name: "devtools.debugger.remote-enabled",
            default: false,
          },
          {
            handler: _ => this.GeckoViewRemoteDebugger,
          }
        );

        GeckoViewUtils.addLazyGetter(this, "DownloadTracker", {
          module: "resource://gre/modules/GeckoViewWebExtension.jsm",
          ged: ["GeckoView:WebExtension:DownloadChanged"],
        });

        ChromeUtils.import("resource://gre/modules/NotificationDB.jsm");

        // Listen for global EventDispatcher messages
        EventDispatcher.instance.registerListener(this, [
          "GeckoView:ResetUserPrefs",
          "GeckoView:SetDefaultPrefs",
          "GeckoView:SetLocale",
        ]);

        Services.obs.notifyObservers(null, "geckoview-startup-complete");
        break;
      }
      case "browser-idle-startup-tasks-finished": {
        // This only needs to happen once during startup.
        Services.obs.removeObserver(this, aTopic);
        // Notify the start up crash tracker that the browser has successfully
        // started up so the startup cache isn't rebuilt on next startup.
        Services.startup.trackStartupCrashEnd();
        break;
      }
    }
  }

  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent}`;

    switch (aEvent) {
      case "GeckoView:ResetUserPrefs": {
        const prefs = new Preferences();
        prefs.reset(aData.names);
        break;
      }
      case "GeckoView:SetDefaultPrefs": {
        const prefs = new Preferences({ defaultBranch: true });
        for (const name of Object.keys(aData)) {
          try {
            prefs.set(name, aData[name]);
          } catch (e) {
            warn`Failed to set preference ${name}: ${e}`;
          }
        }
        break;
      }
      case "GeckoView:SetLocale":
        if (aData.requestedLocales) {
          Services.locale.requestedLocales = aData.requestedLocales;
        }
        const pls = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(
          Ci.nsIPrefLocalizedString
        );
        pls.data = aData.acceptLanguages;
        Services.prefs.setComplexValue(
          "intl.accept_languages",
          Ci.nsIPrefLocalizedString,
          pls
        );
        break;
    }
  }
}

GeckoViewStartup.prototype.classID = Components.ID(
  "{8e993c34-fdd6-432c-967e-f995d888777f}"
);
GeckoViewStartup.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIObserver",
]);
