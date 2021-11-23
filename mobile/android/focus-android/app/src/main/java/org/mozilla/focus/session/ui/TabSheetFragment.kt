/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewAnimationUtils
import android.view.ViewGroup
import android.view.animation.AccelerateInterpolator
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.focus.GleanMetrics.TabCount
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentSessionssheetBinding
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.utils.OneShotOnPreDrawListener
import kotlin.math.pow
import kotlin.math.sqrt

class TabSheetFragment : Fragment() {
    private var isAnimating: Boolean = false
    private lateinit var store: BrowserStore
    private var scope: CoroutineScope? = null
    private lateinit var binding: FragmentSessionssheetBinding

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        binding = FragmentSessionssheetBinding.inflate(layoutInflater, container, false)
        store = requireComponents.store

        binding.apply {
            background.setOnClickListener {
                if (isAnimating) {
                    // Ignore touched while we are animating
                    return@setOnClickListener
                }

                animateAndDismiss()
                val openedTabs = store.state.tabs.size
                TabCount.sessionListClosed.record(TabCount.SessionListClosedExtra(openedTabs))
            }

            OneShotOnPreDrawListener(binding.card) {
                playAnimation(false)
                true
            }

            val sessionsAdapter = TabsAdapter(
                tabList = store.state.privateTabs,
                isCurrentSession = { tab -> isCurrentSession(tab) },
                selectSession = { tab -> selectSession(tab) },
                closeSession = { tab -> closeSession(tab) }
            )

            sessions.apply {
                adapter = sessionsAdapter
                layoutManager = LinearLayoutManager(context)
                setHasFixedSize(true)
            }
        }

        return binding.root
    }

    override fun onDestroyView() {
        scope?.cancel()
        super.onDestroyView()
    }

    @Suppress("ComplexMethod")
    private fun playAnimation(reverse: Boolean): Animator {
        isAnimating = true

        val cardView = binding.card
        val offset = resources.getDimensionPixelSize(R.dimen.tab_sheet_end_margin) / 2
        val cx = cardView.measuredWidth - offset
        val cy = cardView.measuredHeight - offset

        // The final radius is the diagonal of the card view -> sqrt(w^2 + h^2)
        val fullRadius = sqrt(
            cardView.width.toDouble().pow(2.0) + cardView.height.toDouble().pow(2.0)
        ).toFloat()

        val sheetAnimator = ViewAnimationUtils.createCircularReveal(
            cardView, cx, cy, if (reverse) fullRadius else 0f, if (reverse) 0f else fullRadius
        ).apply {
            duration = ANIMATION_DURATION.toLong()
            interpolator = AccelerateInterpolator()
            addListener(object : AnimatorListenerAdapter() {
                override fun onAnimationStart(animation: Animator) {
                    cardView.visibility = View.VISIBLE
                }

                override fun onAnimationEnd(animation: Animator) {
                    isAnimating = false

                    cardView.visibility = if (reverse) View.GONE else View.VISIBLE
                }
            })
            start()
        }

        binding.background.alpha = if (reverse) 1f else 0f
        binding.background.animate()
            .alpha(if (reverse) 0f else 1f)
            .setDuration(ANIMATION_DURATION.toLong())
            .start()

        return sheetAnimator
    }

    private fun animateAndDismiss(): Animator {
        val animator = playAnimation(true)

        animator.addListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                // Use nullable Components reference: At the end of the animation the fragment
                // may already be detached if a navigation happened in the meantime.
                components?.appStore?.dispatch(AppAction.HideTabs)
            }
        })

        return animator
    }

    fun onBackPressed(): Boolean {
        animateAndDismiss()

        val openedTabs = store.state.tabs.size
        TabCount.sessionListClosed.record(TabCount.SessionListClosedExtra(openedTabs))

        return true
    }

    private fun isCurrentSession(tab: TabSessionState): Boolean {
        return requireComponents.store.state.selectedTabId == tab.id
    }

    private fun selectSession(tab: TabSessionState) {
        animateAndDismiss().addListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                requireComponents.tabsUseCases.selectTab.invoke(tab.id)

                val openedTabs = requireComponents.store.state.tabs.size
                TabCount.sessionListItemTapped.record(TabCount.SessionListItemTappedExtra(openedTabs))
            }
        })
    }

    private fun closeSession(tab: TabSessionState) {
        animateAndDismiss().addListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                requireComponents.tabsUseCases.removeTab.invoke(tab.id, selectParentIfExists = false)

                val openedTabs = requireComponents.store.state.tabs.size
                TabCount.sessionListItemTapped.record(TabCount.SessionListItemTappedExtra(openedTabs))
            }
        })
    }

    companion object {
        const val FRAGMENT_TAG = "tab_sheet"
        private const val ANIMATION_DURATION = 200
    }
}
