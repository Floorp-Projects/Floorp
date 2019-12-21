/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu.ext

import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.MenuEffect
import mozilla.components.concept.menu.candidate.MenuIcon
import mozilla.components.concept.menu.candidate.MenuIconEffect
import mozilla.components.concept.menu.candidate.RowMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate

private fun MenuIcon?.effect(): MenuIconEffect? =
    if (this is DrawableMenuIcon) effect else null

/**
 * Find the effects used by the menu.
 * Disabled and invisible menu items are not included.
 */
fun List<MenuCandidate>.effects(): Sequence<MenuEffect> = this.asSequence()
    .filter { option -> option.containerStyle.isVisible && option.containerStyle.isEnabled }
    .flatMap { option ->
        when (option) {
            is TextMenuCandidate ->
                sequenceOf(option.effect, option.start.effect(), option.end.effect()).filterNotNull()
            is CompoundMenuCandidate ->
                sequenceOf(option.effect, option.start.effect()).filterNotNull()
            is RowMenuCandidate ->
                option.items.asSequence()
                    .filter { it.containerStyle.isVisible && it.containerStyle.isEnabled }
                    .mapNotNull { it.icon.effect }
            is DecorativeTextMenuCandidate, is DividerMenuCandidate -> emptySequence()
        }
    }

/**
 * Select the highlight with the highest priority.
 */
fun Sequence<MenuEffect>.max() = maxBy {
    // Select the highlight with the highest priority
    when (it) {
        is HighPriorityHighlightEffect -> 2
        is LowPriorityHighlightEffect -> 1
    }
}
