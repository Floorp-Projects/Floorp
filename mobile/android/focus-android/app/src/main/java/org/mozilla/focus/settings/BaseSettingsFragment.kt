/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.View
import android.widget.ListView
import androidx.annotation.StringRes
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.focus.R
import org.mozilla.focus.ext.setNavigationIcon
import org.mozilla.focus.utils.StatusBarUtils

abstract class BaseSettingsFragment : PreferenceFragmentCompat() {
    fun updateTitle(title: String) {
        (requireActivity() as AppCompatActivity).supportActionBar?.title = title
    }

    fun updateTitle(@StringRes titleResource: Int) {
        updateTitle(getString(titleResource))
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val list = view.findViewById<View>(android.R.id.list) as? ListView
        list?.divider = null

        val statusBarView = view.findViewById<View>(R.id.status_bar_background)
        StatusBarUtils.getStatusBarHeight(statusBarView) { statusBarHeight ->
            statusBarView.layoutParams.height = statusBarHeight
            statusBarView.setBackgroundColor(
                ContextCompat.getColor(
                    view.context,
                    R.color.settings_background
                )
            )
        }

        val toolbar = view.findViewById<Toolbar>(R.id.toolbar)

        val activity = requireActivity() as AppCompatActivity
        activity.setSupportActionBar(toolbar)
        activity.setNavigationIcon(R.drawable.ic_back_button)
    }
}
