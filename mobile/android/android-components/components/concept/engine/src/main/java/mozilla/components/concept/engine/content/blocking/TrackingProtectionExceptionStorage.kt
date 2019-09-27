/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.content.blocking

import mozilla.components.concept.engine.EngineSession

/**
 * A contract that define how a tracking protection storage must behave.
 */
interface TrackingProtectionExceptionStorage {

    /**
     * Fetch all domains that will be ignored for tracking protection.
     * @param onResult A callback to inform that the domains in the exception list has been fetched,
     * it provides a list of all the domains that are on the exception list, if there are none
     * domains in the exception list, an empty list will be provided.
     */
    fun fetchAll(onResult: (List<String>) -> Unit)

    /**
     * Adds a new [session] to the exception list.
     * @param session The [session] that will be added to the exception list.
     */
    fun add(session: EngineSession)

    /**
     * Removes a [session] from the exception list.
     * @param session The [session] that will be removed from the exception list.
     */
    fun remove(session: EngineSession)

    /**
     * Indicates if a given [session] is in the exception list.
     * @param session The [session] to be verified.
     * @param onResult A callback to inform if the given [session] is in
     * the exception list, true if it is in, otherwise false.
     */
    fun contains(session: EngineSession, onResult: (Boolean) -> Unit)

    /**
     * Removes all domains from the exception list.
     */
    fun removeAll()

    /**
     * Restore all domains stored in the storage.
     */
    fun restore()
}
