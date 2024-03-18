/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

/**
 * Contract to indicate how objects with the ability to clear data should behave.
 */
interface DataCleanable {
    /**
     * Clears browsing data stored.
     *
     * @param data the type of data that should be cleared, defaults to all.
     * @param host (optional) name of the host for which data should be cleared. If
     * omitted data will be cleared for all hosts.
     * @param onSuccess (optional) callback invoked if the data was cleared successfully.
     * @param onError (optional) callback invoked if clearing the data caused an exception.
     */
    fun clearData(
        data: Engine.BrowsingData = Engine.BrowsingData.all(),
        host: String? = null,
        onSuccess: (() -> Unit) = { },
        onError: ((Throwable) -> Unit) = { },
    ): Unit = onError(UnsupportedOperationException("Clearing browsing data is not supported."))
}
