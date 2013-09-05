/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Cc = Components.classes;
let Ci = Components.interfaces;
let CC = Components.Constructor;

Cu.import("resource://gre/modules/osfile.jsm");

let promise;

function debug(aMsg) {
  /*
  Cc["@mozilla.org/consoleservice;1"]
    .getService(Ci.nsIConsoleService)
    .logStringMessage("--*-- WebappsActor : " + aMsg);
  */
}

function PackageUploadActor(aPath, aFile) {
  this._path = aPath;
  this._file = aFile;
  this.size = 0;
}

PackageUploadActor.prototype = {
  actorPrefix: "packageUploadActor",

  /**
   * This method isn't exposed to the client.
   * It is meant to be called by server code, in order to get
   * access to the temporary file out of the actor ID.
   */
  getFilePath: function () {
    return this._path;
  },

  /**
   * This method allows you to upload a piece of file.
   * It expects a chunk argument that is the a string to write to the file.
   */
  chunk: function (aRequest) {
    let chunk = aRequest.chunk;
    if (!chunk || chunk.length <= 0) {
      return {error: "parameterError",
              message: "Missing or invalid chunk argument"};
    }
    // Translate the string used to transfer the chunk over JSON
    // back to a typed array
    let data = new Uint8Array(chunk.length);
    for (let i = 0, l = chunk.length; i < l ; i++) {
      data[i] = chunk.charCodeAt(i);
    }
    return this._file.write(data)
               .then((written) => {
                 this.size += written;
                 return {
                   written: written,
                   size: this.size
                 };
               });
  },

  /**
   * This method needs to be called, when you are done uploading
   * chunks, before trying to access/use the temporary file.
   * Otherwise, the file may be partially written
   * and also be locked.
   */
  done: function (aRequest) {
    return this._file.close();
  },

  /**
   * This method allows you to delete the temporary file,
   * when you are done using it.
   */
  remove: function (aRequest) {
    this._cleanupFile();

    return {};
  },

  _cleanupFile: function () {
    try {
      this._file.close();
    } catch(e) {}
    try {
      OS.File.remove(this._path);
    } catch(e) {}
  }
};

/**
 * The request types this actor can handle.
 */
PackageUploadActor.prototype.requestTypes = {
  "chunk": PackageUploadActor.prototype.chunk,
  "done": PackageUploadActor.prototype.done,
  "remove": PackageUploadActor.prototype.remove
};

/**
 * Creates a WebappsActor. WebappsActor provides remote access to
 * install apps.
 */
function WebappsActor(aConnection) {
  debug("init");
  // Load actor dependencies lazily as this actor require extra environnement
  // preparation to work (like have a profile setup in xpcshell tests)

  Cu.import("resource://gre/modules/Webapps.jsm");
  Cu.import("resource://gre/modules/AppsUtils.jsm");
  Cu.import("resource://gre/modules/FileUtils.jsm");
  Cu.import('resource://gre/modules/Services.jsm');
  promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js").Promise;

  // Keep reference of already created app actors.
  // key: app frame message manager, value: ContentTabActor's grip() value
  this._appActorsMap = new Map();

  this.conn = aConnection;
  this._uploads = [];
  this._actorPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._actorPool);
}

