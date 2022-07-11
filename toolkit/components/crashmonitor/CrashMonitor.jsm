/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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

var EXPORTED_SYMBOLS = ["CrashMonitor"];

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);

const SESSIONSTORE_WINDOWS_RESTORED_TOPIC = "sessionstore-windows-restored";
const SESSIONSTORE_FINAL_STATE_WRITE_COMPLETE_TOPIC =
  "sessionstore-final-state-write-complete";

const NOTIFICATIONS = [
  "final-ui-startup",
  SESSIONSTORE_WINDOWS_RESTORED_TOPIC,
  "quit-application-granted",
  "quit-application",
  "profile-change-net-teardown",
  "profile-change-teardown",
  SESSIONSTORE_FINAL_STATE_WRITE_COMPLETE_TOPIC,
];

const SHUTDOWN_PHASES = ["profile-before-change"];

var CrashMonitorInternal = {
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
   * A deferred promise that resolves when all checkpoints have been written.
   */
  sessionStoreFinalWriteComplete: PromiseUtils.defer(),

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

  /**
   * Path to checkpoint file.
   *
   * Each time a new notification is received, this file is written to
   * disc to reflect the information in |checkpoints|.
   */
  path: PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "sessionCheckpoints.json"
  ),

  /**
   * Load checkpoints from previous session asynchronously.
   *
   * @return {Promise} A promise that resolves/rejects once loading is complete
   */
  loadPreviousCheckpoints() {
    this.previousCheckpoints = (async function() {
      let notifications;
      try {
        notifications = await IOUtils.readJSON(CrashMonitorInternal.path);
      } catch (ex) {
        // Ignore file not found errors, but report all others.
        if (ex.name !== "NotFoundError") {
          Cu.reportError(
            `Error while loading crash monitor data: ${ex.message}`
          );
        }
        return null;
      }

      // If `notifications` isn't an object, then the monitor data isn't valid.
      if (Object(notifications) !== notifications) {
        Cu.reportError(
          "Error while parsing crash monitor data: invalid monitor data"
        );
        return null;
      }

      return Object.freeze(notifications);
    })();

    return this.previousCheckpoints;
  },
};

var CrashMonitor = {
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
      throw new Error(
        "CrashMonitor must be initialized before getting previous checkpoints"
      );
    }

    return CrashMonitorInternal.previousCheckpoints;
  },

  /**
   * Initialize CrashMonitor.
   *
   * Should only be called from the CrashMonitor XPCOM component.
   *
   * @return {Promise}
   */
  init() {
    if (CrashMonitorInternal.initialized) {
      throw new Error("CrashMonitor.init() must only be called once!");
    }

    let promise = CrashMonitorInternal.loadPreviousCheckpoints();
    // Add "profile-after-change" to checkpoint as this method is
    // called after receiving it
    CrashMonitorInternal.checkpoints["profile-after-change"] = true;

    NOTIFICATIONS.forEach(function(aTopic) {
      Services.obs.addObserver(this, aTopic);
    }, this);

    // Add shutdown blocker for profile-before-change
    IOUtils.profileBeforeChange.addBlocker(
      "CrashMonitor: Writing notifications to file after receiving profile-before-change and awaiting all checkpoints written",
      async () => {
        await this.writeCheckpoint("profile-before-change");

        // If SessionStore has not initialized, we don't want to wait for
        // checkpoints that we won't hit, or we'll crash the browser during
        // async shutdown.
        if (
          !PrivateBrowsingUtils.permanentPrivateBrowsing &&
          CrashMonitorInternal.checkpoints[SESSIONSTORE_WINDOWS_RESTORED_TOPIC]
        ) {
          await CrashMonitorInternal.sessionStoreFinalWriteComplete.promise;
        }
      },
      () => CrashMonitorInternal.checkpoints
    );

    CrashMonitorInternal.initialized = true;
    return promise;
  },

  /**
   * Handle registered notifications.
   *
   * Update checkpoint file for every new notification received.
   */
  observe(aSubject, aTopic, aData) {
    this.writeCheckpoint(aTopic);

    if (
      NOTIFICATIONS.every(elem => elem in CrashMonitorInternal.checkpoints) &&
      SHUTDOWN_PHASES.every(elem => elem in CrashMonitorInternal.checkpoints)
    ) {
      // All notifications received, unregister observers
      NOTIFICATIONS.forEach(function(aTopic) {
        Services.obs.removeObserver(this, aTopic);
      }, this);
    }

    if (aTopic === SESSIONSTORE_FINAL_STATE_WRITE_COMPLETE_TOPIC) {
      CrashMonitorInternal.sessionStoreFinalWriteComplete.resolve();
    }
  },

  async writeCheckpoint(aCheckpoint) {
    if (!(aCheckpoint in CrashMonitorInternal.checkpoints)) {
      // If this is the first time this notification is received,
      // remember it and write it to file
      CrashMonitorInternal.checkpoints[aCheckpoint] = true;

      /* Write to the checkpoint file asynchronously, off the main
       * thread, for performance reasons. Note that this means
       * that there's not a 100% guarantee that the file will be
       * written by the time the notification completes. The
       * exception is profile-before-change which has a shutdown
       * blocker. */
      await IOUtils.writeJSON(
        CrashMonitorInternal.path,
        CrashMonitorInternal.checkpoints,
        {
          tmpPath: CrashMonitorInternal.path + ".tmp",
        }
      );
    }
  },
};
Object.freeze(CrashMonitor);
