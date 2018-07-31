package org.mozilla.focus.searchsuggestions.ui

import android.arch.lifecycle.Observer
import android.arch.lifecycle.ViewModelProviders
import android.os.Bundle
import android.support.v4.app.Fragment
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.text.SpannableString
import android.text.SpannableStringBuilder
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import kotlinx.android.synthetic.main.fragment_autocomplete_customdomains.*
import kotlinx.android.synthetic.main.fragment_search_suggestions.*
import kotlinx.android.synthetic.main.item_suggestion.*

import org.mozilla.focus.R
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel

class SearchSuggestionsFragment : Fragment() {

    private lateinit var searchSuggestionsViewModel: SearchSuggestionsViewModel

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        searchSuggestionsViewModel = ViewModelProviders.of(activity!!).get(SearchSuggestionsViewModel::class.java)
        searchSuggestionsViewModel.searchQuery.observe(this, Observer {
            searchView.text = it
            it?.apply(searchSuggestionsViewModel::fetchSuggestions)
        })

        searchSuggestionsViewModel.suggestions.observe(this, Observer { suggestions ->
            suggestions?.apply { (suggestionList.adapter as SuggestionsAdapter).refresh(this) }
        })
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? = inflater.inflate(R.layout.fragment_search_suggestions, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        suggestionList.layoutManager = LinearLayoutManager(activity, LinearLayoutManager.VERTICAL, false)
        suggestionList.adapter = SuggestionsAdapter()

        searchView.setOnClickListener {
            val textView = it
            if (textView is TextView) {
                searchSuggestionsViewModel.selectSearchSuggestion(textView.text.toString())
            }
        }
    }

    companion object {
        fun create() = SearchSuggestionsFragment()
    }

    inner class SuggestionsAdapter: RecyclerView.Adapter<RecyclerView.ViewHolder>() {

        private var suggestions: List<SpannableStringBuilder> = listOf()

        fun refresh(suggestions: List<SpannableStringBuilder>) {
            this.suggestions = suggestions
            notifyDataSetChanged()
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
            return SuggestionViewHolder(LayoutInflater.from(parent.context).inflate(viewType, parent, false))
        }
    }
}


/**
 * ViewHolder implementation for a suggestion item in the list.
 */
private class SuggestionViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
    companion object {
        val LAYOUT_ID = R.layout.item_suggestion
    }

    val suggestionText: TextView = itemView.findViewById(R.id.suggestion)

    fun bind(suggestion: SpannableStringBuilder) {
        suggestionText.text = suggestion
    }
}
