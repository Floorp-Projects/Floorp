/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.View
import androidx.annotation.StringRes
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.fragment.app.Fragment
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.focus.R
import org.mozilla.focus.utils.StatusBarUtils

/**
 * Similar behavior as [BaseSettingsFragment], but doesn't extend [PreferenceFragmentCompat] and is
 * a regular [Fragment] instead.
 */
open class BaseSettingsLikeFragment : Fragment() {
    fun updateTitle(title: String) {
        (requireActivity() as AppCompatActivity).supportActionBar?.title = title
    }

    fun updateTitle(@StringRes titleResource: Int) {
        updateTitle(getString(titleResource))
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val statusBarView = view.findViewById<View>(R.id.status_bar_background)
        StatusBarUtils.getStatusBarHeight(statusBarView) { statusBarHeight ->
            statusBarView.layoutParams.height = statusBarHeight
        }

        val toolbar = view.findViewById<Toolbar>(R.id.toolbar)

        val activity = requireActivity() as AppCompatActivity
        activity.setSupportActionBar(toolbar)
        activity.supportActionBar?.setDisplayHomeAsUpEnabled(true)
    }
}