WebappsActor.prototype = {
  actorPrefix: "webapps",

  disconnect: function () {
    // When we stop using this actor, we should ensure removing all files.
    for (let upload of this._uploads) {
      upload.remove();
    }
    this._uploads = null;

    this.conn.removeActorPool(this._actorPool);
    this._actorPool = null;
    this.conn = null;
  },

  _registerApp: function wa_actorRegisterApp(aApp, aId, aDir) {
    debug("registerApp");
    let reg = DOMApplicationRegistry;
    let self = this;

    aApp.installTime = Date.now();
    aApp.installState = "installed";
    aApp.removable = true;
    aApp.id = aId;
    aApp.basePath = reg.getWebAppsBasePath();
    aApp.localId = (aId in reg.webapps) ? reg.webapps[aId].localId
                                        : reg._nextLocalId();

    reg.webapps[aId] = aApp;
    reg.updatePermissionsForApp(aId);

    reg._readManifests([{ id: aId }], function(aResult) {
      let manifest = aResult[0].manifest;
      aApp.name = manifest.name;
      reg.updateAppHandlers(null, manifest, aApp);

      reg._saveApps(function() {
        aApp.manifest = manifest;
        reg.broadcastMessage("Webapps:AddApp", { id: aId, app: aApp });
        reg.broadcastMessage("Webapps:Install:Return:OK",
                             { app: aApp,
                               oid: "foo",
                               requestID: "bar"
                             });
        delete aApp.manifest;
        self.conn.send({ from: self.actorID,
                         type: "webappsEvent",
                         appId: aId
                       });

        // We can't have appcache for packaged apps.
        if (!aApp.origin.startsWith("app://")) {
          reg.startOfflineCacheDownload(new ManifestHelper(manifest, aApp.origin));
        }
      });
      // Cleanup by removing the temporary directory.
      aDir.remove(true);
    });
  },

  _sendError: function wa_actorSendError(aMsg, aId) {
    debug("Sending error: " + aMsg);
    this.conn.send(
      { from: this.actorID,
        type: "webappsEvent",
        appId: aId,
        error: "installationFailed",
        message: aMsg
      });
  },

  _getAppType: function wa_actorGetAppType(aType) {
    let type = Ci.nsIPrincipal.APP_STATUS_INSTALLED;

    if (aType) {
      type = aType == "privileged" ? Ci.nsIPrincipal.APP_STATUS_PRIVILEGED
           : aType == "certified" ? Ci.nsIPrincipal.APP_STATUS_CERTIFIED
           : Ci.nsIPrincipal.APP_STATUS_INSTALLED;
    }

    return type;
  },

  uploadPackage: function () {
    debug("uploadPackage\n");
    let tmpDir = FileUtils.getDir("TmpD", ["file-upload"], true, false);
    if (!tmpDir.exists() || !tmpDir.isDirectory()) {
      return {error: "fileAccessError",
              message: "Unable to create temporary folder"};
    }
    let tmpFile = tmpDir;
    tmpFile.append("package.zip");
    tmpFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));
    if (!tmpFile.exists() || !tmpDir.isFile()) {
      return {error: "fileAccessError",
              message: "Unable to create temporary file"};
    }

    return OS.File.open(tmpFile.path, { write: true, truncate: true })
             .then((file) => {
                let actor = new PackageUploadActor(tmpFile.path, file);
                this._actorPool.addActor(actor);
                this._uploads.push(actor);
                return { actor: actor.actorID };
             });
  },

  installHostedApp: function wa_actorInstallHosted(aDir, aId, aReceipts,
                                                   aManifest, aMetadata) {
    debug("installHostedApp");
    let self = this;

    function readManifest() {
      if (aManifest) {
        return promise.resolve(aManifest);
      } else {
        let deferred = promise.defer();
        let manFile = aDir.clone();
        manFile.append("manifest.webapp");
        DOMApplicationRegistry._loadJSONAsync(manFile, function(aManifest) {
          if (!aManifest) {
            deferred.reject("Error parsing manifest.webapp.");
          } else {
            deferred.resolve(aManifest);
          }
        });
        return deferred.promise;
      }
    }
    function checkSideloading(aManifest) {
      let appType = self._getAppType(aManifest.type);

      // In production builds, don't allow installation of certified apps.
      if (!DOMApplicationRegistry.allowSideloadingCertified &&
          appType == Ci.nsIPrincipal.APP_STATUS_CERTIFIED) {
        throw new Error("Installing certified apps is not allowed.");
      }
      return appType;
    }
    function writeManifest(aAppType) {
      // Move manifest.webapp to the destination directory.
      // The destination directory for this app.
      let installDir = DOMApplicationRegistry._getAppDir(aId);
      if (aManifest) {
        let deferred = promise.defer();
        let manFile = installDir.clone();
        manFile.append("manifest.webapp");
        DOMApplicationRegistry._writeFile(manFile, JSON.stringify(aManifest), function () {
          deferred.resolve(aAppType);
        });
        return deferred.promise;
      } else {
        let manFile = aDir.clone();
        manFile.append("manifest.webapp");
        manFile.moveTo(installDir, "manifest.webapp");
      }
      return null;
    }
    function readMetadata(aAppType) {
      if (aMetadata) {
        return { metadata: aMetadata, appType: aAppType };
      }
      // Read the origin and manifest url from metadata.json
      let deferred = promise.defer();
      let metaFile = aDir.clone();
      metaFile.append("metadata.json");
      DOMApplicationRegistry._loadJSONAsync(metaFile, function(aMetadata) {
        if (!aMetadata) {
          deferred.reject("Error parsing metadata.json.");
          return;
        }
        if (!aMetadata.origin) {
          deferred.reject("Missing 'origin' property in metadata.json");
          return;
        }
        deferred.resolve({ metadata: aMetadata, appType: aAppType });
      });
      return deferred.promise;
    }
    let runnable = {
      run: function run() {
        try {
          readManifest().
            then(checkSideloading).
            then(writeManifest).
            then(readMetadata).
            then(function ({ metadata, appType }) {
              let origin = metadata.origin;
              let manifestURL = metadata.manifestURL ||
                                origin + "/manifest.webapp";
              // Create a fake app object with the minimum set of properties we need.
              let app = {
                origin: origin,
                installOrigin: metadata.installOrigin || origin,
                manifestURL: manifestURL,
                appStatus: appType,
                receipts: aReceipts,
              };

              self._registerApp(app, aId, aDir);
            }, function (error) {
              self._sendError(error, aId);
            });
        } catch(e) {
          // If anything goes wrong, just send it back.
          self._sendError(e.toString(), aId);
        }
      }
    }

    Services.tm.currentThread.dispatch(runnable,
                                       Ci.nsIThread.DISPATCH_NORMAL);
  },

  installPackagedApp: function wa_actorInstallPackaged(aDir, aId, aReceipts) {
    debug("installPackagedApp");
    let self = this;

    let runnable = {
      run: function run() {
        try {
          // The destination directory for this app.
          let installDir = DOMApplicationRegistry._getAppDir(aId);

          // Move application.zip to the destination directory, and
          // extract manifest.webapp there.
          let zipFile = aDir.clone();
          zipFile.append("application.zip");
          let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                            .createInstance(Ci.nsIZipReader);
          zipReader.open(zipFile);
          let manFile = installDir.clone();
          manFile.append("manifest.webapp");
          zipReader.extract("manifest.webapp", manFile);
          zipReader.close();
          zipFile.moveTo(installDir, "application.zip");

          DOMApplicationRegistry._loadJSONAsync(manFile, function(aManifest) {
            if (!aManifest) {
              self._sendError("Error Parsing manifest.webapp", aId);
            }

            let appType = self._getAppType(aManifest.type);

            // In production builds, don't allow installation of certified apps.
            if (!DOMApplicationRegistry.allowSideloadingCertified &&
                appType == Ci.nsIPrincipal.APP_STATUS_CERTIFIED) {
              self._sendError("Installing certified apps is not allowed.", aId);
              return;
            }

            let origin = "app://" + aId;

            // Create a fake app object with the minimum set of properties we need.
            let app = {
              origin: origin,
              installOrigin: origin,
              manifestURL: origin + "/manifest.webapp",
              appStatus: appType,
              receipts: aReceipts,
            }

            self._registerApp(app, aId, aDir);
          });
        } catch(e) {
          // If anything goes wrong, just send it back.
          self._sendError(e.toString(), aId);
        }
      }
    }

    Services.tm.currentThread.dispatch(runnable,
                                       Ci.nsIThread.DISPATCH_NORMAL);
  },

  /**
    * @param appId   : The id of the app we want to install. We will look for
    *                  the files for the app in $TMP/b2g/$appId :
    *                  For packaged apps: application.zip
    *                  For hosted apps:   metadata.json and manifest.webapp
    */
  install: function wa_actorInstall(aRequest) {
    debug("install");

    let appId = aRequest.appId;
    if (!appId) {
      return { error: "missingParameter",
               message: "missing parameter appId" }
    }

    // Check that we are not overriding a preinstalled application.
    let reg = DOMApplicationRegistry;
    if (appId in reg.webapps && reg.webapps[appId].removable === false) {
      return { error: "badParameterType",
               message: "The application " + appId + " can't be overriden."
             }
    }

    let appDir = FileUtils.getDir("TmpD", ["b2g", appId], false, false);

    if (!appDir || !appDir.exists()) {
      if (aRequest.upload) {
        // Ensure creating the directory (recursively)
        appDir = FileUtils.getDir("TmpD", ["b2g", appId], true, false);
        let actor = this.conn.getActor(aRequest.upload);
        if (!actor) {
          return { error: "badParameter",
                   message: "Unable to find upload actor '" + aRequest.upload
                            + "'" };
        }
        let appFile = FileUtils.File(actor.getFilePath());
        if (!appFile.exists()) {
          return { error: "badParameter",
                   message: "The uploaded file doesn't exist on device" };
        }
        appFile.moveTo(appDir, "application.zip");
      }
      else if (!aRequest.manifest && !aRequest.metadata) {
        return { error: "badParameterType",
                 message: "missing directory " + appDir.path
               }
      }
    }

    let testFile = appDir.clone();
    testFile.append("application.zip");

    let receipts = (aRequest.receipts && Array.isArray(aRequest.receipts))
                    ? aRequest.receipts
                    : [];
    let manifest, metadata;

    if (testFile.exists()) {
      this.installPackagedApp(appDir, appId, receipts);
    } else {
      let missing =
        ["manifest.webapp", "metadata.json"]
        .some(function(aName) {
          testFile = appDir.clone();
          testFile.append(aName);
          return !testFile.exists();
        });

      if (missing) {
        if (aRequest.manifest && aRequest.metadata &&
            aRequest.metadata.origin) {
          manifest = aRequest.manifest;
          metadata = aRequest.metadata;
        } else {
          try {
            appDir.remove(true);
          } catch(e) {}
            return { error: "badParameterType",
                     message: "hosted app file and manifest/metadata fields are missing" };
        }
      }

      this.installHostedApp(appDir, appId, receipts, manifest, metadata);
    }

    return { appId: appId, path: appDir.path }
  },

  getAll: function wa_actorGetAll(aRequest) {
    debug("getAll");

    let defer = promise.defer();
    let reg = DOMApplicationRegistry;
    reg.getAll(function onsuccess(apps) {
      defer.resolve({ apps: apps });
    });

    return defer.promise;
  },

  uninstall: function wa_actorUninstall(aRequest) {
    debug("uninstall");

    let manifestURL = aRequest.manifestURL;
    if (!manifestURL) {
      return { error: "missingParameter",
               message: "missing parameter manifestURL" };
    }

    let defer = promise.defer();
    let reg = DOMApplicationRegistry;
    reg.uninstall(
      manifestURL,
      function onsuccess() {
        defer.resolve({});
      },
      function onfailure(reason) {
        defer.resolve({ error: reason });
      }
    );

    return defer.promise;
  },

  getIconAsDataURL: function (aRequest) {
    debug("getIconAsDataURL");

    let manifestURL = aRequest.manifestURL;
    if (!manifestURL) {
      return { error: "missingParameter",
               message: "missing parameter manifestURL" };
    }

    let reg = DOMApplicationRegistry;
    let app = reg.getAppByManifestURL(manifestURL);
    if (!app) {
      return { error: "wrongParameter",
               message: "No application for " + manifestURL };
    }

    let deferred = promise.defer();

    let id = reg._appIdForManifestURL(manifestURL);
    reg._readManifests([{ id: id }], function (aResults) {
      let jsonManifest = aResults[0].manifest;
      let manifest = new ManifestHelper(jsonManifest, app.origin);
      let iconURL = manifest.iconURLForSize(aRequest.size || 128);
      if (!iconURL) {
        deferred.resolve({
          error: "noIcon",
          message: "This app has no icon"
        });
        return;
      }

      // Download the URL as a blob
      // bug 899177: there is a bug with xhr and app:// and jar:// uris
      // that ends up forcing the content type to application/xml.
      let req = Cc['@mozilla.org/xmlextras/xmlhttprequest;1']
                  .createInstance(Ci.nsIXMLHttpRequest);
      req.open("GET", iconURL, false);
      req.responseType = "blob";

      try {
        req.send(null);
      } catch(e) {
        deferred.resolve({
          error: "noIcon",
          message: "The icon file '" + iconURL + "' doesn't exists"
        });
        return;
      }

      // Convert the blog to a base64 encoded data URI
      let reader = Cc["@mozilla.org/files/filereader;1"]
                     .createInstance(Ci.nsIDOMFileReader);
      reader.onload = function () {
        deferred.resolve({
          url: reader.result
        });
      };
      reader.onerror = function () {
        deferred.resolve({
          error: reader.error.name,
          message: String(reader.error)
        });
      };
      reader.readAsDataURL(req.response);
    });

    return deferred.promise;
  },

  launch: function wa_actorLaunch(aRequest) {
    debug("launch");

    let manifestURL = aRequest.manifestURL;
    if (!manifestURL) {
      return { error: "missingParameter",
               message: "missing parameter manifestURL" };
    }

    let defer = promise.defer();
    let reg = DOMApplicationRegistry;
    reg.launch(
      aRequest.manifestURL,
      aRequest.startPoint || "",
      function onsuccess() {
        defer.resolve({});
      },
      function onfailure(reason) {
        defer.resolve({ error: reason });
      });

    return defer.promise;
  },

  close: function wa_actorLaunch(aRequest) {
    debug("close");

    let manifestURL = aRequest.manifestURL;
    if (!manifestURL) {
      return { error: "missingParameter",
               message: "missing parameter manifestURL" };
    }

    let reg = DOMApplicationRegistry;
    let app = reg.getAppByManifestURL(manifestURL);
    if (!app) {
      return { error: "missingParameter",
               message: "No application for " + manifestURL };
    }

    reg.close(app);

    return {};
  },

  _appFrames: function () {
    // Register the system app
    let chromeWindow = Services.wm.getMostRecentWindow('navigator:browser');
    let systemAppFrame = chromeWindow.shell.contentBrowser;
    yield systemAppFrame;

    // Register apps hosted in the system app. i.e. the homescreen, all regular
    // apps and the keyboard.
    // Bookmark apps and other system app internal frames like captive portal
    // are also hosted in system app, but they are not using mozapp attribute.
    let frames = systemAppFrame.contentDocument.querySelectorAll("iframe[mozapp]");
    for (let i = 0; i < frames.length; i++) {
      yield frames[i];
    }
  },

  listRunningApps: function (aRequest) {
    debug("listRunningApps\n");

    let apps = [];

    for each (let frame in this._appFrames()) {
      let manifestURL = frame.getAttribute("mozapp");
      apps.push(manifestURL);
    }

    return { apps: apps };
  },

  _connectToApp: function (aFrame) {
    let defer = Promise.defer();

    let mm = aFrame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
    mm.loadFrameScript("resource://gre/modules/devtools/server/child.js", false);

    let childTransport, prefix;

    let onActorCreated = makeInfallible(function (msg) {
      mm.removeMessageListener("debug:actor", onActorCreated);

      dump("***** Got debug:actor\n");
      let { actor, appId } = msg.json;
      prefix = msg.json.prefix;

      // Pipe Debugger message from/to parent/child via the message manager
      childTransport = new ChildDebuggerTransport(mm, prefix);
      childTransport.hooks = {
        onPacket: this.conn.send.bind(this.conn),
        onClosed: function () {}
      };
      childTransport.ready();

      this.conn.setForwarding(prefix, childTransport);

      debug("establishing forwarding for app with prefix " + prefix);

      this._appActorsMap.set(mm, actor);

      defer.resolve(actor);
    }).bind(this);
    mm.addMessageListener("debug:actor", onActorCreated);

    let onMessageManagerDisconnect = makeInfallible(function (subject, topic, data) {
      if (subject == mm) {
        Services.obs.removeObserver(onMessageManagerDisconnect, topic);
        if (childTransport) {
          // If we have a child transport, the actor has already
          // been created. We need to stop using this message manager.
          childTransport.close();
          this.conn.cancelForwarding(prefix);
        } else {
          // Otherwise, the app has been closed before the actor
          // had a chance to be created, so we are not able to create
          // the actor.
          defer.resolve(null);
        }
        this._appActorsMap.delete(mm);
      }
    }).bind(this);
    Services.obs.addObserver(onMessageManagerDisconnect,
                             "message-manager-disconnect", false);

    let prefixStart = this.conn.prefix + "child";
    mm.sendAsyncMessage("debug:connect", { prefix: prefixStart });

    return defer.promise;
  },

  getAppActor: function ({ manifestURL }) {
    debug("getAppActor\n");

    let appFrame = null;
    for each (let frame in this._appFrames()) {
      if (frame.getAttribute("mozapp") == manifestURL) {
        appFrame = frame;
        break;
      }
    }

    if (!appFrame) {
      return { error: "appNotFound",
               message: "Unable to find any opened app whose manifest " +
                        "is '" + manifestURL + "'" };
    }

    // Only create a new actor, if we haven't already
    // instanciated one for this connection.
    let mm = appFrame.QueryInterface(Ci.nsIFrameLoaderOwner)
                     .frameLoader
                     .messageManager;
    let actor = this._appActorsMap.get(mm);
    if (!actor) {
      return this._connectToApp(appFrame)
                 .then(function (actor) ({ actor: actor }));
    }

    return { actor: actor };
  },

  watchApps: function () {
    this._openedApps = new Set();
    let chromeWindow = Services.wm.getMostRecentWindow('navigator:browser');
    let systemAppFrame = chromeWindow.getContentWindow();
    systemAppFrame.addEventListener("appwillopen", this);
    systemAppFrame.addEventListener("appterminated", this);

    return {};
  },

  unwatchApps: function () {
    this._openedApps = null;
    let chromeWindow = Services.wm.getMostRecentWindow('navigator:browser');
    let systemAppFrame = chromeWindow.getContentWindow();
    systemAppFrame.removeEventListener("appwillopen", this);
    systemAppFrame.removeEventListener("appterminated", this);

    return {};
  },

  handleEvent: function (event) {
    let manifestURL;
    switch(event.type) {
      case "appwillopen":
        let frame = event.target;
        manifestURL = frame.getAttribute("mozapp")

        // Ignore the event if we already received an appwillopen for this app
        // (appwillopen is also fired when the app has been moved to background
        // and get back to foreground)
        if (this._openedApps.has(manifestURL)) {
          return;
        }
        this._openedApps.add(manifestURL);

        this.conn.send({ from: this.actorID,
                         type: "appOpen",
                         manifestURL: manifestURL
                       });
        break;

      case "appterminated":
        manifestURL = event.detail.manifestURL;
        this._openedApps.delete(manifestURL);
        this.conn.send({ from: this.actorID,
                         type: "appClose",
                         manifestURL: manifestURL
                       });
        break;
    }
  }
};

/**
 * The request types this actor can handle.
 */
WebappsActor.prototype.requestTypes = {
  "install": WebappsActor.prototype.install
};

// Until we implement unix domain socket, we only enable app install
// only on production devices
if (Services.prefs.getBoolPref("devtools.debugger.enable-content-actors")) {
  let requestTypes = WebappsActor.prototype.requestTypes;
  requestTypes.uploadPackage = WebappsActor.prototype.uploadPackage;
  requestTypes.getAll = WebappsActor.prototype.getAll;
  requestTypes.launch = WebappsActor.prototype.launch;
  requestTypes.close  = WebappsActor.prototype.close;
  requestTypes.uninstall = WebappsActor.prototype.uninstall;
  requestTypes.listRunningApps = WebappsActor.prototype.listRunningApps;
  requestTypes.getAppActor = WebappsActor.prototype.getAppActor;
  requestTypes.watchApps = WebappsActor.prototype.watchApps;
  requestTypes.unwatchApps = WebappsActor.prototype.unwatchApps;
  requestTypes.getIconAsDataURL = WebappsActor.prototype.getIconAsDataURL;
}

DebuggerServer.addGlobalActor(WebappsActor, "webappsActor");
