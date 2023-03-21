/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This testing component is used in test_vacuum* tests.

const CAT_NAME = "vacuum-participant";
const CONTRACT_ID = "@unit.test.com/test-vacuum-participant;1";

import { MockRegistrar } from "resource://testing-common/MockRegistrar.sys.mjs";
import { TestUtils } from "resource://testing-common/TestUtils.sys.mjs";

export class VacuumParticipant {
  #dbConn;
  #expectedPageSize = 0;
  #useIncrementalVacuum = false;
  #grant = false;

  /**
   * Build a VacuumParticipant instance.
   * Note: After creation you must await instance.promiseRegistered() to ensure
   * Category Caches have been updated.
   *
   * @param {mozIStorageAsyncConnection} databaseConnection
   *   The connection to be vacuumed.
   * @param {Number} [expectedPageSize]
   *   Used to change the database page size.
   * @param {boolean} [useIncrementalVacuum]
   *   Whether to enable incremental vacuum on the database.
   * @param {boolean} [grant]
   *   Whether the vacuum operation should be granted.
   */
  constructor(
    databaseConnection,
    { expectedPageSize = 0, useIncrementalVacuum = false, grant = true } = {}
  ) {
    this.#dbConn = databaseConnection;

    // Register as the only participant.
    this.#unregisterAllParticipants();
    this.#registerAsParticipant();

    this.#expectedPageSize = expectedPageSize;
    this.#useIncrementalVacuum = useIncrementalVacuum;
    this.#grant = grant;

    this.QueryInterface = ChromeUtils.generateQI([
      "mozIStorageVacuumParticipant",
    ]);
  }

  promiseRegistered() {
    // The category manager dispatches change notifications to the main thread,
    // so we must wait one tick.
    return TestUtils.waitForTick();
  }

  #registerAsParticipant() {
    MockRegistrar.register(CONTRACT_ID, this);
    Services.catMan.addCategoryEntry(
      CAT_NAME,
      "vacuumParticipant",
      CONTRACT_ID,
      false,
      false
    );
  }

  #unregisterAllParticipants() {
    // First unregister other participants.
    for (let { data: entry } of Services.catMan.enumerateCategory(CAT_NAME)) {
      Services.catMan.deleteCategoryEntry("vacuum-participant", entry, false);
    }
  }

  async dispose() {
    this.#unregisterAllParticipants();
    MockRegistrar.unregister(CONTRACT_ID);
    await new Promise(resolve => {
      this.#dbConn.asyncClose(resolve);
    });
  }

  get expectedDatabasePageSize() {
    return this.#expectedPageSize;
  }

  get useIncrementalVacuum() {
    return this.#useIncrementalVacuum;
  }

  get databaseConnection() {
    return this.#dbConn;
  }

  onBeginVacuum() {
    if (!this.#grant) {
      return false;
    }
    Services.obs.notifyObservers(null, "test-begin-vacuum");
    return true;
  }
  onEndVacuum(succeeded) {
    Services.obs.notifyObservers(
      null,
      succeeded ? "test-end-vacuum-success" : "test-end-vacuum-failure"
    );
  }
}
