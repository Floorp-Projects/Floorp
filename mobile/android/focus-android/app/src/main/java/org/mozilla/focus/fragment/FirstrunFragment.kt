/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.fragment

import android.content.Context
import android.os.Bundle
import android.transition.TransitionInflater
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.preference.PreferenceManager
import androidx.viewpager.widget.ViewPager
import org.mozilla.focus.GleanMetrics.Onboarding
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentFirstrunBinding
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.firstrun.FirstrunPagerAdapter
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.StatusBarUtils
import kotlin.math.abs

class FirstrunFragment : Fragment(), View.OnClickListener {

    private var _binding: FragmentFirstrunBinding? = null
    private val binding get() = _binding!!

    override fun onAttach(context: Context) {
        super.onAttach(context)

        val transition = TransitionInflater.from(context).inflateTransition(R.transition.firstrun_exit)

        exitTransition = transition

        // We will send a telemetry event whenever a new firstrun page is shown. However this page
        // listener won't fire for the initial page we are showing. So we are going to firing here.
        Onboarding.pageDisplayed.record(Onboarding.PageDisplayedExtra(0))
        TelemetryWrapper.showFirstRunPageEvent(0)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragmentFirstrunBinding.inflate(inflater, container, false)

        setupPager()

        binding.tabs.setupWithViewPager(binding.pager, true)

        binding.skip.setOnClickListener(this)

        return binding.root
    }

    override fun onClick(view: View) {
        val currentItem = binding.pager.currentItem
        when (view.id) {
            R.id.next -> {
                binding.pager.currentItem = binding.pager.currentItem + 1
                Onboarding.nextButtonTapped.record(Onboarding.NextButtonTappedExtra(currentItem))
            }

            R.id.skip -> {
                finishFirstrun()
                Onboarding.skipButtonTapped.record(Onboarding.SkipButtonTappedExtra(currentItem))

                TelemetryWrapper.skipFirstRunEvent()
            }

            R.id.finish -> {
                finishFirstrun()
                Onboarding.finishButtonTapped.record(Onboarding.FinishButtonTappedExtra(currentItem))

                TelemetryWrapper.finishFirstRunEvent()
            }

            else -> throw IllegalArgumentException("Unknown view")
        }
    }

    @Suppress("MagicNumber")
    private fun setupPager() {
        val firstRunPagerAdapter = FirstrunPagerAdapter(requireContext(), this)
        binding.pager.apply {
            contentDescription = firstRunPagerAdapter.getPageAccessibilityDescription(0)
            isFocusable = true

            setPageTransformer(true) { page, position ->
                page.alpha = 1 - 0.5f * abs(position)
            }

            clipToPadding = false
            adapter = firstRunPagerAdapter
            addOnPageChangeListener(object : ViewPager.OnPageChangeListener {
                override fun onPageSelected(position: Int) {
                    Onboarding.pageDisplayed.record(Onboarding.PageDisplayedExtra(0))
                    TelemetryWrapper.showFirstRunPageEvent(position)

                    contentDescription =
                        firstRunPagerAdapter.getPageAccessibilityDescription(position)
                }

                override fun onPageScrolled(position: Int, positionOffset: Float, positionOffsetPixels: Int) {}

                override fun onPageScrollStateChanged(state: Int) {}
            })
        }
    }

    private fun finishFirstrun() {
        PreferenceManager.getDefaultSharedPreferences(requireContext())
            .edit()
            .putBoolean(FIRSTRUN_PREF, true)
            .apply()

        val selectedTabId = requireComponents.store.state.selectedTabId
        requireComponents.appStore.dispatch(AppAction.FinishFirstRun(selectedTabId))
    }

    override fun onResume() {
        super.onResume()
        StatusBarUtils.getStatusBarHeight(binding.background) { statusBarHeight ->
            binding.background.setPadding(
                0,
                statusBarHeight,
                0,
                0
            )
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        const val FRAGMENT_TAG = "firstrun"
        const val FIRSTRUN_PREF = "firstrun_shown"

        fun create(): FirstrunFragment {
            val arguments = Bundle()

            val fragment = FirstrunFragment()
            fragment.arguments = arguments

            return fragment
        }
    }
}
