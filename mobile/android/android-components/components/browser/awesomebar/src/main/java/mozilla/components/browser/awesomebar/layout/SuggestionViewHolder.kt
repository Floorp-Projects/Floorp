/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.layout

import android.view.View
import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * A view holder implementation for displaying an [AwesomeBar.Suggestion].
 */
abstract class SuggestionViewHolder(
    protected val view: View
) {
    /**
     * Binds the views in the holder to the [AwesomeBar.Suggestion].
     *
     * Contract: When a suggestion was selected/clicked the view will invoke the appropriate callback of the suggestion
     * ([AwesomeBar.Suggestion.onSuggestionClicked] or [AwesomeBar.Suggestion.onChipClicked]) as well as the provided
     * [selectionListener] function.
     */
    abstract fun bind(suggestion: AwesomeBar.Suggestion, selectionListener: () -> Unit)

    /**
     * Notifies this [SuggestionViewHolder] that it has been recycled. If this holder (or its views) keep references to
     * large or expensive data such as large bitmaps, this may be a good place to release those resources.
     */
    open fun recycle(): Unit = Unit
}
