package org.mozilla.focus.searchsuggestions.ui

import android.arch.lifecycle.Observer
import android.arch.lifecycle.ViewModelProviders
import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.support.v4.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import kotlinx.android.synthetic.main.firstrun_page.*
import kotlinx.android.synthetic.main.firstrun_page.view.*
import kotlinx.android.synthetic.main.fragment_search_suggestions.*

import org.mozilla.focus.R

/**
 * A simple [Fragment] subclass.
 * Activities that contain this fragment must implement the
 * [SearchSuggestionsFragment.OnFragmentInteractionListener] interface
 * to handle interaction events.
 * Use the [SearchSuggestionsFragment.newInstance] factory method to
 * create an instance of this fragment.
 *
 */
class SearchSuggestionsFragment : Fragment() {

    private lateinit var searchSuggestionsViewModel: SearchSuggestionsViewModel

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        searchSuggestionsViewModel = ViewModelProviders.of(activity!!).get(SearchSuggestionsViewModel::class.java)
        searchSuggestionsViewModel.searchQuery.observe(this, Observer {
            searchView.text = it
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
