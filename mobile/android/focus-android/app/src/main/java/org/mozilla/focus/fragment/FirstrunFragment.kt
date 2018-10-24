/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.fragment

import android.content.Context
import android.graphics.drawable.TransitionDrawable
import android.os.Bundle
import android.preference.PreferenceManager
import android.support.design.widget.TabLayout
import android.support.v4.app.Fragment
import android.support.v4.view.ViewPager
import android.transition.TransitionInflater
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import mozilla.components.browser.session.Session
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.firstrun.FirstrunPagerAdapter
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.StatusBarUtils

class FirstrunFragment : Fragment(), View.OnClickListener {

    private var viewPager: ViewPager? = null

    private var background: View? = null

    override fun onAttach(context: Context?) {
        super.onAttach(context)

        val transition = TransitionInflater.from(context).inflateTransition(R.transition.firstrun_exit)

        exitTransition = transition

        // We will send a telemetry event whenever a new firstrun page is shown. However this page
        // listener won't fire for the initial page we are showing. So we are going to firing here.
        TelemetryWrapper.showFirstRunPageEvent(0)
    }

    @Suppress("MagicNumber")
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_firstrun, container, false)

        view.findViewById<View>(R.id.skip).setOnClickListener(this)

        background = view.findViewById(R.id.background)

        val adapter = FirstrunPagerAdapter(container!!.context, this)

        viewPager = view.findViewById(R.id.pager)
        viewPager!!.contentDescription = adapter.getPageAccessibilityDescription(0)
        viewPager!!.isFocusable = true

        viewPager!!.setPageTransformer(true) { page, position -> page.alpha = 1 - 0.5f * Math.abs(position) }

        viewPager!!.clipToPadding = false
        viewPager!!.adapter = adapter
        viewPager!!.addOnPageChangeListener(object : ViewPager.OnPageChangeListener {
            override fun onPageSelected(position: Int) {
                TelemetryWrapper.showFirstRunPageEvent(position)

                val drawable = background!!.background as TransitionDrawable

                if (position == adapter.count - 1) {
                    drawable.startTransition(200)
                } else {
                    drawable.resetTransition()
                }

                viewPager!!.contentDescription = adapter.getPageAccessibilityDescription(position)
            }

            override fun onPageScrolled(position: Int, positionOffset: Float, positionOffsetPixels: Int) {}

            override fun onPageScrollStateChanged(state: Int) {}
        })

        val tabLayout = view.findViewById<TabLayout>(R.id.tabs)
        tabLayout.setupWithViewPager(viewPager, true)

        return view
    }

    override fun onClick(view: View) {
        when (view.id) {
            R.id.next -> viewPager!!.currentItem = viewPager!!.currentItem + 1

            R.id.skip -> {
                finishFirstrun()
                TelemetryWrapper.skipFirstRunEvent()
            }

            R.id.finish -> {
                finishFirstrun()
                TelemetryWrapper.finishFirstRunEvent()
            }

            else -> throw IllegalArgumentException("Unknown view")
        }
    }

    private fun finishFirstrun() {
        val fragmentManager = requireActivity().supportFragmentManager

        PreferenceManager.getDefaultSharedPreferences(requireContext())
            .edit()
            .putBoolean(FIRSTRUN_PREF, true)
            .apply()

        val sessionUUID = arguments!!.getString(ARGUMENT_SESSION_UUID)
        val sessionManager = requireContext().components.sessionManager

        val fragment: Fragment
        fragment = if (sessionUUID == null) {
            UrlInputFragment.createWithoutSession()
        } else {
            val session = sessionManager.findSessionById(sessionUUID)
            BrowserFragment.createForSession(session!!)
        }

        val fragmentTag =
            if (fragment is BrowserFragment) BrowserFragment.FRAGMENT_TAG else UrlInputFragment.FRAGMENT_TAG

        fragmentManager
            .beginTransaction()
            .replace(R.id.container, fragment, fragmentTag)
            .commit()
    }

    override fun onResume() {
        super.onResume()
        StatusBarUtils.getStatusBarHeight(background) { statusBarHeight ->
            background!!.setPadding(
                0,
                statusBarHeight,
                0,
                0
            )
        }
    }

    companion object {
        const val FRAGMENT_TAG = "firstrun"
        const val FIRSTRUN_PREF = "firstrun_shown"

        private const val ARGUMENT_SESSION_UUID = "sessionUUID"

        fun create(currentSession: Session?): FirstrunFragment {
            val uuid = currentSession?.id

            val arguments = Bundle()
            arguments.putString(ARGUMENT_SESSION_UUID, uuid)

            val fragment = FirstrunFragment()
            fragment.arguments = arguments

            return fragment
        }
    }
}
