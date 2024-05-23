/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The operations that can be performed on a language model.
 */
enum class ModelOperation(val operation: String) {
    /**
     * Download the model(s).
     */
    DOWNLOAD("download"),

    /**
     * Delete the model(s).
     */
    DELETE("delete"),
    ;

    /**
     * The operation will use the string literal on the engine.
     */
    override fun toString(): String {
        return operation
    }
}
