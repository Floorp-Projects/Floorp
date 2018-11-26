/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser

import android.view.View
import android.widget.CompoundButton
import android.widget.Switch
import android.widget.TextView
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.launch
import mozilla.components.support.utils.ThreadUtils
import org.mozilla.focus.R
import org.mozilla.focus.exceptions.ExceptionDomains
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.telemetry.TelemetryWrapper
import java.net.MalformedURLException
import java.net.URL

internal class BlockingItemViewHolder(itemView: View, private val fragment: BrowserFragment) :
    BrowserMenuViewHolder(itemView), CompoundButton.OnCheckedChangeListener {

    private val trackerCounter: TextView

    init {
        val switchView = itemView.findViewById<Switch>(R.id.blocking_switch)
        switchView.isChecked = fragment.session.trackerBlockingEnabled
        switchView.setOnCheckedChangeListener(this)

        val helpView = itemView.findViewById<View>(R.id.help_trackers)
        helpView.setOnClickListener { view ->
            if (browserFragment != null) {
                browserFragment.onClick(view)
            }
        }

        trackerCounter = itemView.findViewById(R.id.trackers_count)

        updateTrackers(fragment.session.trackersBlocked.size)
    }

    fun updateTrackers(trackers: Int) {
        if (fragment.session.trackerBlockingEnabled) {
            updateTrackingCount(trackerCounter, trackers)
        } else {
            disableTrackingCount(trackerCounter)
        }
    }

    private fun updateTrackingCount(view: TextView, count: Int) {
        ThreadUtils.postToMainThread(Runnable { view.text = count.toString() })
    }

    private fun disableTrackingCount(view: TextView) {
        ThreadUtils.postToMainThread(Runnable { view.setText(R.string.content_blocking_disabled) })
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        fragment.setBlockingUI(isChecked)

        val url = fragment.url
        val host = try {
            URL(url).host
        } catch (e: MalformedURLException) {
            url
        } ?: url

        if (!isChecked) {
            addUrlToExceptionsList(host = host)
        } else {
            removeUrlFromExceptionsList(host = host)
        }

        TelemetryWrapper.blockingSwitchEvent(isChecked)

        // Delay closing the menu and reloading the website a bit so that the user can actually see
        // the switch change its state.
        ThreadUtils.postToMainThreadDelayed(Runnable {
            menu.dismiss()

            fragment.reload()
        }, Switch_THUMB_ANIMATION_DURATION)
    }

    private fun addUrlToExceptionsList(host: String) {
        fragment.launch(IO) {
            val duplicateURL = ExceptionDomains.load(fragment.requireContext()).contains(host)

            if (duplicateURL) return@launch
            ExceptionDomains.add(fragment.requireContext(), host)
        }
    }

    private fun removeUrlFromExceptionsList(host: String) {
        fragment.launch(IO) {
            if (ExceptionDomains.load(fragment.requireContext()).contains(host)) {
                TelemetryWrapper.removeExceptionDomains(1)
                ExceptionDomains.remove(fragment.requireContext(), listOf(host))
            }
        }
    }

    companion object {
        val LAYOUT_ID = R.layout.menu_blocking_switch
        val Switch_THUMB_ANIMATION_DURATION = 250L
    }
}
