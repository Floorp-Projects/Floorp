/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gExternalProtocolService",
                                   "@mozilla.org/uriloader/external-protocol-service;1",
                                   "nsIExternalProtocolService");
XPCOMUtils.defineLazyServiceGetter(this, "gMIMEService",
                                   "@mozilla.org/mime;1",
                                   "nsIMIMEService");

function HandlerService() {
  // Observe handlersvc-json-replace so we can switch to the datasource
  Services.obs.addObserver(this, "handlersvc-json-replace", true);
}

HandlerService.prototype = {

  classID: Components.ID("{220cc253-b60f-41f6-b9cf-fdcb325f970f}"),
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsISupportsWeakReference,
    Ci.nsIHandlerService,
    Ci.nsIObserver
  ]),

  __store: null,
  get _store() {
    if (!this.__store) {
      this.__store = new JSONFile({
        path: OS.Path.join(OS.Constants.Path.profileDir,
                           "handlers.json"),
        dataPostProcessor: this._dataPostProcessor.bind(this),
      });
      this.__store.ensureDataReady();
      this._updateDB();
    }
    return this.__store;
  },

  _dataPostProcessor(data) {
    return data.schemes ? data : { version: {}, mimetypes: {}, schemes: {} };
  },

  _updateDB() {
    try {

      let locale = Services.locale.getAppLocaleAsLangTag();
      let prefsDefaultHandlersVersion = Number(Services.prefs.getComplexValue(
        "gecko.handlerService.defaultHandlersVersion",
        Ci.nsIPrefLocalizedString).data);

      let defaultHandlersVersion = this._store.data.version[locale] || 0;
      if (defaultHandlersVersion < prefsDefaultHandlersVersion ) {
        this._injectNewDefaults();
        this._store.data.version[locale] = prefsDefaultHandlersVersion;
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
  },

  _injectNewDefaults() {
    let schemesPrefBranch = Services.prefs.getBranch("gecko.handlerService.schemes.");
    let schemePrefList = schemesPrefBranch.getChildList("");

    let schemes = {};

    // read all the scheme prefs into a hash
    for (let schemePrefName of schemePrefList) {

      let [scheme, handlerNumber, attribute] = schemePrefName.split(".");

      try {
        let attrData =
          schemesPrefBranch.getComplexValue(schemePrefName,
                                            Ci.nsIPrefLocalizedString).data;
        if (!(scheme in schemes)) {
          schemes[scheme] = {};
        }

        if (!(handlerNumber in schemes[scheme])) {
          schemes[scheme][handlerNumber] = {};
        }

        schemes[scheme][handlerNumber][attribute] = attrData;
      } catch (ex) {}
    }

    for (let scheme of Object.keys(schemes)) {

      // This clause is essentially a reimplementation of
      // nsIExternalProtocolHandlerService.getProtocolHandlerInfo().
      // Necessary because we want to use this instance of the service,
      // but nsIExternalProtocolHandlerService would call the RDF-based based version
      // until we complete the conversion.
      let osDefaultHandlerFound = {};
      let protoInfo = gExternalProtocolService.getProtocolHandlerInfoFromOS(scheme,
                                                                            osDefaultHandlerFound);

      if (this.exists(protoInfo)) {
        this.fillHandlerInfo(protoInfo, null);
      } else {
        gExternalProtocolService.setProtocolHandlerDefaults(protoInfo,
                                                            osDefaultHandlerFound.value);
      }

      // cache the possible handlers to avoid extra xpconnect traversals.
      let possibleHandlers = protoInfo.possibleApplicationHandlers;

      for (let handlerNumber of Object.keys(schemes[scheme])) {
        let handlerApp = this.handlerAppFromSerializable(schemes[scheme][handlerNumber]);
        if (!this._isInHandlerArray(possibleHandlers, handlerApp)) {
          possibleHandlers.appendElement(handlerApp, false);
        }
      }

      this.store(protoInfo);
    }
  },

  _isInHandlerArray(array, handler) {
    let enumerator = array.enumerate();
    while (enumerator.hasMoreElements()) {
      let handlerApp = enumerator.getNext().QueryInterface(Ci.nsIHandlerApp);
      if (handlerApp.equals(handler)) {
        return true;
      }
    }
    return false;
  },

  _onDBChange() {
    return Task.spawn(function* () {
      if (this.__store) {
        yield this.__store.finalize();
      }
      this.__store = null;
    }.bind(this)).catch(Cu.reportError);
  },

  // nsIObserver
  observe(subject, topic, data) {
    if (topic != "handlersvc-json-replace") {
      return;
    }
    let promise = this._onDBChange();
    promise.then(() => {
      Services.obs.notifyObservers(null, "handlersvc-json-replace-complete", null);
    });
  },

  // nsIHandlerService
  enumerate() {
    let handlers = Cc["@mozilla.org/array;1"].
                     createInstance(Ci.nsIMutableArray);
    for (let type of Object.keys(this._store.data.mimetypes)) {
      let handler = gMIMEService.getFromTypeAndExtension(type, null);
      handlers.appendElement(handler, false);
    }
    for (let type of Object.keys(this._store.data.schemes)) {
      let handler = gExternalProtocolService.getProtocolHandlerInfo(type);
      handlers.appendElement(handler, false);
    }
    return handlers.enumerate();
  },

  // nsIHandlerService
  store(handlerInfo) {
    let handlerObj = {
      action: handlerInfo.preferredAction,
      askBeforeHandling: handlerInfo.alwaysAskBeforeHandling,
    };

    let preferredHandler = handlerInfo.preferredApplicationHandler;
    if (preferredHandler) {
      let serializable = this.handlerAppToSerializable(preferredHandler);
      if (serializable) {
        handlerObj.preferredHandler = serializable;
      }
    }

    let apps = handlerInfo.possibleApplicationHandlers.enumerate();
    let possibleHandlers = [];
    while (apps.hasMoreElements()) {
      let handler = apps.getNext().QueryInterface(Ci.nsIHandlerApp);
      let serializable = this.handlerAppToSerializable(handler);
      if (serializable) {
        possibleHandlers.push(serializable);
      }
    }
    if (possibleHandlers.length) {
      handlerObj.possibleHandlers = possibleHandlers;
    }

    if (this._isMIMEInfo(handlerInfo)) {
      let extEnumerator = handlerInfo.getFileExtensions();
      let extensions = [];
      while (extEnumerator.hasMore()) {
        let extension = extEnumerator.getNext();
        if (!extensions.includes(extension)) {
          extensions.push(extension);
        }
      }
      if (extensions.length) {
        handlerObj.fileExtensions = extensions;
      }
    }
    this._getHandlerListByHandlerInfoType(handlerInfo)[handlerInfo.type] = handlerObj;
    this._store.saveSoon();
  },

  // nsIHandlerService
  fillHandlerInfo(handlerInfo, overrideType) {
    let type = overrideType || handlerInfo.type;
    let storedHandlerInfo = this._getHandlerListByHandlerInfoType(handlerInfo)[type];
    if (!storedHandlerInfo) {
      throw new Components.Exception("handlerSvc fillHandlerInfo: don't know this type",
                                     Cr.NS_ERROR_NOT_AVAILABLE);
    }

    // logic from _retrievePreferredAction of nsHandlerService.js
    if (storedHandlerInfo.action == Ci.nsIHandlerInfo.saveToDisk ||
        storedHandlerInfo.action == Ci.nsIHandlerInfo.useSystemDefault ||
        storedHandlerInfo.action == Ci.nsIHandlerInfo.handleInternally) {
      handlerInfo.preferredAction = storedHandlerInfo.action;
    } else {
      handlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
    }

    let preferHandler = null;
    if (storedHandlerInfo.preferredHandler) {
      preferHandler = this.handlerAppFromSerializable(storedHandlerInfo.preferredHandler);
    }
    handlerInfo.preferredApplicationHandler = preferHandler;
    if (preferHandler) {
      handlerInfo.possibleApplicationHandlers.appendElement(preferHandler, false);
    }

    if (storedHandlerInfo.possibleHandlers) {
      for (let handler of storedHandlerInfo.possibleHandlers) {
        let possibleHandler = this.handlerAppFromSerializable(handler);
        if (possibleHandler && (!preferHandler ||
                                !possibleHandler.equals(preferHandler))) {
          handlerInfo.possibleApplicationHandlers.appendElement(possibleHandler, false);
        }
      }
    }

    // We always store "askBeforeHandling" in the JSON file. Just use this value.
    handlerInfo.alwaysAskBeforeHandling = storedHandlerInfo.askBeforeHandling;

    if (this._isMIMEInfo(handlerInfo)) {
      if (storedHandlerInfo.fileExtensions) {
        for (let extension of storedHandlerInfo.fileExtensions) {
          handlerInfo.appendExtension(extension);
        }
      }
    }
  },

  /**
   * @param handler
   *        A nsIHandlerApp handler app
   * @returns  Serializable representation of a handler app object.
   */
  handlerAppToSerializable(handler) {
    if (handler instanceof Ci.nsILocalHandlerApp) {
      return {
        name: handler.name,
        path: handler.executable.path,
      };
    } else if (handler instanceof Ci.nsIWebHandlerApp) {
      return {
        name: handler.name,
        uriTemplate: handler.uriTemplate,
      };
    } else if (handler instanceof Ci.nsIDBusHandlerApp) {
      return {
        name: handler.name,
        service: handler.service,
        method: handler.method,
        objectPath: handler.objectPath,
        dBusInterface: handler.dBusInterface,
      };
    }
    // If the handler is an unknown handler type, return null.
    // Android default application handler is the case.
    return null;
  },

  /**
   * @param handlerObj
   *        Serializable representation of a handler object.
   * @returns  {nsIHandlerApp}  the handler app, if any; otherwise null
   */
  handlerAppFromSerializable(handlerObj) {
    let handlerApp;
    if ("path" in handlerObj) {
      try {
        let file = new FileUtils.File(handlerObj.path);
        if (!file.exists()) {
          return null;
        }
        handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                   createInstance(Ci.nsILocalHandlerApp);
        handlerApp.executable = file;
      } catch (ex) {
        return null;
      }
    } else if ("uriTemplate" in handlerObj) {
      handlerApp = Cc["@mozilla.org/uriloader/web-handler-app;1"].
                   createInstance(Ci.nsIWebHandlerApp);
      handlerApp.uriTemplate = handlerObj.uriTemplate;
    } else if ("service" in handlerObj) {
      handlerApp = Cc["@mozilla.org/uriloader/dbus-handler-app;1"].
                   createInstance(Ci.nsIDBusHandlerApp);
      handlerApp.service = handlerObj.service;
      handlerApp.method = handlerObj.method;
      handlerApp.objectPath = handlerObj.objectPath;
      handlerApp.dBusInterface = handlerObj.dBusInterface;
    } else {
      return null;
    }

    handlerApp.name = handlerObj.name;
    return handlerApp;
  },

  /**
   * The function return a reference to the "mimetypes" or "schemes" object
   * based on which type of handlerInfo is provided.
   */
  _getHandlerListByHandlerInfoType(handlerInfo) {
    return this._isMIMEInfo(handlerInfo) ? this._store.data.mimetypes
                                         : this._store.data.schemes;
  },

  /**
   * Determines whether an nsIHandlerInfo instance represents a MIME type.
   */
  _isMIMEInfo(handlerInfo) {
    // We cannot rely only on the instanceof check because on Android both MIME
    // types and protocols are instances of nsIMIMEInfo. We still do the check
    // so that properties of nsIMIMEInfo become available to the callers.
    return handlerInfo instanceof Ci.nsIMIMEInfo &&
           handlerInfo.type.includes("/");
  },

  // nsIHandlerService
  exists(handlerInfo) {
    return handlerInfo.type in this._getHandlerListByHandlerInfoType(handlerInfo);
  },

  // nsIHandlerService
  remove(handlerInfo) {
    delete this._getHandlerListByHandlerInfoType(handlerInfo)[handlerInfo.type];
    this._store.saveSoon();
  },

  // nsIHandlerService
  getTypeFromExtension(fileExtension) {
    let extension = fileExtension.toLowerCase();
    let mimeTypes = this._store.data.mimetypes;
    for (let type of Object.keys(mimeTypes)) {
      if (mimeTypes[type].fileExtensions &&
          mimeTypes[type].fileExtensions.includes(extension)) {
          return type;
      }
    }
    return "";
  },

};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HandlerService]);
