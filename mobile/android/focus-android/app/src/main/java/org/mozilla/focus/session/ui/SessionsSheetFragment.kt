/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.os.Bundle
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewAnimationUtils
import android.view.ViewGroup
import android.view.animation.AccelerateInterpolator

import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.locale.LocaleAwareFragment
import org.mozilla.focus.session.SessionManager
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.OneShotOnPreDrawListener

class SessionsSheetFragment : LocaleAwareFragment(), View.OnClickListener {

    private lateinit var backgroundView: View
    private lateinit var cardView: View
    private var isAnimating: Boolean = false

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_sessionssheet, container, false)

        backgroundView = view.findViewById(R.id.background)
        backgroundView.setOnClickListener(this)

        cardView = view.findViewById(R.id.card)
        cardView.viewTreeObserver.addOnPreDrawListener(object : OneShotOnPreDrawListener(cardView) {
            override fun onPreDraw(view: View) {
                playAnimation(false)
            }
        })

        val sessionsAdapter = SessionsAdapter(this)
        SessionManager.getInstance().sessions.observe(this, sessionsAdapter)

        view.findViewById<RecyclerView>(R.id.sessions).let {
            it.layoutManager = LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false)
            it.adapter = sessionsAdapter
        }

        return view
    }

    @Suppress("ComplexMethod")
    private fun playAnimation(reverse: Boolean): Animator {
        isAnimating = true

        val offset = resources.getDimensionPixelSize(R.dimen.floating_action_button_size) / 2
        val cx = cardView.measuredWidth - offset
        val cy = cardView.measuredHeight - offset

        // The final radius is the diagonal of the card view -> sqrt(w^2 + h^2)
        val fullRadius = Math.sqrt(
            Math.pow(cardView.width.toDouble(), 2.0) + Math.pow(cardView.height.toDouble(), 2.0)
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

        backgroundView.alpha = if (reverse) 1f else 0f
        backgroundView.animate()
            .alpha(if (reverse) 0f else 1f)
            .setDuration(ANIMATION_DURATION.toLong())
            .start()

        return sheetAnimator
    }

    internal fun animateAndDismiss(): Animator {
        val animator = playAnimation(true)

        animator.addListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                val activity = activity as MainActivity?
                activity?.supportFragmentManager?.beginTransaction()?.remove(this@SessionsSheetFragment)?.commit()
            }
        })

        return animator
    }

    fun onBackPressed(): Boolean {
        animateAndDismiss()

        TelemetryWrapper.closeTabsTrayEvent()
        return true
    }

    override fun applyLocale() {}

    override fun onClick(view: View) {
        if (isAnimating) {
            // Ignore touched while we are animating
            return
        }

        when (view.id) {
            R.id.background -> {
                animateAndDismiss()

                TelemetryWrapper.closeTabsTrayEvent()
            }

            else -> throw IllegalStateException("Unhandled view in onClick()")
        }
    }

    companion object {
        const val FRAGMENT_TAG = "tab_sheet"

        private const val ANIMATION_DURATION = 200
    }
}
