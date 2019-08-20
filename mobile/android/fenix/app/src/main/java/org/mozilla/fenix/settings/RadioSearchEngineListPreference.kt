/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.CompoundButton
import mozilla.components.browser.search.SearchEngine
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class RadioSearchEngineListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = android.R.attr.preferenceStyle
) : SearchEngineListPreference(context, attrs, defStyleAttr) {
    override val itemResId: Int
        get() = R.layout.search_engine_radio_button

    override fun updateDefaultItem(defaultButton: CompoundButton) {
        defaultButton.isChecked = true
    }

    override fun onSearchEngineSelected(searchEngine: SearchEngine) {
        context.components.search.searchEngineManager.defaultSearchEngine = searchEngine
        Settings.getInstance(context).defaultSearchEngineName = searchEngine.name
    }
}
