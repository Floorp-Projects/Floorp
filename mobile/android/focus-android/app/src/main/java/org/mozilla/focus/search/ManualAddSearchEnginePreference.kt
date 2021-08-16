/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.os.Bundle
import android.os.Parcelable
import android.text.Editable
import android.text.TextUtils
import android.text.TextWatcher
import android.util.AttributeSet
import android.view.View
import android.widget.EditText
import android.widget.ProgressBar
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import com.google.android.material.textfield.TextInputLayout
import org.mozilla.focus.R
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.utils.ViewUtils

class ManualAddSearchEnginePreference(context: Context, attrs: AttributeSet) :
    Preference(context, attrs) {
    private var engineNameEditText: EditText? = null
    private var searchQueryEditText: EditText? = null
    private var engineNameErrorLayout: TextInputLayout? = null
    private var searchQueryErrorLayout: TextInputLayout? = null

    private var progressView: ProgressBar? = null

    private var savedSearchEngineName: String? = null
    private var savedSearchQuery: String? = null

    override fun onBindViewHolder(holder: PreferenceViewHolder?) {
        super.onBindViewHolder(holder)

        engineNameErrorLayout =
            holder!!.findViewById(R.id.edit_engine_name_layout) as TextInputLayout
        searchQueryErrorLayout =
            holder.findViewById(R.id.edit_search_string_layout) as TextInputLayout

        engineNameEditText = holder.findViewById(R.id.edit_engine_name) as EditText
        engineNameEditText?.addTextChangedListener(
            buildTextWatcherForErrorLayout(
                engineNameErrorLayout!!
            )
        )
        searchQueryEditText = holder.findViewById(R.id.edit_search_string) as EditText
        searchQueryEditText?.addTextChangedListener(
            buildTextWatcherForErrorLayout(
                searchQueryErrorLayout!!
            )
        )

        progressView = holder.findViewById(R.id.progress) as ProgressBar

        updateState()

        ViewUtils.showKeyboard(engineNameEditText)
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
            TextUtils.isEmpty(engineName) ->
                context.getString(R.string.search_add_error_empty_name)

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
