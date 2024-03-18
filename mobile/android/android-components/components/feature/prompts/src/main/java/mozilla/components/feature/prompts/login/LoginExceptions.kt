/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

/**
 * Interface to be implemented by a storage layer to exclude the save logins prompt from showing.
 */
interface LoginExceptions {
    /**
     * Checks if a specific origin should show a save logins prompt or if it is an exception.
     * @param origin The origin to search exceptions list for.
     */
    fun isLoginExceptionByOrigin(origin: String): Boolean

    /**
     * Adds a new origin to the exceptions list implementation.
     * @param origin The origin to add to the list of exceptions.
     */
    fun addLoginException(origin: String)
}
