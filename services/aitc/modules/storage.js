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
        self._processApps(remoteApps, localApps, callback);
      }
    );
  },

  /**
   * Take a list of remote and local apps and figured out what changes (if any)
   * are to be made to the local DOMApplicationRegistry.
   *
   * General algorithm:
   *  1. Put all remote apps in a dictionary of origin->app.
   *  2. Put all local apps in a dictionary of origin->app.
   *  3. Mark all local apps as "to be deleted".
   *  4. Go through each remote app:
   *    4a. If remote app is not marked as hidden, remove from the "to be
   *        deleted" set.
   *    4b. If remote app is marked as hidden, but isn't present locally,
   *        process the next remote app.
   *    4c. If remote app is not marked as hidden and isn't present locally,
   *        add to the "to be installed" set.
   *  5. For each app either in the "to be installed" or "to be deleted" set,
   *     apply the changes locally. For apps to be installed, we must also
   *     fetch the manifest.
   *
   */
  _processApps: function _processApps(remoteApps, lApps, callback) {
    let toDelete = {};
    let localApps = {};

    // If remoteApps is empty, do nothing. The correct thing to do is to
    // delete all local apps, but we'll play it safe for now since we are
    // marking apps as deleted anyway. In a subsequent version (when the
    // hidden flag is no longer in use), this check can be removed.
    if (!Object.keys(remoteApps).length) {
      this._log.warn("Empty set of remote apps to _processApps, returning");
      callback();
      return;
    }

    // Convert lApps to a dictionary of origin -> app (instead of id -> app).
    for (let [id, app] in Iterator(lApps)) {
      app.id = id;
      toDelete[app.origin] = app;
      localApps[app.origin] = app;
    }

    // Iterate over remote apps, and find out what changes we must apply.
    let toInstall = [];
    for each (let app in remoteApps) {
      // Don't delete apps that are both local & remote.
      let origin = app.origin;
      if (!app.hidden) {
        delete toDelete[origin];
      }

      // A remote app that was hidden, but also isn't present locally is NOP.
      if (app.hidden && !localApps[origin]) {
        continue;
      }

      // If there is a remote app that isn't local or if the remote app was
      // installed or updated later.
      let id;
      if (!localApps[origin]) {
        id = DOMApplicationRegistry.makeAppId();
      }
      if (localApps[origin] &&
          (localApps[origin].installTime < app.installTime)) {
        id = localApps[origin].id;
      }

      // We should (re)install this app locally
      if (id) {
        toInstall.push({id: id, value: app});
      }
    }

    // Uninstalls only need the ID & hidden flag.
    let toUninstall = [];
    for (let origin in toDelete) {
      toUninstall.push({id: toDelete[origin].id, hidden: true});
    }

    // Apply uninstalls first, we do not need to fetch manifests.
    if (toUninstall.length) {
      this._log.info("Applying uninstalls to registry");

      let self = this;
      DOMApplicationRegistry.updateApps(toUninstall, function() {
        // If there are installs, proceed to apply each on in parallel. 
        if (toInstall.length) {
          self._applyInstalls(toInstall, callback);
          return;
        }
        callback();
      });

      return;
    }

    // If there were no uninstalls, only apply installs
    if (toInstall.length) {
      this._applyInstalls(toInstall, callback);
      return;
    }

    this._log.info("There were no changes to be applied, returning");
    callback();
    return;
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
