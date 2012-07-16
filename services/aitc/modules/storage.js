/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AitcStorage", "AitcQueue"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");

/**
 * Provides a file-backed queue. Currently used by manager.js as persistent
 * storage to manage pending installs and uninstalls.
 *
 * @param filename
 *        (String)    The file backing this queue will be named as this string.
 *
 * @param cb
 *        (Function)  This function will be called when the queue is ready to
 *                    use. *DO NOT* call any methods on this object until the
 *                    callback is invoked, if you do so, none of your operations
 *                    will be persisted on disk.
 *
 */
function AitcQueue(filename, cb) {
  if (!cb) {
    throw new Error("AitcQueue constructor called without callback");
  }

  this._log = Log4Moz.repository.getLogger("Service.AITC.Storage.Queue");
  this._log.level = Log4Moz.Level[Preferences.get(
    "services.aitc.storage.log.level"
  )];

  this._queue = [];
  this._writeLock = false;
  this._filePath = "webapps/" + filename;

  this._log.info("AitcQueue instance loading");

  CommonUtils.jsonLoad(this._filePath, this, function jsonLoaded(data) {
    if (data && Array.isArray(data)) {
      this._queue = data;
    }
    this._log.info("AitcQueue instance created");
    cb(true);
  });
}
AitcQueue.prototype = {
  /**
   * Add an object to the queue, and data is saved to disk.
   */
  enqueue: function enqueue(obj, cb) {
    this._log.info("Adding to queue " + obj);

    if (!cb) {
      throw new Error("enqueue called without callback");
    }

    let self = this;
    this._queue.push(obj);

    try {
      this._putFile(this._queue, function _enqueuePutFile(err) {
        if (err) {
          // Write unsuccessful, don't add to queue.
          self._queue.pop();
          cb(err, false);
          return;
        }
        // Successful write.
        cb(null, true);
        return;
      });
    } catch (e) {
      self._queue.pop();
      cb(e, false);
    }
  },

  /**
   * Remove the object at the head of the queue, and data is saved to disk.
   */
  dequeue: function dequeue(cb) {
    this._log.info("Removing head of queue");

    if (!cb) {
      throw new Error("dequeue called without callback");
    }
    if (!this._queue.length) {
      throw new Error("Queue is empty");
    }

    let self = this;
    let obj = this._queue.shift();

    try {
      this._putFile(this._queue, function _dequeuePutFile(err) {
        if (!err) {
          // Successful write.
          cb(null, true);
          return;
        }
        // Unsuccessful write, put back in queue.
        self._queue.unshift(obj);
        cb(err, false);
      });
    } catch (e) {
      self._queue.unshift(obj);
      cb(e, false);
    }
  },

  /**
   * Return the object at the front of the queue without removing it.
   */
  peek: function peek() {
    this._log.info("Peek called when length " + this._queue.length);
    if (!this._queue.length) {
      throw new Error("Queue is empty");
    }
    return this._queue[0];
  },

  /**
   * Find out the length of the queue.
   */
  get length() {
    return this._queue.length;
  },

  /**
   * Put an array into the cache file. Will throw an exception if there is
   * an error while trying to write to the file.
   */
  _putFile: function _putFile(value, cb) {
    if (this._writeLock) {
      throw new Error("_putFile already in progress");
    }

    this._writeLock = true;
    this._log.info("Writing queue to disk");
    CommonUtils.jsonSave(this._filePath, this, value, function jsonSaved(err) {
      if (err) {
        let msg = new Error("_putFile failed with " + err);
        this._writeLock = false;
        cb(msg);
        return;
      }
      this._log.info("_putFile succeeded");
      this._writeLock = false;
      cb(null);
    });
  },
};

/**
 * An interface to DOMApplicationRegistry, used by manager.js to process
 * remote changes received and apply them to the local registry.
 */
