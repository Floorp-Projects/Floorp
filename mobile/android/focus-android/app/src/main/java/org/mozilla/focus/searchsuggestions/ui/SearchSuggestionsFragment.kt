package org.mozilla.focus.searchsuggestions.ui

import android.arch.lifecycle.Observer
import android.arch.lifecycle.ViewModelProviders
import android.os.Bundle
import android.support.v4.app.Fragment
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import kotlinx.android.synthetic.main.fragment_search_suggestions.searchView

import org.mozilla.focus.R
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel

class SearchSuggestionsFragment : Fragment() {

    private lateinit var searchSuggestionsViewModel: SearchSuggestionsViewModel

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        Log.e("HAHA ONE", "ONE")
        searchSuggestionsViewModel = ViewModelProviders.of(activity!!).get(SearchSuggestionsViewModel::class.java)
        searchSuggestionsViewModel.searchQuery.observe(this, Observer {
            searchView.text = it

            val query = it ?: return@Observer
            searchSuggestionsViewModel.fetchSuggestions(query)
        })

        searchSuggestionsViewModel.suggestions.observe(this, Observer {
            Log.e("foobar", "" + it)
        })
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? = inflater.inflate(R.layout.fragment_search_suggestions, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

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
}
