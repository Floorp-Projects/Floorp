/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

if (typeof(Ci) == 'undefined') {
  var Ci = Components.interfaces;
}

if (typeof(Cc) == 'undefined') {
  var Cc = Components.classes;
}

this.SpecialPowersError = function(aMsg) {
  Error.call(this);
  let {stack} = new Error();
  this.message = aMsg;
  this.name = "SpecialPowersError";
}
SpecialPowersError.prototype = Object.create(Error.prototype);

SpecialPowersError.prototype.toString = function() {
  return `${this.name}: ${this.message}`;
};

this.SpecialPowersObserverAPI = function SpecialPowersObserverAPI() {
  this._crashDumpDir = null;
  this._processCrashObserversRegistered = false;
  this._chromeScriptListeners = [];
  this._extensions = new Map();
}

function parseKeyValuePairs(text) {
  var lines = text.split('\n');
  var data = {};
  for (let i = 0; i < lines.length; i++) {
    if (lines[i] == '')
      continue;

    // can't just .split() because the value might contain = characters
    let eq = lines[i].indexOf('=');
    if (eq != -1) {
      let [key, value] = [lines[i].substring(0, eq),
                          lines[i].substring(eq + 1)];
      if (key && value)
        data[key] = value.replace(/\\n/g, "\n").replace(/\\\\/g, "\\");
    }
  }
  return data;
}

function parseKeyValuePairsFromFile(file) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  var is = Cc["@mozilla.org/intl/converter-input-stream;1"].
           createInstance(Ci.nsIConverterInputStream);
  is.init(fstream, "UTF-8", 1024, Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
  var str = {};
  var contents = '';
  while (is.readString(4096, str) != 0) {
    contents += str.value;
  }
  is.close();
  fstream.close();
  return parseKeyValuePairs(contents);
}

function getTestPlugin(pluginName) {
  var ph = Cc["@mozilla.org/plugin/host;1"]
    .getService(Ci.nsIPluginHost);
  var tags = ph.getPluginTags();
  var name = pluginName || "Test Plug-in";
  for (var tag of tags) {
    if (tag.name == name) {
      return tag;
    }
  }

  return null;
}

