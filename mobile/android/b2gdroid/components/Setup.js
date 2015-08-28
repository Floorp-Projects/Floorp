/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppsUtils",
                                  "resource://gre/modules/AppsUtils.jsm");

function debug() {
  dump("-*- B2GDroidSetup " + Array.slice(arguments) + "\n");
}

function B2GDroidSetup() { }

B2GDroidSetup.prototype = {
  classID:         Components.ID('{8bc88ef2-3aab-4e94-a40c-e2c80added2c}'),
  QueryInterface:  XPCOMUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),

  getApk: function() {
    let registry = Cc["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Ci.nsIChromeRegistry);
    let url = registry.convertChromeURL(
      Services.io.newURI("chrome://b2g/content/shell.html", null, null)).spec;
    // url is something like jar:jar:file:///data/app/org.mozilla.fennec_fabrice-1.apk!/assets/omni.ja!/chrome/chrome/content/shell.html
    // and we just need the apk file path.
    let path = url.substring(url.indexOf("///") + 2, url.indexOf(".apk") + 4);
    debug("apk path: " + path);
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(path);
    return file;
  },

  installWebapps: function(aDir) {
    debug("Extracting webapps");
    let apk = this.getApk();
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                      .createInstance(Ci.nsIZipReader);
    zipReader.open(apk);

    // Get the target file for a zip entry, normalizing it to remove the
    // assets/gaia part.
    function getTargetFile(aDir, entry) {
      let target = aDir.clone();
      let part = 0;
      entry.split("/").forEach(aPart => {
        if (part > 1) {
          target.append(aPart);
        }
        part++;
      });
      return target;
    }

    try {
      // create directories first
      let entries = zipReader.findEntries("assets/gaia/webapps/*");
      while (entries.hasMore()) {
        let entryName = entries.getNext();
        let entry = zipReader.getEntry(entryName);
        let target = getTargetFile(aDir, entryName);
        if (!target.exists() && entry.isDirectory) {
          try {
            debug("Creating " + entryName);
            target.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
          }
          catch (e) {
            debug("extractFiles: failed to create directory " + target.path + " : " + e);
          }
        }
      }

      entries = zipReader.findEntries("assets/gaia/webapps/*");
      while (entries.hasMore()) {
        let entryName = entries.getNext();
        let target = getTargetFile(aDir, entryName);
        if (target.exists())
          continue;

        debug("Extracting " + entryName + " to " + target.path);
        zipReader.extract(entryName, target);
        try {
          target.permissions |= FileUtils.PERMS_FILE;
        }
        catch (e) {
          debug("Failed to set permissions " + aPermissions.toString(8) + " on " + target.path);
        }
      }
    }
    finally {
      zipReader.close();
    }

    debug("Webapps extracted");
  },

  installSettings: function(aDir) {
    debug("Installing default settings");
    let apk = this.getApk();
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                      .createInstance(Ci.nsIZipReader);
    zipReader.open(apk);

    try {
      let dest = aDir.clone();
      dest.append("settings.json")
      zipReader.extract("assets/gaia/settings.json", dest);
      debug("Default settings installed to " + dest.path);
    }
    catch(e) {
      dump("Error extracting settings.json : " + e);
    }
    finally {
      zipReader.close();
    }
  },

  shellStartup: function(aWindow) {
    Services.androidBridge.browserApp = {
      selectedTab: function() {
        debug("browserApp::selectedTab");
        return null;
      },

      getBrowserTab: function(aTabId) {
        debug("browserApp::getBrowserTab " + aTabId);
        return null;
      },

      getPreferences: function(aRequestId, aPrefNames, aCount) {
        debug("browserApp::getPreferences " + uneval(aPrefNames));
        let prefs = [];

        for (let prefName of aPrefNames) {
          let pref = {
            name: prefName,
            type: "",
            value: null
          };

          try {
            switch (Services.prefs.getPrefType(prefName)) {
              case Ci.nsIPrefBranch.PREF_BOOL:
                pref.type = "bool";
                pref.value = Services.prefs.getBoolPref(prefName);
                break;
              case Ci.nsIPrefBranch.PREF_INT:
                pref.type = "int";
                pref.value = Services.prefs.getIntPref(prefName);
                break;
              case Ci.nsIPrefBranch.PREF_STRING:
              default:
                pref.type = "string";
                try {
                  // Try in case it's a localized string (will throw an exception if not)
                  pref.value = Services.prefs.getComplexValue(prefName, Ci.nsIPrefLocalizedString).data;
                } catch (e) {
                  pref.value = Services.prefs.getCharPref(prefName);
                }
                break;
            }
          } catch (e) {
            debug("Error reading pref [" + prefName + "]: " + e);
            // preference does not exist; do not send it
            continue;
          }

          prefs.push(pref);
        }

        Messaging.sendRequest({
          type: "Preferences:Data",
          requestId: aRequestId, // opaque request identifier, can be any string/int/whatever
          preferences: prefs
        });
      },

      observePreferences: function(requestId, prefNames, count) {
        debug("browserApp::observePreferences " + prefNames);
      },

      removePreferenceObservers: function(aRequestId) {
        debug("browserApp::removePreferenceObservers");
      },

      getUITelemetryObserver: function() {
        debug("browserApp:getUITelemetryObserver");
        return null;
      }
    };

    Cu.import("resource://gre/modules/Messaging.jsm");
    Messaging.sendRequest({ type: "Launcher:Ready" });
    Messaging.sendRequest({ type: "Gecko:Ready" });
    debug("Sent Gecko:Ready");
    aWindow.top.QueryInterface(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIDOMWindowUtils).isFirstPaint = true;
    Services.androidBridge.contentDocumentChanged();
  },

  observe: function (aSubject, aTopic, aData) {
    debug("observer notification: " + aTopic);
    if (aTopic === "shell-startup") {
      this.shellStartup(aSubject);
      return;
    }

    if (aTopic !== "profile-after-change") {
      return;
    }

    // At first run of after updates, unpack gaia.
    let branch = Services.prefs.getBranch("b2gdroid");
    if (!AppsUtils.isFirstRun(branch)) {
      debug("No need to unpack gaia again.");
      return;
    }

    let profile = Services.dirsvc.get("DefRt", Ci.nsIFile);
    debug("profile directory is " + profile.path);

    let webapps = profile.clone();
    webapps.append("webapps");
    webapps.append("webapps.json");
    this.installWebapps(profile);

    let settings = profile.clone();
    settings.append("settings.json");
    this.installSettings(profile);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([B2GDroidSetup]);
