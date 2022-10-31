/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.ext

import mozilla.components.browser.engine.gecko.prompt.GeckoChoice
import mozilla.components.concept.engine.prompt.Choice

/**
 * Converts a GeckoView [GeckoChoice] to an Android Components [Choice].
 */
private fun GeckoChoice.toChoice(): Choice {
    val choiceChildren = items?.map { it.toChoice() }?.toTypedArray()
    // On the GeckoView docs states that label is a @NonNull, but on run-time
    // we are getting null values
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1771149
    @Suppress("USELESS_ELVIS")
    return Choice(id, !disabled, label ?: "", selected, separator, choiceChildren)
}

/**
 * Convert an array of [GeckoChoice] to Choice array.
 * @return array of Choice
 */
fun convertToChoices(
    geckoChoices: Array<out GeckoChoice>,
): Array<Choice> = geckoChoices.map { geckoChoice ->
    val choice = geckoChoice.toChoice()
    choice
}.toTypedArray()
