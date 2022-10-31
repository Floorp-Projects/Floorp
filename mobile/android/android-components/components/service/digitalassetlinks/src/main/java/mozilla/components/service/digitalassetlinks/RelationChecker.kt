/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks

/**
 * Verifies that a source is linked to a target.
 */
interface RelationChecker {

    /**
     * Performs a check to ensure a directional relationships exists between the specified
     * [source] and [target] assets. The relationship must match the [relation] type given.
     */
    fun checkRelationship(
        source: AssetDescriptor.Web,
        relation: Relation,
        target: AssetDescriptor,
    ): Boolean
}
