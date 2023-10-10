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
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", function () {
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

  /**
   * Handler for alternative frecency.
   * This allows to manager alternative ranking algorithms to experiment with.
   */
  #alternativeFrecencyHelper = null;

  /**
   * This is useful for testing.
   */
  get alternativeFrecencyInfo() {
    return this.#alternativeFrecencyHelper?.sets;
  }

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

    this.#alternativeFrecencyHelper = new AlternativeFrecencyHelper(this);

    // Run once on startup, so we pick up any leftover work.
    lazy.PlacesUtils.history.shouldStartFrecencyRecalculation = true;
    this.maybeStartFrecencyRecalculation();
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
   * @resolves {boolean} Whether any entry was recalculated.
   */
  async recalculateSomeFrecencies({ chunkSize = DEFAULT_CHUNK_SIZE } = {}) {
    lazy.logger.trace(
      `Recalculate ${chunkSize >= 0 ? chunkSize : "infinite"} frecency values`
    );
    let affectedCount = 0;
    let hasRecalculatedAnything = false;
    let db = await lazy.PlacesUtils.promiseUnsafeWritableDBConnection();
    await db.executeTransaction(async function () {
      let affected = await db.executeCached(
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
      affectedCount += affected.length;
    });
    let shouldRestartRecalculation = affectedCount >= chunkSize;
    hasRecalculatedAnything = affectedCount > 0;
    if (hasRecalculatedAnything) {
      PlacesObservers.notifyListeners([new PlacesRanking()]);
    }

    // Also recalculate some origins frecency.
    affectedCount = await this.#recalculateSomeOriginsFrecencies({
      chunkSize,
    });
    shouldRestartRecalculation ||= affectedCount >= chunkSize;
    hasRecalculatedAnything ||= affectedCount > 0;

    // If alternative frecency is enabled, also recalculate a chunk of it.
    affectedCount =
      await this.#alternativeFrecencyHelper.recalculateSomeAlternativeFrecencies(
        { chunkSize }
      );
    shouldRestartRecalculation ||= affectedCount >= chunkSize;
    hasRecalculatedAnything ||= affectedCount > 0;

    if (chunkSize > 0 && shouldRestartRecalculation) {
      // There's more entries to recalculate, rearm the task.
      this.#task.arm();
    } else {
      // There's nothing left to recalculate, wait for the next change.
      lazy.PlacesUtils.history.shouldStartFrecencyRecalculation = false;
      this.#task.disarm();
    }
    return hasRecalculatedAnything;
  }

  async #recalculateSomeOriginsFrecencies({ chunkSize }) {
    lazy.logger.trace(`Recalculate ${chunkSize} origins frecency values`);
    let affectedCount = 0;
    let db = await lazy.PlacesUtils.promiseUnsafeWritableDBConnection();
    await db.executeTransaction(async () => {
      let affected = await db.executeCached(
        `
        UPDATE moz_origins
        SET frecency = CAST(
              (SELECT total(frecency)
               FROM moz_places h
               WHERE origin_id = moz_origins.id AND frecency > 0)
              AS INT
            ),
            recalc_frecency = 0
        WHERE id IN (
          SELECT id FROM moz_origins
          WHERE recalc_frecency = 1
          ORDER BY frecency DESC
          LIMIT ${chunkSize}
        )
        RETURNING id`
      );
      affectedCount += affected.length;

      // Calculate and store the frecency statistics used to calculate a
      // thredhold. Origins above that threshold will be considered meaningful
      // and autofilled.
      // While it may be tempting to do this only when some frecency was
      // updated, that won't catch the edge case of the moz_origins table being
      // emptied.
      let row = (
        await db.executeCached(`
        SELECT count(*), total(frecency), total(pow(frecency,2))
        FROM moz_origins
        WHERE frecency > 0
      `)
      )[0];
      await lazy.PlacesUtils.metadata.setMany(
        new Map([
          ["origin_frecency_count", row.getResultByIndex(0)],
          ["origin_frecency_sum", row.getResultByIndex(1)],
          ["origin_frecency_sum_of_squares", row.getResultByIndex(2)],
        ])
      );
    });

    return affectedCount;
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
      await db.executeTransaction(async function () {
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
        lazy.logger.trace("Frecency recalculation on idle");
        lazy.PlacesUtils.history.shouldStartFrecencyRecalculation = true;
        this.maybeStartFrecencyRecalculation();
        return;
      case "frecency-recalculation-needed":
        lazy.logger.trace("Frecency recalculation requested");
        this.maybeStartFrecencyRecalculation();
        return;
      case "test-execute-taskFn":
        subject.promise = this.#taskFn();
        return;
      case "test-alternative-frecency-init":
        this.#alternativeFrecencyHelper = new AlternativeFrecencyHelper(this);
        subject.promise =
          this.#alternativeFrecencyHelper.initializedDeferred.promise;
    }
  }
}

