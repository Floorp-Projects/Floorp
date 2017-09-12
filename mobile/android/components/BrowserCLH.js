/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");

var Strings = {};

XPCOMUtils.defineLazyGetter(Strings, "brand", _ =>
        Services.strings.createBundle("chrome://branding/locale/brand.properties"));
XPCOMUtils.defineLazyGetter(Strings, "browser", _ =>
        Services.strings.createBundle("chrome://browser/locale/browser.properties"));
XPCOMUtils.defineLazyGetter(Strings, "reader", _ =>
        Services.strings.createBundle("chrome://global/locale/aboutReader.properties"));

function BrowserCLH() {}

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

  addObserverScripts: function(aScripts) {
    aScripts.forEach(item => {
      let {name, topics, script} = item;
      XPCOMUtils.defineLazyGetter(this, name, _ => {
        let sandbox = {};
        if (script.endsWith(".jsm")) {
          Cu.import(script, sandbox);
        } else {
          Services.scriptloader.loadSubScript(script, sandbox);
        }
        return sandbox[name];
      });
      let observer = (subject, topic, data) => {
        Services.obs.removeObserver(observer, topic);
        if (!item.once) {
          Services.obs.addObserver(this[name], topic);
        }
        this[name].observe(subject, topic, data); // Explicitly notify new observer
      };
      topics.forEach(topic => {
        Services.obs.addObserver(observer, topic);
      });
    });
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "app-startup":
        this.setResourceSubstitutions();

        let observerScripts = [{
          name: "DownloadNotifications",
          script: "resource://gre/modules/DownloadNotifications.jsm",
          topics: ["chrome-document-interactive"],
          once: true,
        }];
        if (AppConstants.MOZ_WEBRTC) {
          observerScripts.push({
            name: "WebrtcUI",
            script: "chrome://browser/content/WebrtcUI.js",
            topics: [
              "getUserMedia:ask-device-permission",
              "getUserMedia:request",
              "PeerConnection:request",
              "recording-device-events",
              "VideoCapture:Paused",
              "VideoCapture:Resumed",
            ],
          });
        }
        this.addObserverScripts(observerScripts);
        break;
    }
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  // XPCOMUtils factory
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}")
};

var components = [ BrowserCLH ];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
