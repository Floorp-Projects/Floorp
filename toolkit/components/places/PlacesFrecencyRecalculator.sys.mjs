/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component handles frecency recalculations and decay on idle.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", function() {
  return lazy.PlacesUtils.getLogger({ prefix: "FrecencyRecalculator" });
});

// Decay rate applied daily to frecency scores.
// A scaling factor of .975 results in an half-life of 28 days.
const FRECENCY_DECAYRATE = "0.975";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "frecencyDecayRate",
  "places.frecency.decayRate",
  FRECENCY_DECAYRATE,
  null,
  val => {
    if (typeof val == "string") {
      val = parseFloat(val);
    }
    if (val > 1.0) {
      lazy.logger.error("Invalid frecency decay rate value: " + val);
      val = parseFloat(FRECENCY_DECAYRATE);
    }
    return val;
  }
);

// An adaptive history entry is removed if unused for these many days.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "adaptiveHistoryExpireDays",
  "places.adaptiveHistory.expireDays",
  90
);

// Time between deferred task executions.
const DEFERRED_TASK_INTERVAL_MS = 2 * 60000;
// Maximum time to wait for an idle before the task is executed anyway.
const DEFERRED_TASK_MAX_IDLE_WAIT_MS = 5 * 60000;
// Number of entries to update at once.
const DEFAULT_CHUNK_SIZE = 50;

export class PlacesFrecencyRecalculator {
  classID = Components.ID("1141fd31-4c1a-48eb-8f1a-2f05fad94085");

  /**
   * A DeferredTask that runs our tasks.
   */
  #task = null;

  constructor() {
    lazy.logger.trace("Initializing Frecency Recalculator");

    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);

    // Do not initialize during shutdown.
    if (
      Services.startup.isInOrBeyondShutdownPhase(
        Ci.nsIAppStartup.SHUTDOWN_PHASE_APPSHUTDOWNTEARDOWN
      )
    ) {
      return;
    }

