/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.deletebrowsingdata

import android.content.Context
import androidx.annotation.StringRes
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey

enum class DeleteBrowsingDataOnQuitType(@StringRes private val prefKey: Int) {
    TABS(R.string.pref_key_delete_open_tabs_on_quit),
    HISTORY(R.string.pref_key_delete_browsing_history_on_quit),
    COOKIES(R.string.pref_key_delete_cookies_and_site_data_on_quit),
    CACHE(R.string.pref_key_delete_caches_on_quit),
    PERMISSIONS(R.string.pref_key_delete_permissions_on_quit),
    DOWNLOADS(R.string.pref_key_delete_downloads_on_quit),
    ;

    fun getPreferenceKey(context: Context) = context.getPreferenceKey(prefKey)
}
