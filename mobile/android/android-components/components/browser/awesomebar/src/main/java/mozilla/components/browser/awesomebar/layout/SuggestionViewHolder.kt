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
}
