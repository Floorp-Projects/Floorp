/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.TextView
import androidx.appcompat.widget.SwitchCompat
import androidx.core.view.isVisible
import androidx.preference.PreferenceViewHolder
import androidx.preference.SwitchPreferenceCompat
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity

/**
 *  A [SwitchPreferenceCompat] that include a learn more link.
 */
abstract class LearnMoreSwitchPreference(context: Context, attrs: AttributeSet?) :
    SwitchPreferenceCompat(context, attrs) {

    init {
        layoutResource = R.layout.preference_switch_learn_more
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        val switch = holder.findViewById(R.id.learn_more_switch) as SwitchCompat

        switch.run {
            isChecked = getSwitchValue()
        }

        getDescription()?.let {
            val summaryView = holder.findViewById(android.R.id.summary) as TextView
            summaryView.text = it
            summaryView.isVisible = true
        }

        val learnMoreLink = holder.findViewById(R.id.link) as TextView
        learnMoreLink.paint?.isUnderlineText = true
        learnMoreLink.setOnClickListener {
            it.context.asActivity()?.let { activity ->
                (activity as HomeActivity).openToBrowserAndLoad(
                    searchTermOrURL = getLearnMoreUrl(),
                    newTab = true,
                    from = BrowserDirection.FromCookieBanner,
                )
            }
        }

        val backgroundDrawableArray =
            context.obtainStyledAttributes(intArrayOf(R.attr.selectableItemBackground))
        val backgroundDrawable = backgroundDrawableArray.getDrawable(0)
        backgroundDrawableArray.recycle()
        learnMoreLink.background = backgroundDrawable
    }

    /**
     *  Returns the description to be used the UI.
     */
    open fun getDescription(): String? = null

    /**
     *  Returns the URL that should be used when the learn more link is clicked.
     */
    abstract fun getLearnMoreUrl(): String

    /**
     *  Indicates the value which the switch widget should show.
     */
    abstract fun getSwitchValue(): Boolean
}
