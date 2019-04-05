/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.core.widget.NestedScrollView
import com.google.android.material.bottomsheet.BottomSheetBehavior
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.layout_quick_action_sheet.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.mvi.UIView

class QuickActionUIView(
    container: ViewGroup,
    actionEmitter: Observer<QuickActionAction>,
    changesObservable: Observable<QuickActionChange>
) : UIView<QuickActionState, QuickActionAction, QuickActionChange>(container, actionEmitter, changesObservable) {

    override val view: NestedScrollView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_quick_action_sheet, container, true)
        .findViewById(R.id.nestedScrollQuickAction) as NestedScrollView

    init {
        val quickActionSheetBehavior =
            BottomSheetBehavior.from(nestedScrollQuickAction as View) as QuickActionSheetBehavior

        quickActionSheetBehavior.setBottomSheetCallback(object : BottomSheetBehavior.BottomSheetCallback() {
            override fun onStateChanged(v: View, state: Int) {
                updateImportantForAccessibility(state)

                if (state == BottomSheetBehavior.STATE_EXPANDED) {
                    actionEmitter.onNext(QuickActionAction.Opened)
                } else if (state == BottomSheetBehavior.STATE_COLLAPSED) {
                    actionEmitter.onNext(QuickActionAction.Closed)
                }
            }

            override fun onSlide(p0: View, p1: Float) {
            }
        })

        updateImportantForAccessibility(quickActionSheetBehavior.state)

        view.quick_action_share.setOnClickListener {
            actionEmitter.onNext(QuickActionAction.SharePressed)
            quickActionSheetBehavior.state = BottomSheetBehavior.STATE_COLLAPSED
        }
        view.quick_action_downloads.setOnClickListener {
            actionEmitter.onNext(QuickActionAction.DownloadsPressed)
            quickActionSheetBehavior.state = BottomSheetBehavior.STATE_COLLAPSED
        }
        view.quick_action_bookmark.setOnClickListener {
            actionEmitter.onNext(QuickActionAction.BookmarkPressed)
            quickActionSheetBehavior.state = BottomSheetBehavior.STATE_COLLAPSED
        }
        view.quick_action_read.setOnClickListener {
            actionEmitter.onNext(QuickActionAction.ReadPressed)
            quickActionSheetBehavior.state = BottomSheetBehavior.STATE_COLLAPSED
        }
    }

    private fun updateImportantForAccessibility(state: Int) {
        view.findViewById<LinearLayout>(R.id.quick_action_buttons_layout).importantForAccessibility =
                if (state == BottomSheetBehavior.STATE_COLLAPSED || state == BottomSheetBehavior.STATE_HIDDEN)
                    View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                else
                    View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
    }

    private fun sendTelemetryEvent(state: Int) {
        when (state) {
            BottomSheetBehavior.STATE_EXPANDED ->
                view.context.components.analytics.metrics.track(Event.QuickActionSheetOpened)
            BottomSheetBehavior.STATE_COLLAPSED ->
                view.context.components.analytics.metrics.track(Event.QuickActionSheetClosed)
        }
    }

    override fun updateView() = Consumer<QuickActionState> {
        view.quick_action_read.visibility = if (it.readable) View.VISIBLE else View.GONE
    }
}
