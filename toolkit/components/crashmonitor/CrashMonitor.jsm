/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Crash Monitor
 *
 * Monitors execution of a program to detect possible crashes. After
 * program termination, the monitor can be queried during the next run
 * to determine whether the last run exited cleanly or not.
 *
 * The monitoring is done by registering and listening for special
 * notifications, or checkpoints, known to be sent by the monitored
 * program as different stages in the execution are reached. As they
 * are observed, these notifications are written asynchronously to a
 * checkpoint file.
 *
 * During next program startup the crash monitor reads the checkpoint
 * file from the last session. If notifications are missing, a crash
 * has likely happened. By inspecting the notifications present, it is
 * possible to determine what stages were reached in the program
 * before the crash.
 *
 * Note that since the file is written asynchronously it is possible
 * that a received notification is lost if the program crashes right
 * after a checkpoint, but before crash monitor has been able to write
 * it to disk. Thus, while the presence of a notification in the
 * checkpoint file tells us that the corresponding stage was reached
 * during the last run, the absence of a notification after a crash
 * does not necessarily tell us that the checkpoint wasn't reached.
 */

this.EXPORTED_SYMBOLS = [ "CrashMonitor" ];

const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

const NOTIFICATIONS = [
  "final-ui-startup",
  "sessionstore-windows-restored",
  "quit-application-granted",
  "quit-application",
  "profile-change-net-teardown",
  "profile-change-teardown",
  "profile-before-change",
  "sessionstore-final-state-write-complete"
];

let CrashMonitorInternal = {

  /**
   * Notifications received during the current session.
   *
   * Object where a property with a value of |true| means that the
   * notification of the same name has been received at least once by
   * the CrashMonitor during this session. Notifications that have not
   * yet been received are not present as properties. |NOTIFICATIONS|
   * lists the notifications tracked by the CrashMonitor.
   */
  checkpoints: {},

  /**
   * Notifications received during previous session.
   *
   * Available after |loadPreviousCheckpoints|. Promise which resolves
   * to an object containing a set of properties, where a property
   * with a value of |true| means that the notification with the same
   * name as the property name was received at least once last
   * session.
   */
  previousCheckpoints: null,

  /* Deferred for AsyncShutdown blocker */
  profileBeforeChangeDeferred: Promise.defer(),

  /**
   * Path to checkpoint file.
   *
   * Each time a new notification is received, this file is written to
   * disc to reflect the information in |checkpoints|.
   */
  path: OS.Path.join(OS.Constants.Path.profileDir, "sessionCheckpoints.json"),

  /**
   * Load checkpoints from previous session asynchronously.
   *
   * @return {Promise} A promise that resolves/rejects once loading is complete
   */
  loadPreviousCheckpoints: function () {
    let deferred = Promise.defer();
    CrashMonitorInternal.previousCheckpoints = deferred.promise;

    let file = FileUtils.File(CrashMonitorInternal.path);
    NetUtil.asyncFetch(file, function(inputStream, status) {
      if (!Components.isSuccessCode(status)) {
        if (status != Cr.NS_ERROR_FILE_NOT_FOUND) {
          Cu.reportError("Error while loading crash monitor data: " + status);
        }

        deferred.resolve(null);
        return;
      }

      let data = NetUtil.readInputStreamToString(inputStream,
        inputStream.available(), { charset: "UTF-8" });

      let notifications = null;
      try {
        notifications = JSON.parse(data);
      } catch (ex) {
        Cu.reportError("Error while parsing crash monitor data: " + ex);
        deferred.resolve(null);
      }

      try {
        deferred.resolve(Object.freeze(notifications));
      } catch (ex) {
        // The only exception we reject from is if notifications is not
        // an object. This happens when the checkpoints file contained
        // just a numeric string.
        deferred.reject(ex);
      }
    });

    return deferred.promise;
  }
};

this.CrashMonitor = {

  /**
   * Notifications received during previous session.
   *
   * Return object containing the set of notifications received last
   * session as keys with values set to |true|.
   *
   * @return {Promise} A promise resolving to previous checkpoints
   */
  get previousCheckpoints() {
    if (!CrashMonitorInternal.initialized) {
      throw new Error("CrashMonitor must be initialized before getting previous checkpoints");
    }

    return CrashMonitorInternal.previousCheckpoints
  },

  /**
   * Initialize CrashMonitor.
   *
   * Should only be called from the CrashMonitor XPCOM component.
   *
   * @return {Promise}
   */
  init: function () {
    if (CrashMonitorInternal.initialized) {
      throw new Error("CrashMonitor.init() must only be called once!");
    }

    let promise = CrashMonitorInternal.loadPreviousCheckpoints();
    // Add "profile-after-change" to checkpoint as this method is
    // called after receiving it
    CrashMonitorInternal.checkpoints["profile-after-change"] = true;

    NOTIFICATIONS.forEach(function (aTopic) {
      Services.obs.addObserver(this, aTopic, false);
    }, this);

    // Add shutdown blocker for profile-before-change
    AsyncShutdown.profileBeforeChange.addBlocker(
      "CrashMonitor: Writing notifications to file after receiving profile-before-change",
      CrashMonitorInternal.profileBeforeChangeDeferred.promise
    );

    CrashMonitorInternal.initialized = true;
    return promise;
  },

  /**
   * Handle registered notifications.
   *
   * Update checkpoint file for every new notification received.
   */
  observe: function (aSubject, aTopic, aData) {
    if (!(aTopic in CrashMonitorInternal.checkpoints)) {
      // If this is the first time this notification is received,
      // remember it and write it to file
      CrashMonitorInternal.checkpoints[aTopic] = true;
      Task.spawn(function() {
        try {
          let data = JSON.stringify(CrashMonitorInternal.checkpoints);

          /* Write to the checkpoint file asynchronously, off the main
           * thread, for performance reasons. Note that this means
           * that there's not a 100% guarantee that the file will be
           * written by the time the notification completes. The
           * exception is profile-before-change which has a shutdown
           * blocker. */
          yield OS.File.writeAtomic(
            CrashMonitorInternal.path,
            data, {tmpPath: CrashMonitorInternal.path + ".tmp"});

        } finally {
          // Resolve promise for blocker
          if (aTopic == "profile-before-change") {
            CrashMonitorInternal.profileBeforeChangeDeferred.resolve();
          }
        }
      });
    }

    if (NOTIFICATIONS.every(elem => elem in CrashMonitorInternal.checkpoints)) {
      // All notifications received, unregister observers
      NOTIFICATIONS.forEach(function (aTopic) {
        Services.obs.removeObserver(this, aTopic);
      }, this);
    }
  }
};
Object.freeze(this.CrashMonitor);
