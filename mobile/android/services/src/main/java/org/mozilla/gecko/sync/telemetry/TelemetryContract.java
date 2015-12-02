/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.telemetry;

public class TelemetryContract {
  /**
   * We are a Sync 1.1 (legacy) client, and we downloaded a migration sentinel.
   */
  public static final String SYNC11_MIGRATION_SENTINELS_SEEN = "FENNEC_SYNC11_MIGRATION_SENTINELS_SEEN";

  /**
   * We are a Sync 1.1 (legacy) client and we have downloaded a migration
   * sentinel, but there was an error creating a Firefox Account from that
   * sentinel.
   * <p>
   * We have logged the error and are ignoring that sentinel.
   */
  public static final String SYNC11_MIGRATIONS_FAILED = "FENNEC_SYNC11_MIGRATIONS_FAILED";

  /**
   * We are a Sync 1.1 (legacy) client and we have downloaded a migration
   * sentinel, and there was no reported error creating a Firefox Account from
   * that sentinel.
   * <p>
   * We have created a Firefox Account corresponding to the sentinel and have
   * queued the existing Old Sync account for removal.
   */
  public static final String SYNC11_MIGRATIONS_SUCCEEDED = "FENNEC_SYNC11_MIGRATIONS_SUCCEEDED";

  /**
   * We are (now) a Sync 1.5 (Firefox Accounts-based) client that migrated from
   * Sync 1.1. We have presented the user the "complete upgrade" notification.
   * <p>
   * We will offer every time a sync is triggered, including when a notification
   * is already pending.
   */
  public static final String SYNC11_MIGRATION_NOTIFICATIONS_OFFERED = "FENNEC_SYNC11_MIGRATION_NOTIFICATIONS_OFFERED";

  /**
   * We are (now) a Sync 1.5 (Firefox Accounts-based) client that migrated from
   * Sync 1.1. We have presented the user the "complete upgrade" notification
   * and they have successfully completed the upgrade process by entering their
   * Firefox Account credentials.
   */
  public static final String SYNC11_MIGRATIONS_COMPLETED = "FENNEC_SYNC11_MIGRATIONS_COMPLETED";

  public static final String SYNC_STARTED = "FENNEC_SYNC_NUMBER_OF_SYNCS_STARTED";

  public static final String SYNC_COMPLETED = "FENNEC_SYNC_NUMBER_OF_SYNCS_COMPLETED";

  public static final String SYNC_FAILED = "FENNEC_SYNC_NUMBER_OF_SYNCS_FAILED";

  public static final String SYNC_FAILED_BACKOFF = "FENNEC_SYNC_NUMBER_OF_SYNCS_FAILED_BACKOFF";
}
