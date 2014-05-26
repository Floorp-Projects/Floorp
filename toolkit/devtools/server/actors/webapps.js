/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Cc = Components.classes;
let Ci = Components.interfaces;
let CC = Components.Constructor;

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

let {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

function debug(aMsg) {
  /*
  Cc["@mozilla.org/consoleservice;1"]
    .getService(Ci.nsIConsoleService)
    .logStringMessage("--*-- WebappsActor : " + aMsg);
  */
}

function PackageUploadActor(file) {
  this._file = file;
  this._path = file.path;
}

PackageUploadActor.fromRequest = function(request, file) {
  if (request.bulk) {
    return new PackageUploadBulkActor(file);
  }
  return new PackageUploadJSONActor(file);
};

PackageUploadActor.prototype = {

  /**
   * This method isn't exposed to the client.
   * It is meant to be called by server code, in order to get
   * access to the temporary file out of the actor ID.
   */
  get filePath() {
    return this._path;
  },

  get openedFile() {
    if (this._openedFile) {
      return this._openedFile;
    }
    this._openedFile = this._openFile();
    return this._openedFile;
  },

  /**
   * This method allows you to delete the temporary file,
   * when you are done using it.
   */
  remove: function () {
    this._cleanupFile();
    return {};
  },

  _cleanupFile: function () {
    try {
      this._closeFile();
    } catch(e) {}
    try {
      OS.File.remove(this._path);
    } catch(e) {}
  }

};

/**
 * Create a new JSON package upload actor.
 * @param file nsIFile temporary file to write to
 */
function PackageUploadJSONActor(file) {
  PackageUploadActor.call(this, file);
  this._size = 0;
}

PackageUploadJSONActor.prototype = Object.create(PackageUploadActor.prototype);

PackageUploadJSONActor.prototype.actorPrefix = "packageUploadJSONActor";

PackageUploadJSONActor.prototype._openFile = function() {
  return OS.File.open(this._path, { write: true, truncate: true });
};

PackageUploadJSONActor.prototype._closeFile = function() {
  this.openedFile.then(file => file.close());
};

/**
 * This method allows you to upload a piece of file.
 * It expects a chunk argument that is the a string to write to the file.
 */
PackageUploadJSONActor.prototype.chunk = function(aRequest) {
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
  return this.openedFile
             .then(file => file.write(data))
             .then((written) => {
               this._size += written;
               return {
                 written: written,
                 _size: this._size
               };
             });
};

/**
 * This method needs to be called, when you are done uploading
 * chunks, before trying to access/use the temporary file.
 * Otherwise, the file may be partially written
 * and also be locked.
 */
PackageUploadJSONActor.prototype.done = function() {
  this._closeFile();
  return {};
};

/**
 * The request types this actor can handle.
 */
PackageUploadJSONActor.prototype.requestTypes = {
  "chunk": PackageUploadJSONActor.prototype.chunk,
  "done": PackageUploadJSONActor.prototype.done,
  "remove": PackageUploadJSONActor.prototype.remove
};

/**
 * Create a new bulk package upload actor.
 * @param file nsIFile temporary file to write to
 */
function PackageUploadBulkActor(file) {
  PackageUploadActor.call(this, file);
}

PackageUploadBulkActor.prototype = Object.create(PackageUploadActor.prototype);

PackageUploadBulkActor.prototype.actorPrefix = "packageUploadBulkActor";

PackageUploadBulkActor.prototype._openFile = function() {
  return FileUtils.openSafeFileOutputStream(this._file);
};

PackageUploadBulkActor.prototype._closeFile = function() {
  FileUtils.closeSafeFileOutputStream(this.openedFile);
};

PackageUploadBulkActor.prototype.stream = function({copyTo}) {
  copyTo(this.openedFile).then(() => {
    this._closeFile();
  });
};

/**
 * The request types this actor can handle.
 */
PackageUploadBulkActor.prototype.requestTypes = {
  "stream": PackageUploadBulkActor.prototype.stream,
  "remove": PackageUploadBulkActor.prototype.remove
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

  // Keep reference of already created app actors.
  // key: app frame message manager, value: ContentActor's grip() value
  this._appActorsMap = new Map();

  this.conn = aConnection;
  this._uploads = [];
  this._actorPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._actorPool);
}

