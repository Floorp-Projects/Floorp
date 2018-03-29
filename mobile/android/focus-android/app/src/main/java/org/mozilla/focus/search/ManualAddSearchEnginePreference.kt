/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.os.Bundle
import android.os.Parcelable
import android.preference.Preference
import android.support.design.widget.TextInputLayout
import android.text.Editable
import android.text.TextUtils
import android.text.TextWatcher
import android.util.AttributeSet
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.ProgressBar
import org.mozilla.focus.R
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.utils.ViewUtils

class ManualAddSearchEnginePreference(context: Context, attrs: AttributeSet) : Preference(context, attrs) {
    private var engineNameEditText: EditText? = null
    private var searchQueryEditText: EditText? = null
    private var engineNameErrorLayout: TextInputLayout? = null
    private var searchQueryErrorLayout: TextInputLayout? = null

    private var progressView: ProgressBar? = null

    private var savedSearchEngineName: String? = null
    private var savedSearchQuery: String? = null

    override fun onCreateView(parent: ViewGroup?): View {
        val view = super.onCreateView(parent)

        engineNameErrorLayout = view.findViewById(R.id.edit_engine_name_layout)
        searchQueryErrorLayout = view.findViewById(R.id.edit_search_string_layout)

        engineNameEditText = view.findViewById(R.id.edit_engine_name)
        engineNameEditText?.addTextChangedListener(buildTextWatcherForErrorLayout(engineNameErrorLayout!!))
        searchQueryEditText = view.findViewById(R.id.edit_search_string)
        searchQueryEditText?.addTextChangedListener(buildTextWatcherForErrorLayout(searchQueryErrorLayout!!))

        progressView = view.findViewById(R.id.progress)

        updateState()

        ViewUtils.showKeyboard(engineNameEditText)

        return view
    }

    override fun onRestoreInstanceState(state: Parcelable?) {
        val bundle = state as Bundle
        super.onRestoreInstanceState(bundle.getParcelable(SUPER_STATE_KEY))
        savedSearchEngineName = bundle.getString(SEARCH_ENGINE_NAME_KEY)
        savedSearchQuery = bundle.getString(SEARCH_QUERY_KEY)
    }

    override fun onSaveInstanceState(): Parcelable {
        val state = super.onSaveInstanceState()
        val bundle = Bundle()
        bundle.putParcelable(SUPER_STATE_KEY, state)
        bundle.putString(SEARCH_ENGINE_NAME_KEY, engineNameEditText?.text.toString())
        bundle.putString(SEARCH_QUERY_KEY, searchQueryEditText?.text.toString())

        return bundle
    }

    fun validateEngineNameAndShowError(engineName: String): Boolean {
        val errorMessage = when {
            TextUtils.isEmpty(engineName) -> context.getString(R.string.search_add_error_empty_name)
            !engineNameIsUnique(engineName) -> context.getString(R.string.search_add_error_duplicate_name)
            else -> null
        }

        engineNameErrorLayout?.error = errorMessage
        return errorMessage == null
    }

    fun validateSearchQueryAndShowError(searchQuery: String): Boolean {
        val errorMessage = when {
            TextUtils.isEmpty(searchQuery) -> context.getString(R.string.search_add_error_empty_search)
            !UrlUtils.isValidSearchQueryUrl(searchQuery) -> context.getString(R.string.search_add_error_format)
            else -> null
        }

        searchQueryErrorLayout?.error = errorMessage
        return errorMessage == null
    }

    fun setSearchQueryErrorText(err: String) {
        searchQueryErrorLayout?.error = err
    }

    fun setProgressViewShown(isShown: Boolean) {
        progressView?.visibility = if (isShown) View.VISIBLE else View.GONE
    }
    private fun updateState() {
        if (engineNameEditText != null) engineNameEditText?.setText(savedSearchEngineName)
        if (searchQueryEditText != null) searchQueryEditText?.setText(savedSearchQuery)
    }

    private fun engineNameIsUnique(engineName: String): Boolean {
        val sharedPreferences = context.getSharedPreferences(SearchEngineManager.PREF_FILE_SEARCH_ENGINES,
                Context.MODE_PRIVATE)

        return !sharedPreferences.contains(engineName)
    }

    private fun buildTextWatcherForErrorLayout(errorLayout: TextInputLayout): TextWatcher {
        return object : TextWatcher {
            override fun beforeTextChanged(charSequence: CharSequence, i: Int, i1: Int, i2: Int) {}

            override fun onTextChanged(charSequence: CharSequence, i: Int, i1: Int, i2: Int) {
                errorLayout.error = null
            }

            override fun afterTextChanged(editable: Editable) {}
        }
    }

    companion object {
        private val SUPER_STATE_KEY = "super-state"
        private val SEARCH_ENGINE_NAME_KEY = "search-engine-name"
        private val SEARCH_QUERY_KEY = "search-query"
    }
}
