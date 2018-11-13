/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import mozilla.components.service.glean.Lifetime
import mozilla.components.support.base.log.logger.Logger

/**
 * This singleton handles the in-memory storage logic for strings. It is meant to be used by
 * the Specific Strings API and the ping assembling objects. No validation on the stored data
 * is performed at this point: validation must be performed by the Specific Strings API.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object StringsStorageEngine : StringsStorageEngineImplementation()

open class StringsStorageEngineImplementation(
    override val logger: Logger = Logger("glean/StringsStorageEngine")
) : GenericScalarStorageEngine<String>() {

    override fun singleMetricDeserializer(value: Any?): String? {
        return value as? String
    }

    /**
     * Record a string in the desired stores.
     *
     * @param stores the list of stores to record the string into
     * @param category the category of the string
     * @param name the name of the string
     * @param lifetime the lifetime of the stored metric data
     * @param value the string value to record
     */
    @Synchronized
    fun record(
        stores: List<String>,
        category: String,
        name: String,
        lifetime: Lifetime,
        value: String
    ) {
        super.recordScalar(stores, category, name, lifetime, value)
    }
}