class AlternativeFrecencyHelper {
  initializedDeferred = lazy.PromiseUtils.defer();
  #recalculator = null;

  sets = {
    pages: {
      // This pref is only read once and used to kick-off recalculations.
      enabled: Services.prefs.getBoolPref(
        "places.frecency.pages.alternative.featureGate",
        false
      ),
      // Key used to store variables in the moz_meta table.
      metadataKey: "page_alternative_frecency",
      // The table containing frecency.
      table: "moz_places",
      // Object containing variables influencing the calculation.
      // Any change to this object will cause a full recalculation on restart.
      variables: {
        // Current version of origins alternative frecency.
        //  ! IMPORTANT: Always bump up when making changes to the algorithm.
        version: 2,
        highWeight: Services.prefs.getIntPref(
          "places.frecency.pages.alternative.highWeight",
          100
        ),
        mediumWeight: Services.prefs.getIntPref(
          "places.frecency.pages.alternative.mediumWeight",
          50
        ),
        lowWeight: Services.prefs.getIntPref(
          "places.frecency.pages.alternative.lowWeight",
          20
        ),
        halfLifeDays: Services.prefs.getIntPref(
          "places.frecency.pages.alternative.halfLifeDays",
          30
        ),
        numSampledVisits: Services.prefs.getIntPref(
          "places.frecency.pages.alternative.numSampledVisits",
          10
        ),
      },
      method: this.#recalculateSomePagesAlternativeFrecencies,
    },

    origins: {
      // This pref is only read once and used to kick-off recalculations.
      enabled: Services.prefs.getBoolPref(
        "places.frecency.origins.alternative.featureGate",
        false
      ),
      // Key used to store variables in the moz_meta table.
      metadataKey: "origin_alternative_frecency",
      // The table containing frecency.
      table: "moz_origins",
      // Object containing variables influencing the calculation.
      // Any change to this object will cause a full recalculation on restart.
      variables: {
        // Current version of origins alternative frecency.
        //  ! IMPORTANT: Always bump up when making changes to the algorithm.
        version: 2,
        // Frecencies of pages are ignored after these many days.
        daysCutOff: Services.prefs.getIntPref(
          "places.frecency.origins.alternative.daysCutOff",
          90
        ),
      },
      method: this.#recalculateSomeOriginsAlternativeFrecencies,
    },
  };

  constructor(recalculator) {
    this.#recalculator = recalculator;
    this.#kickOffAlternativeFrecencies()
      .catch(console.error)
      .finally(() => this.initializedDeferred.resolve());
  }

  async #kickOffAlternativeFrecencies() {
    let recalculateFirstChunk = false;
    for (let [type, set] of Object.entries(this.sets)) {
      // Now check the variables cached in the moz_meta table. If not found we
      // assume alternative frecency was disabled in the previous session.
      let storedVariables = await lazy.PlacesUtils.metadata.get(
        set.metadataKey,
        null
      );

      // Check whether this is the first-run, that happens when the alternative
      // ranking is enabled and it was not at the previous session, or variables
      // were changed. We should recalculate all the alternative frecency values.
      if (
        set.enabled &&
        !lazy.ObjectUtils.deepEqual(set.variables, storedVariables)
      ) {
        lazy.logger.trace(
          `Alternative frecency of ${type} must be recalculated`
        );
        await lazy.PlacesUtils.withConnectionWrapper(
          `PlacesFrecencyRecalculator :: ${type} alternative frecency set recalc`,
          async db => {
            await db.execute(`UPDATE ${set.table} SET recalc_alt_frecency = 1`);
          }
        );
        await lazy.PlacesUtils.metadata.set(set.metadataKey, set.variables);
        recalculateFirstChunk = true;
        continue;
      }

      if (!set.enabled && storedVariables) {
        lazy.logger.trace(`Clean up alternative frecency of ${type}`);
        // Clear alternative frecency to save on space.
        await lazy.PlacesUtils.withConnectionWrapper(
          `PlacesFrecencyRecalculator :: ${type} alternative frecency set NULL`,
          async db => {
            await db.execute(`UPDATE ${set.table} SET alt_frecency = NULL`);
          }
        );
        await lazy.PlacesUtils.metadata.delete(set.metadataKey);
      }
    }

