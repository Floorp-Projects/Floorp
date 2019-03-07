/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.layout

import android.view.View
import androidx.annotation.LayoutRes
import mozilla.components.browser.awesomebar.BrowserAwesomeBar
import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * A [SuggestionLayout] implementation defines how the suggestions of the [BrowserAwesomeBar] are getting layout. By
 * default [BrowserAwesomeBar] uses [DefaultSuggestionLayout]. However a consumer can provide its own implementation
 * in order to create a customized look & feel.
 */
interface SuggestionLayout {
    /**
     * Returns a layout resource ID to be used for this suggestion. The [BrowserAwesomeBar] implementation will take
     * care of inflating the layout or re-using instances as needed.
     */
    @LayoutRes
    fun getLayoutResource(suggestion: AwesomeBar.Suggestion): Int

    /**
     * Creates and returns a [SuggestionViewHolder] instance for the provided [View]. The [BrowserAwesomeBar] will call
     * [SuggestionViewHolder.bind] once this view holder should display the data of a specific [AwesomeBar.Suggestion].
     */
    fun createViewHolder(awesomeBar: BrowserAwesomeBar, view: View, @LayoutRes layoutId: Int): SuggestionViewHolder
}
