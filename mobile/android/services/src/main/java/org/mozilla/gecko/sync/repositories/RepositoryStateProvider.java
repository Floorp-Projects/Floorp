/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import android.support.annotation.CheckResult;
import android.support.annotation.Nullable;

/**
 * Interface describing a repository state provider.
 * Repository's state might consist of a number of key-value pairs.
 *
 * Currently there are two types of implementations: persistent and non-persistent state.
 * Persistent state survives between syncs, and is currently used by the BatchingDownloader to
 * resume downloads in case of interruptions. Non-persistent state is used when resuming downloads
 * is not possible.
 *
 * In order to safely use a persistent state provider for resuming downloads, a sync stage must match
 * the following criteria:
 * - records are downloaded with sort=oldest
 * - records must be downloaded into a persistent buffer, or applied to live storage
 *
 * @author grisha
 */
public interface RepositoryStateProvider {
    String KEY_HIGH_WATER_MARK = "highWaterMark";
    String KEY_OFFSET = "offset";
    String KEY_OFFSET_SINCE = "offsetSince";
    String KEY_OFFSET_ORDER = "offsetOrder";

    boolean isPersistent();

    @CheckResult
    boolean commit();

    RepositoryStateProvider clear(String key);

    RepositoryStateProvider setString(String key, String value);
    @Nullable String getString(String key);

    RepositoryStateProvider setLong(String key, Long value);
    @Nullable Long getLong(String key);

    @CheckResult
    boolean resetAndCommit();
}
