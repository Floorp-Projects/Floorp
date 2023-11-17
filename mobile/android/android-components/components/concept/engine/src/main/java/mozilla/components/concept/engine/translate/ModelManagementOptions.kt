/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The operations that can be performed on a given language model.
 *
 * @property languageToManage The BCP 47 language code to manage the models for.
 * May be null when performing operations not at the "language" scope or level.
 * @property operation The operation to perform.
 * @property operationLevel At what scope or level the operations should be performed at.
 */
data class ModelManagementOptions(
    val languageToManage: String? = null,
    val operation: ModelOperation,
    val operationLevel: OperationLevel,
)
