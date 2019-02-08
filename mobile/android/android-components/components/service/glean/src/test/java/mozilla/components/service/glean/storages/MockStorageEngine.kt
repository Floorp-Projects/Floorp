/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import org.junit.Assert

/**
 * A mock storage engine. This merely returns some sample
 * data as a JSONObject or JSONArray.
 */
internal class MockStorageEngine(
    // The following needs to be Any as we expect to
    // return both JSONObject and JSONArray
    private val sampleJSON: Any,
    private val sampleStore: String = "test"
) : StorageEngine {
    override fun clearAllStores() {
        // Nothing to do here for a mocked storage engine
    }

    override lateinit var applicationContext: Context

    override fun getSnapshotAsJSON(storeName: String, clearStore: Boolean): Any? {
        Assert.assertTrue(clearStore)
        return if (storeName == sampleStore) sampleJSON else null
    }
}
