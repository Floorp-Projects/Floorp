/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.local

import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation
import mozilla.components.service.digitalassetlinks.RelationChecker
import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.service.digitalassetlinks.StatementLister

class StatementRelationChecker(
    private val lister: StatementLister
) : RelationChecker {

    override fun checkDigitalAssetLinkRelationship(source: AssetDescriptor.Web, relation: Relation, target: AssetDescriptor): Boolean {
        val statements = lister.listDigitalAssetLinkStatements(source)
        return checkLink(statements, relation, target)
    }

    companion object {

        /**
         * Check if any of the given [Statement]s are linked to the given [target].
         */
        fun checkLink(statements: List<Statement>, relation: Relation, target: AssetDescriptor) =
            statements.any { statement ->
                relation in statement.relation && statement.target == target
            }
    }
}
