/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks

/**
 * Lists all statements made by a given source.
 */
interface StatementListFetcher {

    /**
     * Retrieves a list of all statements from a given [source].
     */
    fun listStatements(source: AssetDescriptor.Web): Sequence<Statement>
}