    this.#task = new lazy.DeferredTask(
      this.#taskFn.bind(this),
      DEFERRED_TASK_INTERVAL_MS,
      DEFERRED_TASK_MAX_IDLE_WAIT_MS
    );
    lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
      "PlacesFrecencyRecalculator: shutdown",
      () => this.#finalize()
    );

    // The public methods and properties are intended to be used by tests, and
    // are exposed through the raw js object. Since this is expected to work
    // based on signals or notification, it should not be necessary to expose
    // any API for the product, though if that would become necessary in the
    // future, we could add an interface for the service.
    this.wrappedJSObject = this;
    // This can be used by tests to await for the decay process.
    this.pendingFrecencyDecayPromise = Promise.resolve();

    Services.obs.addObserver(this, "idle-daily", true);
    Services.obs.addObserver(this, "frecency-recalculation-needed", true);

    // Run once on startup, so we pick up any leftover work.
    this.#task.arm();
  }

  async #taskFn() {
    if (this.#task.isFinalized) {
      return;
    }
    const refObj = {};
    const histogram = "PLACES_FRECENCY_RECALC_CHUNK_TIME_MS";
    TelemetryStopwatch.start(histogram, refObj);
    try {
      if (await this.recalculateSomeFrecencies()) {
        TelemetryStopwatch.finish(histogram, refObj);
      } else {
        TelemetryStopwatch.cancel(histogram, refObj);
      }
    } catch (ex) {
      TelemetryStopwatch.cancel(histogram, refObj);
      console.error(ex);
      lazy.logger.error(ex);
    }
  }

  #finalize() {
    lazy.logger.trace("Finalizing frecency recalculator");
    // We don't mind about tasks completiion, since we can execute them in the
    // next session.
    this.#task.disarm();
    this.#task.finalize().catch(console.error);
  }

  /**
   * Updates a chunk of outdated frecency values. If there's more frecency
   * values to update at the end of the process, it may rearm the task.
   * @param {Number} chunkSize maximum number of entries to update at a time,
   *   set to -1 to update any entry.
   * @resolves {Number} Number of affected pages.
   */
  async recalculateSomeFrecencies({ chunkSize = DEFAULT_CHUNK_SIZE } = {}) {
    lazy.logger.trace(`Recalculate ${chunkSize} frecency values`);
    let affected;
    let db = await lazy.PlacesUtils.promiseUnsafeWritableDBConnection();
    await db.executeTransaction(async function() {
      affected = await db.executeCached(
        `UPDATE moz_places
        SET frecency = CALCULATE_FRECENCY(id)
        WHERE id IN (
          SELECT id FROM moz_places
          WHERE recalc_frecency = 1
          ORDER BY frecency DESC
          LIMIT ${chunkSize}
        )
        RETURNING id`
      );

      // We've finished recalculating frecencies. Trigger frecency updates for
      // any affected origin.
      await db.executeCached(`DELETE FROM moz_updateoriginsupdate_temp`);
    });

    if (affected.length) {
      PlacesObservers.notifyListeners([new PlacesRanking()]);
    }

    if (affected.length == chunkSize) {
      // There's more entries to recalculate, rearm the task.
      this.#task.arm();
    } else {
      // There's nothing left to recalculate, wait for the next change.
      lazy.PlacesUtils.history.shouldStartFrecencyRecalculation = false;
      this.#task.disarm();
    }
    return affected.length;
  }

  /**
   * Forces full recalculation of any outdated frecency values.
   * This exists for testing purposes; in tests we don't want to wait for
   * the deferred task to run, this can enforce a recalculation.
   */
  async recalculateAnyOutdatedFrecencies() {
    this.#task.disarm();
    return this.recalculateSomeFrecencies({ chunkSize: -1 });
  }

  /**
   * Whether a recalculation task is pending.
   */
  get isRecalculationPending() {
    return this.#task.isArmed;
  }

  /**
   * Invoked periodically to eventually start a recalculation task.
   */
  maybeStartFrecencyRecalculation() {
    if (
      lazy.PlacesUtils.history.shouldStartFrecencyRecalculation &&
      !this.#task.isFinalized
    ) {
      lazy.logger.trace("Arm frecency recalculation");
      this.#task.arm();
    }
  }

  /**
   * Decays frecency and adaptive history.
   * @resolves once the process is complete. Never rejects.
   */
  async decay() {
    lazy.logger.trace("Decay frecency");
    let refObj = {};
    TelemetryStopwatch.start("PLACES_IDLE_FRECENCY_DECAY_TIME_MS", refObj);
    // Ensure moz_places_afterupdate_frecency_trigger ignores decaying
    // frecency changes.
    lazy.PlacesUtils.history.isFrecencyDecaying = true;
    try {
      let db = await lazy.PlacesUtils.promiseUnsafeWritableDBConnection();
      await db.executeTransaction(async function() {
        // Decay all frecency rankings to reduce value of pages that haven't
        // been visited in a while.
        await db.executeCached(
          `UPDATE moz_places SET frecency = ROUND(frecency * :decay_rate)
            WHERE frecency > 0 AND recalc_frecency = 0`,
          { decay_rate: lazy.frecencyDecayRate }
        );
        // Decay potentially unused adaptive entries (e.g. those that are at 1)
        // to allow better chances for new entries that will start at 1.
        await db.executeCached(
          `UPDATE moz_inputhistory SET use_count = use_count * :decay_rate`,
          { decay_rate: lazy.frecencyDecayRate }
        );
        // Delete any adaptive entries that won't help in ordering anymore.
        await db.executeCached(
          `DELETE FROM moz_inputhistory WHERE use_count < :use_count`,
          {
            use_count: Math.pow(
              lazy.frecencyDecayRate,
              lazy.adaptiveHistoryExpireDays
            ),
          }
        );
        // We've finished decaying frecencies. Trigger frecency updates for
        // any affected origin.
        await db.executeCached(`DELETE FROM moz_updateoriginsupdate_temp`);

        TelemetryStopwatch.finish("PLACES_IDLE_FRECENCY_DECAY_TIME_MS", refObj);
        PlacesObservers.notifyListeners([new PlacesRanking()]);
      });
    } catch (ex) {
      TelemetryStopwatch.cancel("PLACES_IDLE_FRECENCY_DECAY_TIME_MS", refObj);
      console.error(ex);
      lazy.logger.error(ex);
    } finally {
      lazy.PlacesUtils.history.isFrecencyDecaying = false;
    }
  }

  observe(subject, topic, data) {
    lazy.logger.trace(`Got ${topic} topic`);
    switch (topic) {
      case "idle-daily":
        this.pendingFrecencyDecayPromise = this.decay();
        // Also recalculate frecencies.
        this.#task.arm();
        return;
      case "frecency-recalculation-needed":
        lazy.logger.trace("Frecency recalculation requested");
        this.maybeStartFrecencyRecalculation();
        return;
      case "test-execute-taskFn":
        subject.promise = this.#taskFn();
        return;
      default:
        console.error(`Unknown "${topic}" topic`);
    }
  }
}
