/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

function GeckoViewStartup() {
}

GeckoViewStartup.prototype = {
  classID: Components.ID("{8e993c34-fdd6-432c-967e-f995d888777f}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  addLazyGetter: function(aOptions) {
    let {name, script, service, module, observers, ppmm, mm} = aOptions;
    if (script) {
      XPCOMUtils.defineLazyScriptGetter(this, name, script);
    } else if (module) {
      XPCOMUtils.defineLazyGetter(this, name, _ => {
        let sandbox = {};
        Cu.import(module, sandbox);
        if (aOptions.init) {
          aOptions.init.call(this, sandbox[name]);
        }
        return sandbox[name];
      });
    } else if (service) {
      XPCOMUtils.defineLazyGetter(this, name, _ =>
        Cc[service].getService(Ci.nsISupports).wrappedJSObject);
    }

    if (observers) {
      let observer = (subject, topic, data) => {
        Services.obs.removeObserver(observer, topic);
        if (!aOptions.once) {
          Services.obs.addObserver(this[name], topic);
        }
        this[name].observe(subject, topic, data); // Explicitly notify new observer
      };
      observers.forEach(topic => Services.obs.addObserver(observer, topic));
    }

    if (ppmm || mm) {
      let target = ppmm ? Services.ppmm : Services.mm;
      let listener = msg => {
        target.removeMessageListener(msg.name, listener);
        if (!aOptions.once) {
          target.addMessageListener(msg.name, this[name]);
        }
        this[name].receiveMessage(msg);
      };
      (ppmm || mm).forEach(msg => target.addMessageListener(msg, listener));
    }
  },

  addLazyEventListener: function(aOptions) {
    let {name, target, events, options} = aOptions;
    let listener = event => {
      if (!options || !options.once) {
        target.removeEventListener(event.type, listener, options);
        target.addEventListener(event.type, this[name], options);
      }
      this[name].handleEvent(event);
    };
    events.forEach(event => target.addEventListener(event, listener, options));
  },

  /* ----------  nsIObserver  ---------- */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "app-startup": {
        // Parent and content process.
        Services.obs.addObserver(this, "chrome-document-global-created");
        Services.obs.addObserver(this, "content-document-global-created");

        this.addLazyGetter({
          name: "GeckoViewPermission",
          service: "@mozilla.org/content-permission/prompt;1",
          observers: [
            "getUserMedia:ask-device-permission",
            "getUserMedia:request",
            "PeerConnection:request",
          ],
        });

        if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
          // Content process only.
          this.addLazyGetter({
            name: "GeckoViewPrompt",
            service: "@mozilla.org/prompter;1",
          });
        }
        break;
      }

      case "profile-after-change": {
        // Parent process only.
        // ContentPrefServiceParent is needed for e10s file picker.
        this.addLazyGetter({
          name: "ContentPrefServiceParent",
          module: "resource://gre/modules/ContentPrefServiceParent.jsm",
          init: cpsp => cpsp.alwaysInit(),
          ppmm: [
            "ContentPrefs:FunctionCall",
            "ContentPrefs:AddObserverForName",
            "ContentPrefs:RemoveObserverForName",
          ],
        });

        this.addLazyGetter({
          name: "GeckoViewPrompt",
          service: "@mozilla.org/prompter;1",
          mm: [
            "GeckoView:Prompt",
          ],
        });
        break;
      }

      case "chrome-document-global-created":
      case "content-document-global-created": {
        let win = aSubject.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDocShell).QueryInterface(Ci.nsIDocShellTreeItem)
                          .rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindow);
        if (win !== aSubject) {
          // Only attach to top-level windows.
          return;
        }

        this.addLazyEventListener({
          name: "GeckoViewPrompt",
          target: win,
          events: [
            "click",
            "contextmenu",
          ],
          options: {
            capture: false,
            mozSystemGroup: true,
          },
        });
        break;
      }
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([GeckoViewStartup]);
