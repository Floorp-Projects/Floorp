/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions.ui

import android.graphics.Color
import android.os.Bundle
import android.text.SpannableStringBuilder
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.fragment_search_suggestions.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import org.mozilla.focus.R
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel
import org.mozilla.focus.searchsuggestions.State
import org.mozilla.focus.utils.UrlUtils
import kotlin.coroutines.CoroutineContext

class SearchSuggestionsFragment : Fragment(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main

    lateinit var searchSuggestionsViewModel: SearchSuggestionsViewModel

    override fun onResume() {
        super.onResume()

        if (job.isCancelled) {
            job = Job()
        }

        searchSuggestionsViewModel.refresh()
    }

    override fun onPause() {
        job.cancel()
        super.onPause()
    }

    @Suppress("DEPRECATION") // https://github.com/mozilla-mobile/focus-android/issues/4958
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)

        searchSuggestionsViewModel = ViewModelProvider(requireActivity())
            .get(SearchSuggestionsViewModel::class.java)

        searchSuggestionsViewModel.searchQuery.observe(viewLifecycleOwner, Observer {
            searchView.text = it
            searchView.contentDescription = requireContext().getString(R.string.search_hint, it)
        })

        searchSuggestionsViewModel.suggestions.observe(viewLifecycleOwner, Observer { suggestions ->
            launch(IO) {
                suggestions?.apply { (suggestionList.adapter as SuggestionsAdapter).refresh(this) }
            }
        })

        searchSuggestionsViewModel.state.observe(viewLifecycleOwner, Observer { state ->
            enable_search_suggestions_container.visibility = View.GONE
            no_suggestions_container.visibility = View.GONE
            suggestionList.visibility = View.GONE

            when (state) {
                is State.ReadyForSuggestions ->
                    suggestionList.visibility = View.VISIBLE
                is State.NoSuggestionsAPI ->
                    no_suggestions_container.visibility = if (state.givePrompt) {
                        View.VISIBLE
                    } else {
                        View.GONE
                    }
                is State.Disabled ->
                    enable_search_suggestions_container.visibility = if (state.givePrompt) {
                        View.VISIBLE
                    } else {
                        View.GONE
                    }
            }
        })
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_search_suggestions, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val appName = resources.getString(R.string.app_name)

        enable_search_suggestions_subtitle.text =
            resources.getString(R.string.enable_search_suggestion_description, appName)
        enable_search_suggestions_subtitle.movementMethod = LinkMovementMethod.getInstance()
        enable_search_suggestions_subtitle.highlightColor = Color.TRANSPARENT

        suggestionList.layoutManager = LinearLayoutManager(activity,
                RecyclerView.VERTICAL, false)
        suggestionList.adapter = SuggestionsAdapter {
            searchSuggestionsViewModel.selectSearchSuggestion(it)
        }

        searchView.setOnClickListener {
            val textView = it
            if (textView is TextView) {
                searchSuggestionsViewModel.selectSearchSuggestion(textView.text.toString(), true)
            }
        }

        enable_search_suggestions_button.setOnClickListener {
            searchSuggestionsViewModel.enableSearchSuggestions()
        }

        disable_search_suggestions_button.setOnClickListener {
            searchSuggestionsViewModel.disableSearchSuggestions()
        }

        dismiss_no_suggestions_message.setOnClickListener {
            searchSuggestionsViewModel.dismissNoSuggestionsMessage()
        }
    }

    companion object {
        fun create() = SearchSuggestionsFragment()
    }

    inner class SuggestionsAdapter(
        private val clickListener: (String) -> Unit
    ) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
        inner class DiffCallback(
            private val oldSuggestions: List<SpannableStringBuilder>,
            private val newSuggestions: List<SpannableStringBuilder>
        ) : DiffUtil.Callback() {
            override fun getOldListSize(): Int = oldSuggestions.size
            override fun getNewListSize(): Int = newSuggestions.size
            override fun areItemsTheSame(p0: Int, p1: Int): Boolean = true
            override fun areContentsTheSame(p0: Int, p1: Int): Boolean =
                    oldSuggestions[p0] == newSuggestions[p1]
            override fun getChangePayload(oldItemPosition: Int, newItemPosition: Int): Any? =
                    newSuggestions[newItemPosition]
        }

        private var suggestions: List<SpannableStringBuilder> = listOf()
        private var pendingJob: Job? = null

        suspend fun refresh(suggestions: List<SpannableStringBuilder>) = coroutineScope {
            pendingJob?.cancel()

            pendingJob = launch(IO) {
                val result = DiffUtil.calculateDiff(DiffCallback(this@SuggestionsAdapter.suggestions, suggestions))

                launch(Main) {
                    result.dispatchUpdatesTo(this@SuggestionsAdapter)
                    this@SuggestionsAdapter.suggestions = suggestions
                }
            }
        }

        override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int, payloads: MutableList<Any>) {
            if (payloads.isEmpty()) {
                super.onBindViewHolder(holder, position, payloads)
            } else {
                val payload = payloads[0] as? SpannableStringBuilder ?: return
                val view = holder as? SuggestionViewHolder ?: return
                view.bind(payload)
            }
        }

        override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
            if (holder is SuggestionViewHolder) {
                holder.bind(suggestions[position])
            }
        }

        override fun getItemCount(): Int = suggestions.count()

        override fun getItemViewType(position: Int): Int {
            return SuggestionViewHolder.LAYOUT_ID
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
            return SuggestionViewHolder(
                    LayoutInflater.from(parent.context).inflate(viewType, parent, false),
                    clickListener)
        }
    }
}

/**
 * ViewHolder implementation for a suggestion item in the list.
 */
private class SuggestionViewHolder(
    itemView: View,
    private val clickListener: (String) -> Unit
) : RecyclerView.ViewHolder(itemView) {
    companion object {
        const val LAYOUT_ID = R.layout.item_suggestion
    }

    val suggestionText: TextView = itemView.findViewById(R.id.suggestion)

    fun bind(suggestion: SpannableStringBuilder) {
        suggestionText.text = suggestion
        suggestionText.setPaddingRelative(
            itemView.resources.getDimensionPixelSize(R.dimen.search_suggestions_padding_with_icon),
            0,
            0,
            0
        )

        val backgroundDrawableArray =
                suggestionText.context.obtainStyledAttributes(intArrayOf(R.attr.selectableItemBackground))
        val backgroundDrawable = backgroundDrawableArray.getDrawable(0)
        backgroundDrawableArray.recycle()
        suggestionText.background = backgroundDrawable

        val size = itemView.resources.getDimension(R.dimen.preference_icon_drawable_size).toInt()

        if (UrlUtils.isUrl(suggestionText.text.toString())) {
            val icon = ContextCompat.getDrawable(itemView.context, R.drawable.ic_link)
            icon?.setBounds(0, 0, size, size)
            suggestionText.contentDescription = suggestionText.text
            suggestionText.setCompoundDrawables(icon, null, null, null)
        } else {
            val icon = ContextCompat.getDrawable(itemView.context, R.drawable.ic_search)
            icon?.setBounds(0, 0, size, size)
            suggestionText.contentDescription = itemView.context.getString(R.string.search_hint, suggestionText.text)
            suggestionText.setCompoundDrawables(icon, null, null, null)
        }
        itemView.setOnClickListener { clickListener(suggestion.toString()) }
    }
}
