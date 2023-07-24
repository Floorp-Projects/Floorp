/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const {
  saveToDisk,
  alwaysAsk,
  useHelperApp,
  handleInternally,
  useSystemDefault,
} = Ci.nsIHandlerInfo;

const TOPIC_PDFJS_HANDLER_CHANGED = "pdfjs:handlerChanged";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  kHandlerList: "resource://gre/modules/handlers/HandlerList.sys.mjs",
  kHandlerListVersion: "resource://gre/modules/handlers/HandlerList.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  JSONFile: "resource://gre/modules/JSONFile.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "externalProtocolService",
  "@mozilla.org/uriloader/external-protocol-service;1",
  "nsIExternalProtocolService"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "MIMEService",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);

export function HandlerService() {
  // Observe handlersvc-json-replace so we can switch to the datasource
  Services.obs.addObserver(this, "handlersvc-json-replace", true);
}

HandlerService.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsISupportsWeakReference",
    "nsIHandlerService",
    "nsIObserver",
  ]),

  __store: null,
  get _store() {
    if (!this.__store) {
      this.__store = new lazy.JSONFile({
        path: PathUtils.join(
          Services.dirsvc.get("ProfD", Ci.nsIFile).path,
          "handlers.json"
        ),
        dataPostProcessor: this._dataPostProcessor.bind(this),
      });
    }

    // Always call this even if this.__store was set, since it may have been
    // set by asyncInit, which might not have completed yet.
    this._ensureStoreInitialized();
    return this.__store;
  },

  __storeInitialized: false,
  _ensureStoreInitialized() {
    if (!this.__storeInitialized) {
      this.__storeInitialized = true;
      this.__store.ensureDataReady();

      this._injectDefaultProtocolHandlersIfNeeded();
      this._migrateProtocolHandlersIfNeeded();

      Services.obs.notifyObservers(null, "handlersvc-store-initialized");
    }
  },

  _dataPostProcessor(data) {
    return data.defaultHandlersVersion
      ? data
      : {
          defaultHandlersVersion: {},
          mimeTypes: {},
          schemes: {},
          isDownloadsImprovementsAlreadyMigrated: false,
        };
  },

  /**
   * Injects new default protocol handlers if the version in the preferences is
   * newer than the one in the data store.
   */
  _injectDefaultProtocolHandlersIfNeeded() {
    try {
      let defaultHandlersVersion = Services.prefs.getIntPref(
        "gecko.handlerService.defaultHandlersVersion",
        0
      );
      if (defaultHandlersVersion < lazy.kHandlerListVersion) {
        this._injectDefaultProtocolHandlers();
        Services.prefs.setIntPref(
          "gecko.handlerService.defaultHandlersVersion",
          lazy.kHandlerListVersion
        );
        // Now save the result:
        this._store.saveSoon();
      }
    } catch (ex) {
      console.error(ex);
    }
  },

  _injectDefaultProtocolHandlers() {
    let locale = Services.locale.appLocaleAsBCP47;

    // Initialize handlers to default and update based on locale.
    let localeHandlers = lazy.kHandlerList.default;
    if (lazy.kHandlerList[locale]) {
      for (let scheme in lazy.kHandlerList[locale].schemes) {
        localeHandlers.schemes[scheme] =
          lazy.kHandlerList[locale].schemes[scheme];
      }
    }

    // Now, we're going to cheat. Terribly. The idiologically correct way
    // of implementing the following bit of code would be to fetch the
    // handler info objects from the protocol service, manipulate those,
    // and then store each of them.
    // However, that's expensive. It causes us to talk to the OS about
    // default apps, which causes the OS to go hit the disk.
    // All we're trying to do is insert some web apps into the list. We
    // don't care what's already in the file, we just want to do the
    // equivalent of appending into the database. So let's just go do that:
    for (let scheme of Object.keys(localeHandlers.schemes)) {
      if (scheme == "mailto" && AppConstants.MOZ_APP_NAME == "thunderbird") {
        // Thunderbird IS a mailto handler, it doesn't need handlers added.
        continue;
      }

      let existingSchemeInfo = this._store.data.schemes[scheme];
      if (!existingSchemeInfo) {
        // Haven't seen this scheme before. Default to asking which app the
        // user wants to use:
        existingSchemeInfo = {
          // Signal to future readers that we didn't ask the OS anything.
          // When the entry is first used, get the info from the OS.
          stubEntry: true,
          // The first item in the list is the preferred handler, and
          // there isn't one, so we fill in null:
          handlers: [null],
        };
        this._store.data.schemes[scheme] = existingSchemeInfo;
      }
      let { handlers } = existingSchemeInfo;
      for (let newHandler of localeHandlers.schemes[scheme].handlers) {
        if (!newHandler.uriTemplate) {
          console.error(
            `Ignoring protocol handler for ${scheme} without a uriTemplate!`
          );
          continue;
        }
        if (!newHandler.uriTemplate.startsWith("https://")) {
          console.error(
            `Ignoring protocol handler for ${scheme} with insecure template URL ${newHandler.uriTemplate}.`
          );
          continue;
        }
        if (!newHandler.uriTemplate.toLowerCase().includes("%s")) {
          console.error(
            `Ignoring protocol handler for ${scheme} with invalid template URL ${newHandler.uriTemplate}.`
          );
          continue;
        }
        // If there is already a handler registered with the same template
        // URL, ignore the new one:
        let matchingTemplate = handler =>
          handler && handler.uriTemplate == newHandler.uriTemplate;
        if (!handlers.some(matchingTemplate)) {
          handlers.push(newHandler);
        }
      }
    }
  },

  /**
   * Execute any migrations. Migrations are defined here for any changes or removals for
   * existing handlers. Additions are still handled via the localized prefs infrastructure.
   *
   * This depends on the browser.handlers.migrations pref being set by migrateUI in
   * nsBrowserGlue (for Fx Desktop) or similar mechanisms for other products.
   * This is a comma-separated list of identifiers of migrations that need running.
   * This avoids both re-running older migrations and keeping an additional
   * pref around permanently.
   */
  _migrateProtocolHandlersIfNeeded() {
    const kMigrations = {
      "30boxes": () => {
        const k30BoxesRegex =
          /^https?:\/\/(?:www\.)?30boxes.com\/external\/widget/i;
        let webcalHandler =
          lazy.externalProtocolService.getProtocolHandlerInfo("webcal");
        if (this.exists(webcalHandler)) {
          this.fillHandlerInfo(webcalHandler, "");
          let shouldStore = false;
          // First remove 30boxes from possible handlers.
          let handlers = webcalHandler.possibleApplicationHandlers;
          for (let i = handlers.length - 1; i >= 0; i--) {
            let app = handlers.queryElementAt(i, Ci.nsIHandlerApp);
            if (
              app instanceof Ci.nsIWebHandlerApp &&
              k30BoxesRegex.test(app.uriTemplate)
            ) {
              shouldStore = true;
              handlers.removeElementAt(i);
            }
          }
          // Then remove as a preferred handler.
          if (webcalHandler.preferredApplicationHandler) {
            let app = webcalHandler.preferredApplicationHandler;
            if (
              app instanceof Ci.nsIWebHandlerApp &&
              k30BoxesRegex.test(app.uriTemplate)
            ) {
              webcalHandler.preferredApplicationHandler = null;
              shouldStore = true;
            }
          }
          // Then store, if we changed anything.
          if (shouldStore) {
            this.store(webcalHandler);
          }
        }
      },
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1526890 for context.
      "secure-mail": () => {
        const kSubstitutions = new Map([
          [
            "http://compose.mail.yahoo.co.jp/ym/Compose?To=%s",
            "https://mail.yahoo.co.jp/compose/?To=%s",
          ],
          [
            "http://www.inbox.lv/rfc2368/?value=%s",
            "https://mail.inbox.lv/compose?to=%s",
          ],
          [
            "http://poczta.interia.pl/mh/?mailto=%s",
            "https://poczta.interia.pl/mh/?mailto=%s",
          ],
          [
            "http://win.mail.ru/cgi-bin/sentmsg?mailto=%s",
            "https://e.mail.ru/cgi-bin/sentmsg?mailto=%s",
          ],
        ]);

        function maybeReplaceURL(app) {
          if (app instanceof Ci.nsIWebHandlerApp) {
            let { uriTemplate } = app;
            let sub = kSubstitutions.get(uriTemplate);
            if (sub) {
              app.uriTemplate = sub;
              return true;
            }
          }
          return false;
        }
        let mailHandler =
          lazy.externalProtocolService.getProtocolHandlerInfo("mailto");
        if (this.exists(mailHandler)) {
          this.fillHandlerInfo(mailHandler, "");
          let handlers = mailHandler.possibleApplicationHandlers;
          let shouldStore = false;
          for (let i = handlers.length - 1; i >= 0; i--) {
            let app = handlers.queryElementAt(i, Ci.nsIHandlerApp);
            // Note: will evaluate the RHS because it's a binary rather than
            // logical or.
            shouldStore |= maybeReplaceURL(app);
          }
          // Then check the preferred handler.
          if (mailHandler.preferredApplicationHandler) {
            shouldStore |= maybeReplaceURL(
              mailHandler.preferredApplicationHandler
            );
          }
          // Then store, if we changed anything. Note that store() handles
          // duplicates, so we don't have to.
          if (shouldStore) {
            this.store(mailHandler);
          }
        }
      },
    };
    let migrationsToRun = Services.prefs.getCharPref(
      "browser.handlers.migrations",
      ""
    );
    migrationsToRun = migrationsToRun ? migrationsToRun.split(",") : [];
    for (let migration of migrationsToRun) {
      migration.trim();
      try {
        kMigrations[migration]();
      } catch (ex) {
        console.error(ex);
      }
    }

    if (migrationsToRun.length) {
      Services.prefs.clearUserPref("browser.handlers.migrations");
    }
  },

  _onDBChange() {
    return (async () => {
      if (this.__store) {
        await this.__store.finalize();
      }
      this.__store = null;
      this.__storeInitialized = false;
    })().catch(console.error);
  },

  // nsIObserver
  observe(subject, topic, data) {
    if (topic != "handlersvc-json-replace") {
      return;
    }
    let promise = this._onDBChange();
    promise.then(() => {
      Services.obs.notifyObservers(null, "handlersvc-json-replace-complete");
    });
  },

  // nsIHandlerService
  asyncInit() {
    if (!this.__store) {
      this.__store = new lazy.JSONFile({
        path: PathUtils.join(
          Services.dirsvc.get("ProfD", Ci.nsIFile).path,
          "handlers.json"
        ),
        dataPostProcessor: this._dataPostProcessor.bind(this),
      });
      this.__store
        .load()
        .then(() => {
          // __store can be null if we called _onDBChange in the mean time.
          if (this.__store) {
            this._ensureStoreInitialized();
          }
        })
        .catch(console.error);
    }
  },

  // nsIHandlerService
  enumerate() {
    let handlers = Cc["@mozilla.org/array;1"].createInstance(
      Ci.nsIMutableArray
    );
    for (let [type, typeInfo] of Object.entries(this._store.data.mimeTypes)) {
      let primaryExtension = typeInfo.extensions?.[0] ?? null;
      let handler = lazy.MIMEService.getFromTypeAndExtension(
        type,
        primaryExtension
      );
      handlers.appendElement(handler);
    }
    for (let type of Object.keys(this._store.data.schemes)) {
      // nsIExternalProtocolService.getProtocolHandlerInfo can be expensive
      // on Windows, so we return a proxy to delay retrieving the nsIHandlerInfo
      // until one of its properties is accessed.
      //
      // Note: our caller still needs to yield periodically when iterating
      // the enumerator and accessing handler properties to avoid monopolizing
      // the main thread.
      //
      let handler = new Proxy(
        {
          QueryInterface: ChromeUtils.generateQI(["nsIHandlerInfo"]),
          type,
          get _handlerInfo() {
            delete this._handlerInfo;
            return (this._handlerInfo =
              lazy.externalProtocolService.getProtocolHandlerInfo(type));
          },
        },
        {
          get(target, name) {
            return target[name] || target._handlerInfo[name];
          },
          set(target, name, value) {
            target._handlerInfo[name] = value;
          },
        }
      );
      handlers.appendElement(handler);
    }
    return handlers.enumerate(Ci.nsIHandlerInfo);
  },

  // nsIHandlerService
  store(handlerInfo) {
    let handlerList = this._getHandlerListByHandlerInfoType(handlerInfo);

    // Retrieve an existing entry if present, instead of creating a new one, so
    // that we preserve unknown properties for forward compatibility.
    let storedHandlerInfo = handlerList[handlerInfo.type];
    if (!storedHandlerInfo) {
      storedHandlerInfo = {};
      handlerList[handlerInfo.type] = storedHandlerInfo;
    }

    // Only a limited number of preferredAction values is allowed.
    if (
      handlerInfo.preferredAction == saveToDisk ||
      handlerInfo.preferredAction == useSystemDefault ||
      handlerInfo.preferredAction == handleInternally ||
      // For files (ie mimetype rather than protocol handling info), ensure
      // we can store the "always ask" state, too:
      (handlerInfo.preferredAction == alwaysAsk &&
        this._isMIMEInfo(handlerInfo))
    ) {
      storedHandlerInfo.action = handlerInfo.preferredAction;
    } else {
      storedHandlerInfo.action = useHelperApp;
    }

    if (handlerInfo.alwaysAskBeforeHandling) {
      storedHandlerInfo.ask = true;
    } else {
      delete storedHandlerInfo.ask;
    }

    // Build a list of unique nsIHandlerInfo instances to process later.
    let handlers = [];
    if (handlerInfo.preferredApplicationHandler) {
      handlers.push(handlerInfo.preferredApplicationHandler);
    }
    for (let handler of handlerInfo.possibleApplicationHandlers.enumerate(
      Ci.nsIHandlerApp
    )) {
      // If the caller stored duplicate handlers, we save them only once.
      if (!handlers.some(h => h.equals(handler))) {
        handlers.push(handler);
      }
    }

    // If any of the nsIHandlerInfo instances cannot be serialized, it is not
    // included in the final list. The first element is always the preferred
    // handler, or null if there is none.
    let serializableHandlers = handlers
      .map(h => this.handlerAppToSerializable(h))
      .filter(h => h);
    if (serializableHandlers.length) {
      if (!handlerInfo.preferredApplicationHandler) {
        serializableHandlers.unshift(null);
      }
      storedHandlerInfo.handlers = serializableHandlers;
    } else {
      delete storedHandlerInfo.handlers;
    }

    if (this._isMIMEInfo(handlerInfo)) {
      let extensions = storedHandlerInfo.extensions || [];
      for (let extension of handlerInfo.getFileExtensions()) {
        extension = extension.toLowerCase();
        // If the caller stored duplicate extensions, we save them only once.
        if (!extensions.includes(extension)) {
          extensions.push(extension);
        }
      }
      if (extensions.length) {
        storedHandlerInfo.extensions = extensions;
      } else {
        delete storedHandlerInfo.extensions;
      }
    }

    // If we're saving *anything*, it stops being a stub:
    delete storedHandlerInfo.stubEntry;

    this._store.saveSoon();

    // Now notify PDF.js. This is hacky, but a lot better than expecting all
    // the consumers to do it...
    if (handlerInfo.type == "application/pdf") {
      Services.obs.notifyObservers(null, TOPIC_PDFJS_HANDLER_CHANGED);
    }
  },

  // nsIHandlerService
  fillHandlerInfo(handlerInfo, overrideType) {
    let type = overrideType || handlerInfo.type;
    let storedHandlerInfo =
      this._getHandlerListByHandlerInfoType(handlerInfo)[type];
    if (!storedHandlerInfo) {
      throw new Components.Exception(
        "handlerSvc fillHandlerInfo: don't know this type",
        Cr.NS_ERROR_NOT_AVAILABLE
      );
    }

    let isStub = !!storedHandlerInfo.stubEntry;
    // In the normal case, this is not a stub, so we can just read stored info
    // and write to the handlerInfo object we were passed.
    if (!isStub) {
      handlerInfo.preferredAction = storedHandlerInfo.action;
      handlerInfo.alwaysAskBeforeHandling = !!storedHandlerInfo.ask;
    } else {
      // If we've got a stub, ensure the defaults are still set:
      lazy.externalProtocolService.setProtocolHandlerDefaults(
        handlerInfo,
        handlerInfo.hasDefaultHandler
      );
      if (
        handlerInfo.preferredAction == alwaysAsk &&
        handlerInfo.alwaysAskBeforeHandling
      ) {
        // `store` will default to `useHelperApp` because `alwaysAsk` is
        // not one of the 3 recognized options; for compatibility, do
        // the same here.
        handlerInfo.preferredAction = useHelperApp;
      }
    }
    // If it *is* a stub, don't override alwaysAskBeforeHandling or the
    // preferred actions. Instead, just append the stored handlers, without
    // overriding the preferred app, and then schedule a task to store proper
    // info for this handler.
    this._appendStoredHandlers(handlerInfo, storedHandlerInfo.handlers, isStub);

    if (this._isMIMEInfo(handlerInfo) && storedHandlerInfo.extensions) {
      for (let extension of storedHandlerInfo.extensions) {
        handlerInfo.appendExtension(extension);
      }
    } else if (this._mockedHandler) {
      this._insertMockedHandler(handlerInfo);
    }
  },

  /**
   * Private method to inject stored handler information into an nsIHandlerInfo
   * instance.
   * @param handlerInfo           the nsIHandlerInfo instance to write to
   * @param storedHandlers        the stored handlers
   * @param keepPreferredApp      whether to keep the handlerInfo's
   *                              preferredApplicationHandler or override it
   *                              (default: false, ie override it)
   */
  _appendStoredHandlers(handlerInfo, storedHandlers, keepPreferredApp) {
    // If the first item is not null, it is also the preferred handler. Since
    // we cannot modify the stored array, use a boolean to keep track of this.
    let isFirstItem = true;
    for (let handler of storedHandlers || [null]) {
      let handlerApp = this.handlerAppFromSerializable(handler || {});
      if (isFirstItem) {
        isFirstItem = false;
        // Do not overwrite the preferred app unless that's allowed
        if (!keepPreferredApp) {
          handlerInfo.preferredApplicationHandler = handlerApp;
        }
      }
      if (handlerApp) {
        handlerInfo.possibleApplicationHandlers.appendElement(handlerApp);
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
    } else if (handler instanceof Ci.nsIGIOMimeApp) {
      return {
        name: handler.name,
        command: handler.command,
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
        let file = new lazy.FileUtils.File(handlerObj.path);
        if (!file.exists()) {
          return null;
        }
        handlerApp = Cc[
          "@mozilla.org/uriloader/local-handler-app;1"
        ].createInstance(Ci.nsILocalHandlerApp);
        handlerApp.executable = file;
      } catch (ex) {
        return null;
      }
    } else if ("uriTemplate" in handlerObj) {
      handlerApp = Cc[
        "@mozilla.org/uriloader/web-handler-app;1"
      ].createInstance(Ci.nsIWebHandlerApp);
      handlerApp.uriTemplate = handlerObj.uriTemplate;
    } else if ("service" in handlerObj) {
      handlerApp = Cc[
        "@mozilla.org/uriloader/dbus-handler-app;1"
      ].createInstance(Ci.nsIDBusHandlerApp);
      handlerApp.service = handlerObj.service;
      handlerApp.method = handlerObj.method;
      handlerApp.objectPath = handlerObj.objectPath;
      handlerApp.dBusInterface = handlerObj.dBusInterface;
    } else if ("command" in handlerObj && "@mozilla.org/gio-service;1" in Cc) {
      try {
        handlerApp = Cc["@mozilla.org/gio-service;1"]
          .getService(Ci.nsIGIOService)
          .createAppFromCommand(handlerObj.command, handlerObj.name);
      } catch (ex) {
        return null;
      }
    } else {
      return null;
    }

    handlerApp.name = handlerObj.name;
    return handlerApp;
  },

  /**
   * The function returns a reference to the "mimeTypes" or "schemes" object
   * based on which type of handlerInfo is provided.
   */
  _getHandlerListByHandlerInfoType(handlerInfo) {
    return this._isMIMEInfo(handlerInfo)
      ? this._store.data.mimeTypes
      : this._store.data.schemes;
  },

  /**
   * Determines whether an nsIHandlerInfo instance represents a MIME type.
   */
  _isMIMEInfo(handlerInfo) {
    // We cannot rely only on the instanceof check because on Android both MIME
    // types and protocols are instances of nsIMIMEInfo. We still do the check
    // so that properties of nsIMIMEInfo become available to the callers.
    return (
      handlerInfo instanceof Ci.nsIMIMEInfo && handlerInfo.type.includes("/")
    );
  },

  // nsIHandlerService
  exists(handlerInfo) {
    return (
      handlerInfo.type in this._getHandlerListByHandlerInfoType(handlerInfo)
    );
  },

  // nsIHandlerService
  remove(handlerInfo) {
    delete this._getHandlerListByHandlerInfoType(handlerInfo)[handlerInfo.type];
    this._store.saveSoon();
  },

  // nsIHandlerService
  getTypeFromExtension(fileExtension) {
    let extension = fileExtension.toLowerCase();
    let mimeTypes = this._store.data.mimeTypes;
    for (let type of Object.keys(mimeTypes)) {
      if (
        mimeTypes[type].extensions &&
        mimeTypes[type].extensions.includes(extension)
      ) {
        return type;
      }
    }
    return "";
  },

  _mockedHandler: null,
  _mockedProtocol: null,

  _insertMockedHandler(handlerInfo) {
    if (handlerInfo.type == this._mockedProtocol) {
      handlerInfo.preferredApplicationHandler = this._mockedHandler;
      handlerInfo.possibleApplicationHandlers.insertElementAt(
        this._mockedHandler,
        0
      );
    }
  },

  // test-only: mock the handler instance for a particular protocol/scheme
  mockProtocolHandler(protocol) {
    if (!protocol) {
      this._mockedProtocol = null;
      this._mockedHandler = null;
      return;
    }
    this._mockedProtocol = protocol;
    this._mockedHandler = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsILocalHandlerApp]),
      launchWithURI(uri, context) {
        Services.obs.notifyObservers(uri, "mocked-protocol-handler");
      },
      name: "Mocked handler",
      detailedDescription: "Mocked handler for tests",
      equals(x) {
        return x == this;
      },
      get executable() {
        if (AppConstants.platform == "macosx") {
          // We need an app path that isn't us, nor in our app bundle, and
          // Apple no longer allows us to read the default-shipped apps
          // in /Applications/ - except for Safari, it would appear!
          let f = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
          f.initWithPath("/Applications/Safari.app");
          return f;
        }
        return Services.dirsvc.get("XCurProcD", Ci.nsIFile);
      },
      parameterCount: 0,
      clearParameters() {},
      appendParameter() {},
      getParameter() {},
      parameterExists() {
        return false;
      },
    };
  },
};
