/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.local

import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation
import mozilla.components.service.digitalassetlinks.RelationChecker
import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.service.digitalassetlinks.StatementListFetcher

/**
 * Checks if a matching relationship is present in a remote statement list.
 */
class StatementRelationChecker(
    private val listFetcher: StatementListFetcher,
) : RelationChecker {

    override fun checkRelationship(source: AssetDescriptor.Web, relation: Relation, target: AssetDescriptor): Boolean {
        val statements = listFetcher.listStatements(source)
        return checkLink(statements, relation, target)
    }

    companion object {

        /**
         * Check if any of the given [Statement]s are linked to the given [target].
         */
        fun checkLink(statements: Sequence<Statement>, relation: Relation, target: AssetDescriptor) =
            statements.any { statement ->
                statement.relation == relation && statement.target == target
            }
    }
}