    if (recalculateFirstChunk) {
      // Do a first recalculation immediately, so we don't leave the user
      // with unranked entries for too long.
      await this.recalculateSomeAlternativeFrecencies();

      // Ensure the recalculation task is armed for a second run.
      lazy.PlacesUtils.history.shouldStartFrecencyRecalculation = true;
      this.#recalculator.maybeStartFrecencyRecalculation();
    }
  }

  /**
   * Updates a chunk of outdated frecency values.
   * @param {Number} chunkSize maximum number of entries to update at a time,
   *   set to -1 to update any entry.
   * @resolves {Number} Number of affected pages.
   */
  async recalculateSomeAlternativeFrecencies({
    chunkSize = DEFAULT_CHUNK_SIZE,
  } = {}) {
    let affected = 0;
    for (let set of Object.values(this.sets)) {
      if (!set.enabled) {
        continue;
      }
      try {
        affected += await set.method({ chunkSize, variables: set.variables });
      } catch (ex) {
        console.error(ex);
      }
    }
    return affected;
  }

  async #recalculateSomePagesAlternativeFrecencies({ chunkSize, variables }) {
    lazy.logger.trace(
      `Recalculate ${chunkSize} alternative pages frecency values`
    );
    // Since it takes a long period of time to recalculate frecency of all the
    // pages, due to the high number of them, we artificially increase the
    // chunk size here.
    let db = await lazy.PlacesUtils.promiseUnsafeWritableDBConnection();
    let affected = await db.executeCached(
      `UPDATE moz_places
       SET alt_frecency = CALCULATE_ALT_FRECENCY(moz_places.id),
           recalc_alt_frecency = 0
       WHERE id IN (
        SELECT id FROM moz_places
          WHERE recalc_alt_frecency = 1
          ORDER BY frecency DESC
          LIMIT ${chunkSize * 2}
      )
      RETURNING id`
    );
    return affected;
  }

  async #recalculateSomeOriginsAlternativeFrecencies({ chunkSize, variables }) {
    lazy.logger.trace(
      `Recalculate ${chunkSize} alternative origins frecency values`
    );
    let affectedCount = 0;
    let db = await lazy.PlacesUtils.promiseUnsafeWritableDBConnection();
    await db.executeTransaction(async () => {
      let affected = await db.executeCached(
        `
        UPDATE moz_origins
        SET alt_frecency = (
          SELECT sum(frecency)
          FROM moz_places h
          WHERE origin_id = moz_origins.id
          AND last_visit_date >
            strftime('%s','now','localtime','start of day',
                     '-${variables.daysCutOff} day','utc') * 1000000
        ), recalc_alt_frecency = 0
        WHERE id IN (
          SELECT id FROM moz_origins
          WHERE recalc_alt_frecency = 1
          ORDER BY frecency DESC
          LIMIT ${chunkSize}
        )
        RETURNING id`
      );
      affectedCount += affected.length;

      // Calculate and store the alternative frecency threshold. Origins above
      // this threshold will be considered meaningful and autofilled.
      if (affected.length) {
        let threshold = (
          await db.executeCached(`SELECT avg(alt_frecency) FROM moz_origins`)
        )[0].getResultByIndex(0);
        await lazy.PlacesUtils.metadata.set(
          "origin_alt_frecency_threshold",
          threshold
        );
      }
    });

    return affectedCount;
  }
}
