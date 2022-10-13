/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks

/**
 * Represents a statement that can be found in an assetlinks.json file.
 */
sealed class StatementResult

/**
 * Entry in a Digital Asset Links statement file.
 */
data class Statement(
    val relation: Relation,
    val target: AssetDescriptor,
) : StatementResult()

/**
 * Include statements point to another Digital Asset Links statement file.
 */
data class IncludeStatement(
    val include: String,
) : StatementResult()
