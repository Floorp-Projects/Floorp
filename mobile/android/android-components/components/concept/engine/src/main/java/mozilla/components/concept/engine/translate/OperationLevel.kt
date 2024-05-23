/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The level or scope of a model operation.
 */
enum class OperationLevel(val operationLevel: String) {
    /**
     * Complete the operation for a given language.
     */
    LANGUAGE("language"),

    /**
     * Complete the operation on cache elements.
     * (Elements that do not fully make a downloaded language package or [LanguageModel].)
     */
    CACHE("cache"),

    /**
     * Complete the operation all models.
     */
    ALL("all"),
    ;

    /**
     * The operation level will use the string literal on the engine.
     */
    override fun toString(): String {
        return operationLevel
    }
}