SpecialPowersObserverAPI.prototype = {

  _observe: function(aSubject, aTopic, aData) {
    function addDumpIDToMessage(propertyName) {
      try {
        var id = aSubject.getPropertyAsAString(propertyName);
      } catch(ex) {
        var id = null;
      }
      if (id) {
        message.dumpIDs.push({id: id, extension: "dmp"});
        message.dumpIDs.push({id: id, extension: "extra"});
      }
    }

    switch(aTopic) {
      case "plugin-crashed":
      case "ipc:content-shutdown":
        var message = { type: "crash-observed", dumpIDs: [] };
        aSubject = aSubject.QueryInterface(Ci.nsIPropertyBag2);
        if (aTopic == "plugin-crashed") {
          addDumpIDToMessage("pluginDumpID");
          addDumpIDToMessage("browserDumpID");

          let pluginID = aSubject.getPropertyAsAString("pluginDumpID");
          let extra = this._getExtraData(pluginID);
          if (extra && ("additional_minidumps" in extra)) {
            let dumpNames = extra.additional_minidumps.split(',');
            for (let name of dumpNames) {
              message.dumpIDs.push({id: pluginID + "-" + name, extension: "dmp"});
            }
          }
        } else { // ipc:content-shutdown
          addDumpIDToMessage("dumpID");
        }
        this._sendAsyncMessage("SPProcessCrashService", message);
        break;
    }
  },

  _getCrashDumpDir: function() {
    if (!this._crashDumpDir) {
      this._crashDumpDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
      this._crashDumpDir.append("minidumps");
    }
    return this._crashDumpDir;
  },

  _getExtraData: function(dumpId) {
    let extraFile = this._getCrashDumpDir().clone();
    extraFile.append(dumpId + ".extra");
    if (!extraFile.exists()) {
      return null;
    }
    return parseKeyValuePairsFromFile(extraFile);
  },

  _deleteCrashDumpFiles: function(aFilenames) {
    var crashDumpDir = this._getCrashDumpDir();
    if (!crashDumpDir.exists()) {
      return false;
    }

    var success = aFilenames.length != 0;
    aFilenames.forEach(function(crashFilename) {
      var file = crashDumpDir.clone();
      file.append(crashFilename);
      if (file.exists()) {
        file.remove(false);
      } else {
        success = false;
      }
    });
    return success;
  },

  _findCrashDumpFiles: function(aToIgnore) {
    var crashDumpDir = this._getCrashDumpDir();
    var entries = crashDumpDir.exists() && crashDumpDir.directoryEntries;
    if (!entries) {
      return [];
    }

    var crashDumpFiles = [];
    while (entries.hasMoreElements()) {
      var file = entries.getNext().QueryInterface(Ci.nsIFile);
      var path = String(file.path);
      if (path.match(/\.(dmp|extra)$/) && !aToIgnore[path]) {
        crashDumpFiles.push(path);
      }
    }
    return crashDumpFiles.concat();
  },

  _getURI: function (url) {
    return Services.io.newURI(url, null, null);
  },

  _readUrlAsString: function(aUrl) {
    // Fetch script content as we can't use scriptloader's loadSubScript
    // to evaluate http:// urls...
    var scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
                             .getService(Ci.nsIScriptableInputStream);

    var channel = NetUtil.newChannel({
      uri: aUrl,
      loadUsingSystemPrincipal: true
    });
    var input = channel.open2();
    scriptableStream.init(input);

    var str;
    var buffer = [];

    while ((str = scriptableStream.read(4096))) {
      buffer.push(str);
    }

    var output = buffer.join("");

    scriptableStream.close();
    input.close();

    var status;
    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
      status = channel.responseStatus;
    } catch(e) {
      /* The channel is not a nsIHttpCHannel, but that's fine */
      dump("-*- _readUrlAsString: Got an error while fetching " +
           "chrome script '" + aUrl + "': (" + e.name + ") " + e.message + ". " +
           "Ignoring.\n");
    }

    if (status == 404) {
      throw new SpecialPowersError(
        "Error while executing chrome script '" + aUrl + "':\n" +
        "The script doesn't exists. Ensure you have registered it in " +
        "'support-files' in your mochitest.ini.");
    }

    return output;
  },

  _sendReply: function(aMessage, aReplyName, aReplyMsg) {
    let mm = aMessage.target
                     .QueryInterface(Ci.nsIFrameLoaderOwner)
                     .frameLoader
                     .messageManager;
    mm.sendAsyncMessage(aReplyName, aReplyMsg);
  },

  _notifyCategoryAndObservers: function(subject, topic, data) {
    const serviceMarker = "service,";

    // First create observers from the category manager.
    let cm =
      Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    let enumerator = cm.enumerateCategory(topic);

    let observers = [];

    while (enumerator.hasMoreElements()) {
      let entry =
        enumerator.getNext().QueryInterface(Ci.nsISupportsCString).data;
      let contractID = cm.getCategoryEntry(topic, entry);

      let factoryFunction;
      if (contractID.substring(0, serviceMarker.length) == serviceMarker) {
        contractID = contractID.substring(serviceMarker.length);
        factoryFunction = "getService";
      }
      else {
        factoryFunction = "createInstance";
      }

      try {
        let handler = Cc[contractID][factoryFunction]();
        if (handler) {
          let observer = handler.QueryInterface(Ci.nsIObserver);
          observers.push(observer);
        }
      } catch(e) { }
    }

    // Next enumerate the registered observers.
    enumerator = Services.obs.enumerateObservers(topic);
    while (enumerator.hasMoreElements()) {
      try {
        let observer = enumerator.getNext().QueryInterface(Ci.nsIObserver);
        if (observers.indexOf(observer) == -1) {
          observers.push(observer);
        }
      } catch (e) { }
    }

    observers.forEach(function (observer) {
      try {
        observer.observe(subject, topic, data);
      } catch(e) { }
    });
  },

  /**
   * messageManager callback function
   * This will get requests from our API in the window and process them in chrome for it
   **/
  _receiveMessageAPI: function(aMessage) {
    // We explicitly return values in the below code so that this function
    // doesn't trigger a flurry of warnings about "does not always return
    // a value".
    switch(aMessage.name) {
      case "SPPrefService": {
        let prefs = Services.prefs;
        let prefType = aMessage.json.prefType.toUpperCase();
        let prefName = aMessage.json.prefName;
        let prefValue = "prefValue" in aMessage.json ? aMessage.json.prefValue : null;

        if (aMessage.json.op == "get") {
          if (!prefName || !prefType)
            throw new SpecialPowersError("Invalid parameters for get in SPPrefService");

          // return null if the pref doesn't exist
          if (prefs.getPrefType(prefName) == prefs.PREF_INVALID)
            return null;
        } else if (aMessage.json.op == "set") {
          if (!prefName || !prefType  || prefValue === null)
            throw new SpecialPowersError("Invalid parameters for set in SPPrefService");
        } else if (aMessage.json.op == "clear") {
          if (!prefName)
            throw new SpecialPowersError("Invalid parameters for clear in SPPrefService");
        } else {
          throw new SpecialPowersError("Invalid operation for SPPrefService");
        }

        // Now we make the call
        switch(prefType) {
          case "BOOL":
            if (aMessage.json.op == "get")
              return(prefs.getBoolPref(prefName));
            else 
              return(prefs.setBoolPref(prefName, prefValue));
          case "INT":
            if (aMessage.json.op == "get") 
              return(prefs.getIntPref(prefName));
            else
              return(prefs.setIntPref(prefName, prefValue));
          case "CHAR":
            if (aMessage.json.op == "get")
              return(prefs.getCharPref(prefName));
            else
              return(prefs.setCharPref(prefName, prefValue));
          case "COMPLEX":
            if (aMessage.json.op == "get")
              return(prefs.getComplexValue(prefName, prefValue[0]));
            else
              return(prefs.setComplexValue(prefName, prefValue[0], prefValue[1]));
          case "":
            if (aMessage.json.op == "clear") {
              prefs.clearUserPref(prefName);
              return undefined;
            }
        }
        return undefined;	// See comment at the beginning of this function.
      }

      case "SPProcessCrashService": {
        switch (aMessage.json.op) {
          case "register-observer":
            this._addProcessCrashObservers();
            break;
          case "unregister-observer":
            this._removeProcessCrashObservers();
            break;
          case "delete-crash-dump-files":
            return this._deleteCrashDumpFiles(aMessage.json.filenames);
          case "find-crash-dump-files":
            return this._findCrashDumpFiles(aMessage.json.crashDumpFilesToIgnore);
          default:
            throw new SpecialPowersError("Invalid operation for SPProcessCrashService");
        }
        return undefined;	// See comment at the beginning of this function.
      }

      case "SPPermissionManager": {
        let msg = aMessage.json;
        let principal = msg.principal;

        switch (msg.op) {
          case "add":
            Services.perms.addFromPrincipal(principal, msg.type, msg.permission, msg.expireType, msg.expireTime);
            break;
          case "remove":
            Services.perms.removeFromPrincipal(principal, msg.type);
            break;
          case "has":
            let hasPerm = Services.perms.testPermissionFromPrincipal(principal, msg.type);
            return hasPerm == Ci.nsIPermissionManager.ALLOW_ACTION;
          case "test":
            let testPerm = Services.perms.testPermissionFromPrincipal(principal, msg.type, msg.value);
            return testPerm == msg.value;
          default:
            throw new SpecialPowersError(
              "Invalid operation for SPPermissionManager");
        }
        return undefined;	// See comment at the beginning of this function.
      }

      case "SPSetTestPluginEnabledState": {
        var plugin = getTestPlugin(aMessage.data.pluginName);
        if (!plugin) {
          return undefined;
        }
        var oldEnabledState = plugin.enabledState;
        plugin.enabledState = aMessage.data.newEnabledState;
        return oldEnabledState;
      }

      case "SPWebAppService": {
        let Webapps = {};
        Components.utils.import("resource://gre/modules/Webapps.jsm", Webapps);
        switch (aMessage.json.op) {
          case "allow-unsigned-addons":
            {
              let utils = {};
              Components.utils.import("resource://gre/modules/AppsUtils.jsm", utils);
              utils.AppsUtils.allowUnsignedAddons = true;
              return;
            }
          case "debug-customizations":
            {
              let scope = {};
              Components.utils.import("resource://gre/modules/UserCustomizations.jsm", scope);
              scope.UserCustomizations._debug = aMessage.json.value;
              return;
            }
          case "inject-app":
            {
              let aAppId = aMessage.json.appId;
              let aApp   = aMessage.json.app;

              let keys = Object.keys(Webapps.DOMApplicationRegistry.webapps);
              let exists = keys.indexOf(aAppId) !== -1;
              if (exists) {
                return false;
              }

              Webapps.DOMApplicationRegistry.webapps[aAppId] = aApp;
              return true;
            }
          case "reject-app":
            {
              let aAppId = aMessage.json.appId;

              let keys = Object.keys(Webapps.DOMApplicationRegistry.webapps);
              let exists = keys.indexOf(aAppId) !== -1;
              if (!exists) {
                return false;
              }

              delete Webapps.DOMApplicationRegistry.webapps[aAppId];
              return true;
            }
          default:
            throw new SpecialPowersError("Invalid operation for SPWebAppsService");
        }
        return undefined;	// See comment at the beginning of this function.
      }

      case "SPObserverService": {
        let topic = aMessage.json.observerTopic;
        switch (aMessage.json.op) {
          case "notify":
            let data = aMessage.json.observerData
            Services.obs.notifyObservers(null, topic, data);
            break;
          case "add":
            this._registerObservers._self = this;
            this._registerObservers._add(topic);
            break;
          default:
            throw new SpecialPowersError("Invalid operation for SPObserverervice");
        }
        return undefined;	// See comment at the beginning of this function.
      }

      case "SPLoadChromeScript": {
        let id = aMessage.json.id;
        let jsScript;
        let scriptName;

        if (aMessage.json.url) {
          jsScript = this._readUrlAsString(aMessage.json.url);
          scriptName = aMessage.json.url;
        } else if (aMessage.json.function) {
          jsScript = aMessage.json.function.body;
          scriptName = aMessage.json.function.name
            || "<loadChromeScript anonymous function>";
        } else {
          throw new SpecialPowersError("SPLoadChromeScript: Invalid script");
        }

        // Setup a chrome sandbox that has access to sendAsyncMessage
        // and addMessageListener in order to communicate with
        // the mochitest.
        let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
        let sb = Components.utils.Sandbox(systemPrincipal);
        let mm = aMessage.target
                         .QueryInterface(Ci.nsIFrameLoaderOwner)
                         .frameLoader
                         .messageManager;
        sb.sendAsyncMessage = (name, message) => {
          mm.sendAsyncMessage("SPChromeScriptMessage",
                              { id: id, name: name, message: message });
        };
        sb.addMessageListener = (name, listener) => {
          this._chromeScriptListeners.push({ id: id, name: name, listener: listener });
        };
        sb.browserElement = aMessage.target;

        // Also expose assertion functions
        let reporter = function (err, message, stack) {
          // Pipe assertions back to parent process
          mm.sendAsyncMessage("SPChromeScriptAssert",
                              { id, name: scriptName, err, message,
                                stack });
        };
        Object.defineProperty(sb, "assert", {
          get: function () {
            let scope = Components.utils.createObjectIn(sb);
            Services.scriptloader.loadSubScript("chrome://specialpowers/content/Assert.jsm",
                                                scope);

            let assert = new scope.Assert(reporter);
            delete sb.assert;
            return sb.assert = assert;
          },
          configurable: true
        });

        // Evaluate the chrome script
        try {
          Components.utils.evalInSandbox(jsScript, sb, "1.8", scriptName, 1);
        } catch(e) {
          throw new SpecialPowersError(
            "Error while executing chrome script '" + scriptName + "':\n" +
            e + "\n" +
            e.fileName + ":" + e.lineNumber);
        }
        return undefined;	// See comment at the beginning of this function.
      }

      case "SPChromeScriptMessage": {
        let id = aMessage.json.id;
        let name = aMessage.json.name;
        let message = aMessage.json.message;
        return this._chromeScriptListeners
                   .filter(o => (o.name == name && o.id == id))
                   .map(o => o.listener(message));
      }

      case "SPImportInMainProcess": {
        var message = { hadError: false, errorMessage: null };
        try {
          Components.utils.import(aMessage.data);
        } catch (e) {
          message.hadError = true;
          message.errorMessage = e.toString();
        }
        return message;
      }

      case "SPCleanUpSTSData": {
        let origin = aMessage.data.origin;
        let flags = aMessage.data.flags;
        let uri = Services.io.newURI(origin, null, null);
        let sss = Cc["@mozilla.org/ssservice;1"].
                  getService(Ci.nsISiteSecurityService);
        sss.removeState(Ci.nsISiteSecurityService.HEADER_HSTS, uri, flags);
      }

      case "SPLoadExtension": {
        let {Extension} = Components.utils.import("resource://gre/modules/Extension.jsm", {});

        let id = aMessage.data.id;
        let ext = aMessage.data.ext;
        let extension = Extension.generate(ext);

        let resultListener = (...args) => {
          this._sendReply(aMessage, "SPExtensionMessage", {id, type: "testResult", args});
        };

        let messageListener = (...args) => {
          args.shift();
          this._sendReply(aMessage, "SPExtensionMessage", {id, type: "testMessage", args});
        };

        // Register pass/fail handlers.
        extension.on("test-result", resultListener);
        extension.on("test-eq", resultListener);
        extension.on("test-log", resultListener);
        extension.on("test-done", resultListener);

        extension.on("test-message", messageListener);

        this._extensions.set(id, extension);
        return undefined;
      }

      case "SPStartupExtension": {
        let {ExtensionData, Management} = Components.utils.import("resource://gre/modules/Extension.jsm", {});

        let id = aMessage.data.id;
        let extension = this._extensions.get(id);
        let startupListener = (msg, ext) => {
          if (ext == extension) {
            this._sendReply(aMessage, "SPExtensionMessage", {id, type: "extensionSetId", args: [extension.id]});
            Management.off("startup", startupListener);
          }
        };
        Management.on("startup", startupListener);

        // Make sure the extension passes the packaging checks when
        // they're run on a bare archive rather than a running instance,
        // as the add-on manager runs them.
        let extensionData = new ExtensionData(extension.rootURI);
        extensionData.readManifest().then(() => {
          return extensionData.initAllLocales();
        }).then(() => {
          if (extensionData.errors.length) {
            return Promise.reject("Extension contains packaging errors");
          }
          return extension.startup();
        }).then(() => {
          this._sendReply(aMessage, "SPExtensionMessage", {id, type: "extensionStarted", args: []});
        }).catch(e => {
          dump(`Extension startup failed: ${e}\n${e.stack}`);
          Management.off("startup", startupListener);
          this._sendReply(aMessage, "SPExtensionMessage", {id, type: "extensionFailed", args: []});
        });
        return undefined;
      }

      case "SPExtensionMessage": {
        let id = aMessage.data.id;
        let extension = this._extensions.get(id);
        extension.testMessage(...aMessage.data.args);
        return undefined;
      }

      case "SPUnloadExtension": {
        let id = aMessage.data.id;
        let extension = this._extensions.get(id);
        this._extensions.delete(id);
        extension.shutdown();
        this._sendReply(aMessage, "SPExtensionMessage", {id, type: "extensionUnloaded", args: []});
        return undefined;
      }

      case "SPClearAppPrivateData": {
        let appId = aMessage.data.appId;
        let browserOnly = aMessage.data.browserOnly;

        let attributes = { appId: appId };
        if (browserOnly) {
          attributes.inIsolatedMozBrowser = true;
        }
        this._notifyCategoryAndObservers(null,
                                         "clear-origin-data",
                                         JSON.stringify(attributes));

        let subject = {
          appId: appId,
          browserOnly: browserOnly,
          QueryInterface: XPCOMUtils.generateQI([Ci.mozIApplicationClearPrivateDataParams])
        };
        this._notifyCategoryAndObservers(subject, "webapps-clear-data", null);

        return undefined;
      }

      default:
        throw new SpecialPowersError("Unrecognized Special Powers API");
    }

    // We throw an exception before reaching this explicit return because
    // we should never be arriving here anyway.
    throw new SpecialPowersError("Unreached code");
    return undefined;
  }
};
