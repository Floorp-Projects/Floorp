/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.GuardedBy
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.awesomebar.layout.DefaultSuggestionLayout
import mozilla.components.browser.awesomebar.layout.SuggestionLayout
import mozilla.components.browser.awesomebar.layout.SuggestionViewHolder
import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * [RecyclerView.Adapter] for displaying [AwesomeBar.Suggestion] in [BrowserAwesomeBar].
 */
internal class SuggestionsAdapter(
    private val awesomeBar: BrowserAwesomeBar
) : RecyclerView.Adapter<ViewHolderWrapper>() {
    internal var layout: SuggestionLayout = DefaultSuggestionLayout()

    init {
        setHasStableIds(true)
    }

    /**
     * List of suggestions to be displayed by this adapter.
     */
    @GuardedBy("suggestions")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var suggestions = listOf<AwesomeBar.Suggestion>()

    /**
     * Mapping a provider to the suggestions that it provided. This allows us to remove/update the suggestions of
     * a specific provider after they have been added to the list.
     */
    private val suggestionMap: MutableMap<AwesomeBar.SuggestionProvider, List<AwesomeBar.Suggestion>> = mutableMapOf()

    /**
     * Adds the given list of suggestions (from the given provider) to this adapter.
     */
    fun addSuggestions(
        provider: AwesomeBar.SuggestionProvider,
        providerSuggestions: List<AwesomeBar.Suggestion>
    ) = synchronized(suggestions) {
        // Start with the current list of suggestions
        val updatedSuggestions = suggestions.toMutableList()

        // Remove previous suggestions of this provider.
        suggestionMap[provider]?.let { updatedSuggestions.removeAll(it) }

        // Remember which suggestions this provider added
        suggestionMap[provider] = providerSuggestions

        updatedSuggestions.addAll(providerSuggestions)

        updateTo(updatedSuggestions.sortedByDescending { it.score })

        // Make sure we're always displaying first suggestions at the top of the screen after input
        // changes. Without this manual scroll, we might end with UI "scrolled" to a middle of the
        // suggestions list.
        awesomeBar.scrollToPosition(0)
    }

    fun removeSuggestions(provider: AwesomeBar.SuggestionProvider) = synchronized(suggestions) {
        val updatedSuggestions = suggestions.toMutableList()

        suggestionMap[provider]?.let { updatedSuggestions.removeAll(it) }
        updateTo(updatedSuggestions)
    }

    /**
     * Removes all suggestions except the ones from providers that have set shouldClearSuggestions to false.
     */
    fun optionallyClearSuggestions() = synchronized(suggestions) {
        val updatedSuggestions = suggestions.toMutableList()

        suggestionMap.keys.forEach { provider ->
            if (provider.shouldClearSuggestions) {
                suggestionMap[provider]?.let { updatedSuggestions.removeAll(it) }
            }
        }

        updateTo(updatedSuggestions)
    }

    /**
     * Takes an updated list of suggestions, calculates the diff and then dispatches the appropriate notifications to
     * update the RecyclerView.
     */
    private fun updateTo(updatedSuggestions: List<AwesomeBar.Suggestion>) {
        val result = DiffUtil.calculateDiff(SuggestionDiffCallback(suggestions, updatedSuggestions))

        this.suggestions = updatedSuggestions
        result.dispatchUpdatesTo(this)
    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): ViewHolderWrapper = synchronized(suggestions) {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return ViewHolderWrapper(layout.createViewHolder(awesomeBar, view, viewType), view)
    }

    override fun getItemId(position: Int): Long = synchronized(suggestions) {
        val suggestion = suggestions[position]
        return awesomeBar.getUniqueSuggestionId(suggestion)
    }

    override fun getItemViewType(position: Int): Int = synchronized(suggestions) {
        val suggestion = suggestions[position]
        return layout.getLayoutResource(suggestion)
    }

    override fun getItemCount(): Int = synchronized(suggestions) {
        return suggestions.size
    }

    override fun onBindViewHolder(
        holder: ViewHolderWrapper,
        position: Int
    ) = synchronized(suggestions) {
        val suggestion = suggestions[position]
        holder.actual.bind(suggestion) { awesomeBar.listener?.invoke() }
    }

    override fun onViewRecycled(holder: ViewHolderWrapper) {
        holder.actual.recycle()
    }
}

internal class SuggestionDiffCallback(
    private val suggestions: List<AwesomeBar.Suggestion>,
    private val updatedSuggestions: List<AwesomeBar.Suggestion>
) : DiffUtil.Callback() {
    override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
        suggestions[oldItemPosition].id == updatedSuggestions[newItemPosition].id

    override fun getOldListSize(): Int = suggestions.size

    override fun getNewListSize(): Int = updatedSuggestions.size

    override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
        suggestions[oldItemPosition].areContentsTheSame(updatedSuggestions[newItemPosition])
}

internal class ViewHolderWrapper(
    val actual: SuggestionViewHolder,
    view: View
) : RecyclerView.ViewHolder(view)
