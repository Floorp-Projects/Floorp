package org.mozilla.focus.searchsuggestions.ui

import android.content.Context
import android.util.AttributeSet
import org.mozilla.focus.R
import org.mozilla.focus.settings.LearnMoreSwitchPreference

/**
 * Switch preference for enabling/disabling autocompletion for default domains that ship with the app.
 */
class SearchSuggestionsPreference(
        context: Context?,
        attrs: AttributeSet?
) : LearnMoreSwitchPreference(context, attrs) {
    override fun getLearnMoreUrl() = "https://mozilla.org"

    override fun getDescription(): String? =
            context.getString(R.string.preference_show_search_suggestions_summary,
                    context.getString(R.string.app_name))
}
