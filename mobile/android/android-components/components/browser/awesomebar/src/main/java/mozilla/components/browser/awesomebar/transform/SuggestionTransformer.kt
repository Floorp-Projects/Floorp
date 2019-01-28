/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.transform

import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * A [SuggestionTransformer] takes an input list of [AwesomeBar.Suggestion] and returns a new list of transformed
 * [AwesomeBar.Suggestion| objects.
 *
 * A [SuggestionTransformer] can be used to adding data, removing data or filtering suggestions:
 */
interface SuggestionTransformer {
    /**
     * Takes a list of [AwesomeBar.Suggestion] object and the [AwesomeBar.SuggestionProvider] instance that emitted the
     * suggestions in order to return a transformed list of [AwesomeBar.Suggestion] objects.
     */
    fun transform(
        provider: AwesomeBar.SuggestionProvider,
        suggestions: List<AwesomeBar.Suggestion>
    ): List<AwesomeBar.Suggestion>
}