WebappsActor.prototype = {
  actorPrefix: "webapps",

  disconnect: function () {
    try {
      this.unwatchApps();
    } catch(e) {}

    // When we stop using this actor, we should ensure removing all files.
    for (let upload of this._uploads) {
      upload.remove();
    }
    this._uploads = null;

    this.conn.removeActorPool(this._actorPool);
    this._actorPool = null;
    this.conn = null;
  },

  _registerApp: function wa_actorRegisterApp(aDeferred, aApp, aId, aDir) {
    debug("registerApp");
    let reg = DOMApplicationRegistry;
    let self = this;

    // Clean up the deprecated manifest cache if needed.
    if (aId in reg._manifestCache) {
      delete reg._manifestCache[aId];
    }

    aApp.installTime = Date.now();
    aApp.installState = "installed";
    aApp.removable = true;
    aApp.id = aId;
    aApp.basePath = reg.getWebAppsBasePath();
    aApp.localId = (aId in reg.webapps) ? reg.webapps[aId].localId
                                        : reg._nextLocalId();

    reg.webapps[aId] = aApp;
    reg.updatePermissionsForApp(aId);

    reg._readManifests([{ id: aId }]).then((aResult) => {
      let manifest = aResult[0].manifest;
      aApp.name = manifest.name;
      reg.updateAppHandlers(null, manifest, aApp);

      reg._saveApps().then(() => {
        aApp.manifest = manifest;

        // Needed to evict manifest cache on content side
        // (has to be dispatched first, otherwise other messages like
        // Install:Return:OK are going to use old manifest version)
        reg.broadcastMessage("Webapps:UpdateState", {
          app: aApp,
          manifest: manifest,
          id: aApp.id
        });
        reg.broadcastMessage("Webapps:FireEvent", {
          eventType: ["downloadsuccess", "downloadapplied"],
          manifestURL: aApp.manifestURL
        });
        reg.broadcastMessage("Webapps:AddApp", { id: aId, app: aApp });
        reg.broadcastMessage("Webapps:Install:Return:OK", {
          app: aApp,
          oid: "foo",
          requestID: "bar"
        });

        Services.obs.notifyObservers(null, "webapps-installed",
          JSON.stringify({ manifestURL: aApp.manifestURL }));

        delete aApp.manifest;
        aDeferred.resolve({ appId: aId, path: aDir.path });

        // We can't have appcache for packaged apps.
        if (!aApp.origin.startsWith("app://")) {
          reg.startOfflineCacheDownload(new ManifestHelper(manifest, aApp.origin));
        }
      });
      // Cleanup by removing the temporary directory.
      if (aDir.exists())
        aDir.remove(true);
    });
  },

  _sendError: function wa_actorSendError(aDeferred, aMsg, aId) {
    debug("Sending error: " + aMsg);
    aDeferred.resolve({
      error: "installationFailed",
      message: aMsg,
      appId: aId
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

  _createTmpPackage: function() {
    let tmpDir = FileUtils.getDir("TmpD", ["file-upload"], true, false);
    if (!tmpDir.exists() || !tmpDir.isDirectory()) {
      return {
        error: "fileAccessError",
        message: "Unable to create temporary folder"
      };
    }
    let tmpFile = tmpDir;
    tmpFile.append("package.zip");
    tmpFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));
    if (!tmpFile.exists() || !tmpDir.isFile()) {
      return {
        error: "fileAccessError",
        message: "Unable to create temporary file"
      };
    }
    return tmpFile;
  },

  uploadPackage: function (request) {
    debug("uploadPackage");

    let tmpFile = this._createTmpPackage();
    if ("error" in tmpFile) {
      return tmpFile;
    }

    let actor = PackageUploadActor.fromRequest(request, tmpFile);
    this._actorPool.addActor(actor);
    this._uploads.push(actor);
    return { actor: actor.actorID };
  },

  installHostedApp: function wa_actorInstallHosted(aDir, aId, aReceipts,
                                                   aManifest, aMetadata) {
    debug("installHostedApp");
    let self = this;
    let deferred = promise.defer();

    function readManifest() {
      if (aManifest) {
        return promise.resolve(aManifest);
      } else {
        let manFile = OS.Path.join(aDir.path, "manifest.webapp");
        return AppsUtils.loadJSONAsync(manFile);
      }
    }
    function checkSideloading(aManifest) {
      return self._getAppType(aManifest.type);
    }
    function writeManifest(aAppType) {
      // Move manifest.webapp to the destination directory.
      // The destination directory for this app.
      let installDir = DOMApplicationRegistry._getAppDir(aId);
      if (aManifest) {
        let manFile = OS.Path.join(installDir.path, "manifest.webapp");
        return DOMApplicationRegistry._writeFile(manFile, JSON.stringify(aManifest)).then(() => {
          return aAppType;
        });
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
      let metaFile = OS.Path.join(aDir.path, "metadata.json");
      return AppsUtils.loadJSONAsync(metaFile).then((aMetadata) => {
        if (!aMetadata) {
          throw("Error parsing metadata.json.");
        }
        if (!aMetadata.origin) {
          throw("Missing 'origin' property in metadata.json");
        }
        return { metadata: aMetadata, appType: aAppType };
      });
    }
    let runnable = {
      run: function run() {
        try {
          readManifest().
            then(writeManifest).
            then(checkSideloading).
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

              self._registerApp(deferred, app, aId, aDir);
            }, function (error) {
              self._sendError(deferred, error, aId);
            });
        } catch(e) {
          // If anything goes wrong, just send it back.
          self._sendError(deferred, e.toString(), aId);
        }
      }
    }

    Services.tm.currentThread.dispatch(runnable,
                                       Ci.nsIThread.DISPATCH_NORMAL);
    return deferred.promise;
  },

  installPackagedApp: function wa_actorInstallPackaged(aDir, aId, aReceipts) {
    debug("installPackagedApp");
    let self = this;
    let deferred = promise.defer();

    let runnable = {
      run: function run() {
        try {
          // Open the app zip package
          let zipFile = aDir.clone();
          zipFile.append("application.zip");
          let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                            .createInstance(Ci.nsIZipReader);
          zipReader.open(zipFile);

          // Read app manifest `manifest.webapp` from `application.zip`
          let istream = zipReader.getInputStream("manifest.webapp");
          let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                            .createInstance(Ci.nsIScriptableUnicodeConverter);
          converter.charset = "UTF-8";
          let jsonString = converter.ConvertToUnicode(
            NetUtil.readInputStreamToString(istream, istream.available())
          );

          let manifest;
          try {
            manifest = JSON.parse(jsonString);
          } catch(e) {
            self._sendError(deferred, "Error Parsing manifest.webapp: " + e, aId);
          }

          let appType = self._getAppType(manifest.type);

          // Privileged and certified packaged apps can setup a custom origin
          // via `origin` manifest property
          let id = aId;
          if (appType >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED &&
              manifest.origin !== undefined) {
            let uri;
            try {
              uri = Services.io.newURI(manifest.origin, null, null);
            } catch(e) {
              self._sendError(deferred, "Invalid origin in webapp's manifest", aId);
            }

            if (uri.scheme != "app") {
              self._sendError(deferred, "Invalid origin in webapp's manifest", aId);
            }
            id = uri.prePath.substring(6);
          }

          // Only after security checks are made and after final app id is computed
          // we can move application.zip to the destination directory, and
          // extract manifest.webapp there.
          let installDir = DOMApplicationRegistry._getAppDir(id);
          let manFile = installDir.clone();
          manFile.append("manifest.webapp");
          zipReader.extract("manifest.webapp", manFile);
          zipReader.close();
          zipFile.moveTo(installDir, "application.zip");

          let origin = "app://" + id;
          let manifestURL = origin + "/manifest.webapp";

          // Refresh application.zip content (e.g. reinstall app), as done here:
          // http://hg.mozilla.org/mozilla-central/annotate/aaefec5d34f8/dom/apps/src/Webapps.jsm#l1125
          // Do it in parent process for the simulator
          let jar = installDir.clone();
          jar.append("application.zip");
          Services.obs.notifyObservers(jar, "flush-cache-entry", null);

          // And then in app content process
          // This function will be evaluated in the scope of the content process
          // frame script. That will flush the jar cache for this app and allow
          // loading fresh updated resources if we reload its document.
          let FlushFrameScript = function (path) {
            let jar = Components.classes["@mozilla.org/file/local;1"]
                                .createInstance(Components.interfaces.nsILocalFile);
            jar.initWithPath(path);
            let obs = Components.classes["@mozilla.org/observer-service;1"]
                                .getService(Components.interfaces.nsIObserverService);
            obs.notifyObservers(jar, "flush-cache-entry", null);
          };
          for each (let frame in self._appFrames()) {
            if (frame.getAttribute("mozapp") == manifestURL) {
              let mm = frame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
              mm.loadFrameScript("data:," +
                encodeURIComponent("(" + FlushFrameScript.toString() + ")" +
                                   "('" + jar.path + "')"), false);
            }
          }

          // Create a fake app object with the minimum set of properties we need.
          let app = {
            origin: origin,
            installOrigin: origin,
            manifestURL: manifestURL,
            appStatus: appType,
            receipts: aReceipts,
          }

          self._registerApp(deferred, app, id, aDir);
        } catch(e) {
          // If anything goes wrong, just send it back.
          self._sendError(deferred, e.toString(), aId);
        }
      }
    }

    Services.tm.currentThread.dispatch(runnable,
                                       Ci.nsIThread.DISPATCH_NORMAL);
    return deferred.promise;
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
    let reg = DOMApplicationRegistry;
    if (!appId) {
      appId = reg.makeAppId();
    }

    // Check that we are not overriding a preinstalled application.
    if (appId in reg.webapps && reg.webapps[appId].removable === false) {
      return { error: "badParameterType",
               message: "The application " + appId + " can't be overriden."
             }
    }

    let appDir = FileUtils.getDir("TmpD", ["b2g", appId], false, false);

    if (aRequest.upload) {
      // Ensure creating the directory (recursively)
      appDir = FileUtils.getDir("TmpD", ["b2g", appId], true, false);
      let actor = this.conn.getActor(aRequest.upload);
      if (!actor) {
        return { error: "badParameter",
                 message: "Unable to find upload actor '" + aRequest.upload
                          + "'" };
      }
      let appFile = FileUtils.File(actor.filePath);
      if (!appFile.exists()) {
        return { error: "badParameter",
                 message: "The uploaded file doesn't exist on device" };
      }
      appFile.moveTo(appDir, "application.zip");
    } else if ((!appDir || !appDir.exists()) &&
               !aRequest.manifest && !aRequest.metadata) {
      return { error: "badParameterType",
               message: "missing directory " + appDir.path
             };
    }

    let testFile = appDir.clone();
    testFile.append("application.zip");

    let receipts = (aRequest.receipts && Array.isArray(aRequest.receipts))
                    ? aRequest.receipts
                    : [];

    if (testFile.exists()) {
      return this.installPackagedApp(appDir, appId, receipts);
    }

    let manifest, metadata;
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
                 message: "hosted app file and manifest/metadata fields " +
                          "are missing"
        };
      }
    }

    return this.installHostedApp(appDir, appId, receipts, manifest, metadata);
  },

  getAll: function wa_actorGetAll(aRequest) {
    debug("getAll");

    let deferred = promise.defer();
    let reg = DOMApplicationRegistry;
    reg.getAll(apps => {
      deferred.resolve({ apps: this._filterAllowedApps(apps) });
    });

    return deferred.promise;
  },

  getApp: function wa_actorGetApp(aRequest) {
    debug("getApp");

    let manifestURL = aRequest.manifestURL;
    if (!manifestURL) {
      return { error: "missingParameter",
               message: "missing parameter manifestURL" };
    }

    let reg = DOMApplicationRegistry;
    let app = reg.getAppByManifestURL(manifestURL);
    if (!app) {
      return { error: "appNotFound" };
    }

    return this._isAppAllowedForURL(app.manifestURL).then(allowed => {
      if (!allowed) {
        return { error: "forbidden" };
      }
      return reg.getManifestFor(manifestURL).then(function (manifest) {
        app.manifest = manifest;
        return { app: app };
      });
    });
  },

  _areCertifiedAppsAllowed: function wa__areCertifiedAppsAllowed() {
    let pref = "devtools.debugger.forbid-certified-apps";
    return !Services.prefs.getBoolPref(pref);
  },

  _isAppAllowedForManifest: function wa__isAppAllowedForManifest(aManifest) {
    if (this._areCertifiedAppsAllowed()) {
      return true;
    }
    let type = this._getAppType(aManifest.type);
    return type !== Ci.nsIPrincipal.APP_STATUS_CERTIFIED;
  },

  _filterAllowedApps: function wa__filterAllowedApps(aApps) {
    return aApps.filter(app => this._isAppAllowedForManifest(app.manifest));
  },

  _isAppAllowedForURL: function wa__isAppAllowedForURL(aManifestURL) {
    return this._findManifestByURL(aManifestURL).then(manifest => {
      return this._isAppAllowedForManifest(manifest);
    });
  },

  uninstall: function wa_actorUninstall(aRequest) {
    debug("uninstall");

    let manifestURL = aRequest.manifestURL;
    if (!manifestURL) {
      return { error: "missingParameter",
               message: "missing parameter manifestURL" };
    }

    let deferred = promise.defer();
    let reg = DOMApplicationRegistry;
    reg.uninstall(
      manifestURL,
      function onsuccess() {
        deferred.resolve({});
      },
      function onfailure(reason) {
        deferred.resolve({ error: reason });
      }
    );

    return deferred.promise;
  },

  _findManifestByURL: function wa__findManifestByURL(aManifestURL) {
    let deferred = promise.defer();

    let reg = DOMApplicationRegistry;
    let id = reg._appIdForManifestURL(aManifestURL);

    reg._readManifests([{ id: id }]).then((aResults) => {
      deferred.resolve(aResults[0].manifest);
    });

    return deferred.promise;
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

    this._findManifestByURL(manifestURL).then(jsonManifest => {
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
          message: "The icon file '" + iconURL + "' doesn't exist"
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

    let deferred = promise.defer();

    DOMApplicationRegistry.launch(
      aRequest.manifestURL,
      aRequest.startPoint || "",
      Date.now(),
      function onsuccess() {
        deferred.resolve({});
      },
      function onfailure(reason) {
        deferred.resolve({ error: reason });
      });

    return deferred.promise;
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
    // For now, we only support app frames on b2g
    if (Services.appinfo.ID != "{3c2e2abc-06d4-11e1-ac3b-374f68613e61}") {
      return;
    }
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

    let appPromises = [];
    let apps = [];

    for each (let frame in this._appFrames()) {
      let manifestURL = frame.getAttribute("mozapp");

      appPromises.push(this._isAppAllowedForURL(manifestURL).then(allowed => {
        if (allowed) {
          apps.push(manifestURL);
        }
      }));
    }

    return promise.all(appPromises).then(() => {
      return { apps: apps };
    });
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

    let notFoundError = {
      error: "appNotFound",
      message: "Unable to find any opened app whose manifest " +
               "is '" + manifestURL + "'"
    };

    if (!appFrame) {
      return notFoundError;
    }

    return this._isAppAllowedForURL(manifestURL).then(allowed => {
      if (!allowed) {
        return notFoundError;
      }

      // Only create a new actor, if we haven't already
      // instanciated one for this connection.
      let map = this._appActorsMap;
      let mm = appFrame.QueryInterface(Ci.nsIFrameLoaderOwner)
                       .frameLoader
                       .messageManager;
      let actor = map.get(mm);
      if (!actor) {
        let onConnect = actor => {
          map.set(mm, actor);
          return { actor: actor };
        };
        let onDisconnect = mm => {
          map.delete(mm);
        };
        return DebuggerServer.connectToChild(this.conn, appFrame, onDisconnect)
                             .then(onConnect);
      }

      return { actor: actor };
    });
  },

  watchApps: function () {
    this._openedApps = new Set();
    // For now, app open/close events are only implement on b2g
    if (Services.appinfo.ID == "{3c2e2abc-06d4-11e1-ac3b-374f68613e61}") {
      let chromeWindow = Services.wm.getMostRecentWindow('navigator:browser');
      let systemAppFrame = chromeWindow.getContentWindow();
      systemAppFrame.addEventListener("appwillopen", this);
      systemAppFrame.addEventListener("appterminated", this);
    }
    Services.obs.addObserver(this, "webapps-installed", false);
    Services.obs.addObserver(this, "webapps-uninstall", false);

    return {};
  },

  unwatchApps: function () {
    this._openedApps = null;
    if (Services.appinfo.ID == "{3c2e2abc-06d4-11e1-ac3b-374f68613e61}") {
      let chromeWindow = Services.wm.getMostRecentWindow('navigator:browser');
      let systemAppFrame = chromeWindow.getContentWindow();
      systemAppFrame.removeEventListener("appwillopen", this);
      systemAppFrame.removeEventListener("appterminated", this);
    }
    Services.obs.removeObserver(this, "webapps-installed", false);
    Services.obs.removeObserver(this, "webapps-uninstall", false);

    return {};
  },

  handleEvent: function (event) {
    let manifestURL;
    switch(event.type) {
      case "appwillopen":
        manifestURL = event.detail.manifestURL;

        // Ignore the event if we already received an appwillopen for this app
        // (appwillopen is also fired when the app has been moved to background
        // and get back to foreground)
        if (this._openedApps.has(manifestURL)) {
          return;
        }
        this._openedApps.add(manifestURL);

        this._isAppAllowedForURL(manifestURL).then(allowed => {
          if (allowed) {
            this.conn.send({ from: this.actorID,
                             type: "appOpen",
                             manifestURL: manifestURL
                           });
          }
        });

        break;

      case "appterminated":
        manifestURL = event.detail.manifestURL;
        this._openedApps.delete(manifestURL);

        this._isAppAllowedForURL(manifestURL).then(allowed => {
          if (allowed) {
            this.conn.send({ from: this.actorID,
                             type: "appClose",
                             manifestURL: manifestURL
                           });
          }
        });

        break;
    }
  },

  observe: function (subject, topic, data) {
    let app = JSON.parse(data);
    if (topic == "webapps-installed") {
      this.conn.send({ from: this.actorID,
                       type: "appInstall",
                       manifestURL: app.manifestURL
                     });
    } else if (topic == "webapps-uninstall") {
      this.conn.send({ from: this.actorID,
                       type: "appUninstall",
                       manifestURL: app.manifestURL
                     });
    }
  }
};

/**
 * The request types this actor can handle.
 */
WebappsActor.prototype.requestTypes = {
  "install": WebappsActor.prototype.install,
  "uploadPackage": WebappsActor.prototype.uploadPackage,
  "getAll": WebappsActor.prototype.getAll,
  "getApp": WebappsActor.prototype.getApp,
  "launch": WebappsActor.prototype.launch,
  "close": WebappsActor.prototype.close,
  "uninstall": WebappsActor.prototype.uninstall,
  "listRunningApps": WebappsActor.prototype.listRunningApps,
  "getAppActor": WebappsActor.prototype.getAppActor,
  "watchApps": WebappsActor.prototype.watchApps,
  "unwatchApps": WebappsActor.prototype.unwatchApps,
  "getIconAsDataURL": WebappsActor.prototype.getIconAsDataURL
};

DebuggerServer.addGlobalActor(WebappsActor, "webappsActor");