function AitcStorageImpl() {
  this._log = Log4Moz.repository.getLogger("Service.AITC.Storage");
  this._log.level = Log4Moz.Level[Preferences.get(
    "services.aitc.storage.log.level"
  )];
  this._log.info("Loading AitC storage module");
}
AitcStorageImpl.prototype = {
  /**
   * Determines what changes are to be made locally, given a list of
   * remote apps.
   *
   * @param remoteApps
   *        (Array)     An array of app records fetched from the AITC server.
   *
   * @param callback
   *        (function)  A callback to be invoked when processing is finished.
   */
  processApps: function processApps(remoteApps, callback) {
    let self = this;
    this._log.info("Server check got " + remoteApps.length + " apps");

    // Get the set of local apps, and then pass to _processApps.
    // _processApps will check for the validity of remoteApps.
    DOMApplicationRegistry.getAllWithoutManifests(
      function _processAppsGotLocalApps(localApps) {
        let changes = self._determineLocalChanges(localApps, remoteApps);
        self._processChanges(changes, callback);
      }
    );
  },

  /**
   * Determine the set of changes needed to reconcile local with remote data.
   *
   * The return value is a mapping describing the actions that need to be
   * performed. It has the following keys:
   *
   *   deleteIDs
   *     (Array) String app IDs of applications that need to be uninstalled.
   *   installs
   *     (Object) Maps app ID to the remote app record. The app ID may exist
   *       locally. If the app did not exist previously, a new ID will be
   *       generated and used here.
   *
   * @param localApps
   *        (Object) Mapping of App ID to minimal application record (from
   *        DOMApplicationRegistry.getAllWithoutManifests()).
   * @param remoteApps
   *        (Array) Application records from the server.
   */
  _determineLocalChanges: function _determineChanges(localApps, remoteApps) {
    let changes = new Map();
    changes.deleteIDs = [];
    changes.installs  = {};

    // If there are no remote apps, do nothing.
    //
    // Arguably, the correct thing to do is to delete all local apps. The
    // server is the authoritative source, after all. But, we play it safe.
    if (!Object.keys(remoteApps).length) {
      this._log.warn("Empty set of remote apps. Not taking any action.");
      return changes;
    }

    // This is all to avoid potential duplicates. Once JS Sets are iterable, we
    // should switch everything to use them.
    let deletes       = {};
    let remoteOrigins = {};

    let localOrigins = {};
    for (let [id, app] in Iterator(localApps)) {
      localOrigins[app.origin] = id;
    }

    for (let remoteApp of remoteApps) {
      let origin = remoteApp.origin;
      remoteOrigins[origin] = true;

      // If the app is hidden on the remote server, that translates to a local
      // delete/uninstall, but only if the app is present locally.
      if (remoteApp.hidden) {
        if (origin in localOrigins) {
          deletes[localOrigins[origin]] = true;
        }

        continue;
      }

      // If the remote app isn't present locally, we install it under a
      // newly-generated ID.
      if (!localApps[origin]) {
        changes.installs[DOMApplicationRegistry.makeAppId()] = remoteApp;
        continue;
      }

      // If the remote app is newer, we force a re-install using the existing
      // ID.
      if (localApps[origin].installTime < remoteApp.installTime) {
        changes.installs[localApps[origin]] = remoteApp;
        continue;
      }
    }

    // If we have local apps not on the server, we need to delete them, as the
    // server is authoritative.
    for (let [id, app] in Iterator(localApps)) {
      if (!(app.origin in remoteOrigins)) {
        deletes[id] = true;
      }
    }

    changes.deleteIDs = Object.keys(deletes);

    return changes;
  },

  /**
   * Process changes so local client is in sync with server.
   *
   * This takes the output from _determineLocalChanges() and applies it.
   *
   * The supplied callback is invoked with no arguments when all operations
   * have completed.
   */
  _processChanges: function _processChanges(changes, cb) {

    if (!changes.deleteIDs.length && !Object.keys(changes.installs).length) {
      this._log.info("No changes to be applied.");
      cb();
      return;
    }

    // First, we assemble all the changes in the right format.
    let installs = [];
    for (let [id, record] in Iterator(changes.installs)) {
      installs.push({id: id, value: record});
    }

    let uninstalls = [];
    for (let id of changes.deleteIDs) {
      this._log.info("Uninstalling app: " + id);
      uninstalls.push({id: id, hidden: true});
    }

    // Now we need to perform actions.
    //
    // We want to perform all the uninstalls followed by all the installs.
    // However, this is somewhat complicated because uninstalls are
    // asynchronous and there may not even be any uninstalls. So, we simply
    // define a clojure to invoke installation and we call it whenever we're
    // ready.
    let doInstalls = function doInstalls() {
      if (!installs.length) {
        if (cb) {
          try {
            cb();
          } catch (ex) {
            this._log.warn("Exception when invoking callback: " +
                           CommonUtils.exceptionStr(ex));
          } finally {
            cb = null;
          }
        }
        return;
      }

      this._applyInstalls(installs, cb);

      // Prevent double invoke, just in case.
      installs = [];
      cb       = null;
    }.bind(this);

    if (uninstalls.length) {
      DOMApplicationRegistry.updateApps(uninstalls, function onComplete() {
        doInstalls();
        return;
      });
    } else {
      doInstalls();
    }
  },

  /**
   * Apply a set of installs to the local registry. Fetch each app's manifest
   * in parallel (don't retry on failure) and insert into registry.
   */
  _applyInstalls: function _applyInstalls(toInstall, callback) {
    let done = 0;
    let total = toInstall.length;
    this._log.info("Applying " + total + " installs to registry");

    /**
     * The purpose of _checkIfDone is to invoke the callback after all the
     * installs have been applied. They all fire in parallel, and each will
     * check-in when it is done.
     */
    let self = this;
    function _checkIfDone() {
      done += 1;
      self._log.debug(done + "/" + total + " apps processed");
      if (done == total) {
        callback();
      }
    }

    function _makeManifestCallback(appObj) {
      return function(err, manifest) {
        if (err) {
          self._log.warn("Could not fetch manifest for " + appObj.name);
          _checkIfDone();
          return;
        }
        appObj.value.manifest = manifest;
        DOMApplicationRegistry.updateApps([appObj], _checkIfDone);
      }
    }

    /**
     * Now we get a manifest for each record, and add it to the local registry
     * when we receive it. If a manifest GET times out, we will not add
     * the app to the registry but count as "success" anyway. The app will
     * be added on the next GET poll, hopefully the manifest will be
     * available then.
     */
    for each (let app in toInstall) {
      let url = app.value.manifestURL;
      if (url[0] == "/") {
        url = app.value.origin + app.value.manifestURL;
      }
      this._getManifest(url, _makeManifestCallback(app));
    }
  },

  /**
   * Fetch a manifest from given URL. No retries are made on failure. We'll
   * timeout after 20 seconds.
   */
  _getManifest: function _getManifest(url, callback) {
    let req = new RESTRequest(url);
    req.timeout = 20;

    let self = this;
    req.get(function(error) {
      if (error) {
        callback(error, null);
        return;
      }
      if (!req.response.success) {
        callback(new Error("Non-200 while fetching manifest"), null);
        return;
      }

      let err = null;
      let manifest = null;
      try {
        manifest = JSON.parse(req.response.body);
        if (!manifest.name) {
          self._log.warn(
            "_getManifest got invalid manifest: " + req.response.body
          );
          err = new Error("Invalid manifest fetched");
        }
      } catch (e) {
        self._log.warn(
          "_getManifest got invalid JSON response: " + req.response.body
        );
        err = new Error("Invalid manifest fetched");
      }

      callback(err, manifest);
    });
  },

};

XPCOMUtils.defineLazyGetter(this, "AitcStorage", function() {
  return new AitcStorageImpl();
});
