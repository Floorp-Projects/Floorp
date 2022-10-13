/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.awesomebar

import android.graphics.Bitmap
import android.graphics.drawable.Drawable
import android.view.View
import java.util.UUID

/**
 * Interface to be implemented by awesome bar implementations.
 *
 * An awesome bar has multiple duties:
 *  - Display [Suggestion] instances and invoking its callbacks once selected
 *  - React to outside events: [onInputStarted], [onInputChanged], [onInputCancelled].
 *  - Query [SuggestionProvider] instances for new suggestions when the text changes.
 */
interface AwesomeBar {

    /**
     * Adds the following [SuggestionProvider] instances to be queried for [Suggestion]s whenever the text changes.
     */
    fun addProviders(vararg providers: SuggestionProvider)

    /**
     * Removes the following [SuggestionProvider]
     */
    fun removeProviders(vararg providers: SuggestionProvider)

    /**
     * Removes all [SuggestionProvider]s
     */
    fun removeAllProviders()

    /**
     * Returns whether or not this awesome bar contains the following [SuggestionProvider]
     */
    fun containsProvider(provider: SuggestionProvider): Boolean

    /**
     * Fired when the user starts interacting with the awesome bar by entering text in the toolbar.
     */
    fun onInputStarted() = Unit

    /**
     * Fired whenever the user changes their input, after they have started interacting with the awesome bar.
     *
     * @param text The current user input in the toolbar.
     */
    fun onInputChanged(text: String)

    /**
     * Fired when the user has cancelled their interaction with the awesome bar.
     */
    fun onInputCancelled() = Unit

    /**
     * Casts this awesome bar to an Android View object.
     */
    fun asView(): View = this as View

    /**
     * Adds a lambda to be invoked when the user has finished interacting with the awesome bar (e.g. selected a
     * suggestion).
     */
    fun setOnStopListener(listener: () -> Unit)

    /**
     * Adds a lambda to be invoked when the user selected a suggestion to be edited further.
     */
    fun setOnEditSuggestionListener(listener: (String) -> Unit)

    /**
     * A [Suggestion] to be displayed by an [AwesomeBar] implementation.
     *
     * @property provider The provider this suggestion came from.
     * @property id A unique ID (provider scope) identifying this [Suggestion]. A stable ID but different data indicates
     * to the [AwesomeBar] that this is the same [Suggestion] with new data. This will affect how the [AwesomeBar]
     * animates showing the new suggestion.
     * @property title A user-readable title for the [Suggestion].
     * @property description A user-readable description for the [Suggestion].
     * @property editSuggestion The string that will be set to the url bar when using the edit suggestion arrow.
     * @property icon A lambda that can be invoked by the [AwesomeBar] implementation to receive an icon [Bitmap] for
     * this [Suggestion]. The [AwesomeBar] will pass in its desired width and height for the Bitmap.
     * @property indicatorIcon A drawable for indicating different types of [Suggestion].
     * @property chips A list of [Chip] instances to be displayed.
     * @property flags A set of [Flag] values for this [Suggestion].
     * @property onSuggestionClicked A callback to be executed when the [Suggestion] was clicked by the user.
     * @property onChipClicked A callback to be executed when a [Chip] was clicked by the user.
     * @property score A score used to rank suggestions of this provider against each other. A suggestion with a higher
     * score will be shown on top of suggestions with a lower score.
     */
    data class Suggestion(
        val provider: SuggestionProvider,
        val id: String = UUID.randomUUID().toString(),
        val title: String? = null,
        val description: String? = null,
        val editSuggestion: String? = null,
        val icon: Bitmap? = null,
        val indicatorIcon: Drawable? = null,
        val chips: List<Chip> = emptyList(),
        val flags: Set<Flag> = emptySet(),
        val onSuggestionClicked: (() -> Unit)? = null,
        val onChipClicked: ((Chip) -> Unit)? = null,
        val score: Int = 0,
    ) {
        /**
         * Chips are compact actions that are shown as part of a suggestion. For example a [Suggestion] from a search
         * engine may offer multiple search suggestion chips for different search terms.
         */
        data class Chip(
            val title: String,
        )

        /**
         * Flags can be added by a [SuggestionProvider] to help the [AwesomeBar] implementation decide how to display
         * a specific [Suggestion]. For example an [AwesomeBar] could display a bookmark star icon next to [Suggestion]s
         * that contain the [BOOKMARK] flag.
         */
        enum class Flag {
            BOOKMARK,
            OPEN_TAB,
            CLIPBOARD,
            SYNC_TAB,
        }

        /**
         * Returns true if the content of the two suggestions is the same.
         *
         * This is used by [AwesomeBar] implementations to decide whether an updated suggestion (same id) needs its
         * view to be updated in order to display new data.
         */
        fun areContentsTheSame(other: Suggestion): Boolean {
            return title == other.title &&
                description == other.description &&
                chips == other.chips &&
                flags == other.flags
        }
    }

    /**
     * A [SuggestionProvider] is queried by an [AwesomeBar] whenever the text in the address bar is changed by the user.
     * It returns a list of [Suggestion]s to be displayed by the [AwesomeBar].
     */
    interface SuggestionProvider {
        /**
         * A unique ID used for identifying this provider.
         *
         * The recommended approach for a [SuggestionProvider] implementation is to generate a UUID.
         */
        val id: String

        /**
         * A header title for grouping the suggestions.
         **/
        fun groupTitle(): String? = null

        /**
         * Fired when the user starts interacting with the awesome bar by entering text in the toolbar.
         *
         * The provider has the option to return an initial list of suggestions that will be displayed before the
         * user has entered/modified any of the text.
         */
        fun onInputStarted(): List<Suggestion> = emptyList()

        /**
         * Fired whenever the user changes their input, after they have started interacting with the awesome bar.
         *
         * This is a suspending function. An [AwesomeBar] implementation is expected to invoke this method from a
         * [Coroutine](https://kotlinlang.org/docs/reference/coroutines-overview.html). This allows the [AwesomeBar]
         * implementation to group and cancel calls to multiple providers.
         *
         * Coroutine cancellation is cooperative. A coroutine code has to cooperate to be cancellable:
         * https://github.com/Kotlin/kotlinx.coroutines/blob/master/docs/cancellation-and-timeouts.md
         *
         * @param text The current user input in the toolbar.
         * @return A list of suggestions to be displayed by the [AwesomeBar].
         */
        suspend fun onInputChanged(text: String): List<Suggestion>

        /**
         * Fired when the user has cancelled their interaction with the awesome bar.
         */
        fun onInputCancelled() = Unit
    }

    /**
     * A group of [SuggestionProvider]s.
     *
     * @property providers The list of [SuggestionProvider]s in this group.
     * @property title An optional title for this group. The title may be rendered by an AwesomeBar
     * implementation.
     * @property limit The maximum number of suggestions that will be shown in this group.
     * @property id A unique ID for this group (uses a generated UUID by default)
     */
    data class SuggestionProviderGroup(
        val providers: List<SuggestionProvider>,
        val title: String? = null,
        val limit: Int = Integer.MAX_VALUE,
        val id: String = UUID.randomUUID().toString(),
    )
}
