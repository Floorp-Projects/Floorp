/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.ext

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.HighlightableMenuItem

/**
 * Get the highlight effect present in the list of menu items, if any.
 */
@Suppress("Deprecation")
fun List<BrowserMenuItem>.getHighlight() = asSequence()
    .filter { it.visible() }
    .mapNotNull { it as? HighlightableMenuItem }
    .filter { it.isHighlighted() }
    .map { it.highlight }
    .filter { it.canPropagate }
    .maxByOrNull {
        // Select the highlight with the highest priority
        when (it) {
            is BrowserMenuHighlight.HighPriority -> 2
            is BrowserMenuHighlight.LowPriority -> 1
            is BrowserMenuHighlight.ClassicHighlight -> 0
        }
    }

/**
 * Converts the menu items into a menu candidate list.
 */
fun List<BrowserMenuItem>.asCandidateList(context: Context) =
    mapNotNull { it.asCandidate(context) }
