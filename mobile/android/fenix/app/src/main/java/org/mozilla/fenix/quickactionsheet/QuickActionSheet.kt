/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.content.Context
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.LinearLayout
import androidx.core.widget.NestedScrollView
import com.google.android.material.bottomsheet.BottomSheetBehavior
import kotlinx.android.synthetic.main.layout_quick_action_sheet.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Settings

const val POSITION_SNAP_BUFFER = 1f

class QuickActionSheet @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
    defStyleRes: Int = 0
) : LinearLayout(context, attrs, defStyle, defStyleRes) {

    private val scope = MainScope()

    private lateinit var quickActionSheetBehavior: QuickActionSheetBehavior<NestedScrollView>

    init {
        inflate(context, R.layout.layout_quick_action_sheet, this)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        quickActionSheetBehavior =
            QuickActionSheetBehavior.from(quick_action_sheet.parent as NestedScrollView)
        quickActionSheetBehavior.isHideable = false
        setupHandle()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        scope.cancel()
    }

    private fun setupHandle() {
        quick_action_sheet_handle.setOnClickListener {
            quickActionSheetBehavior.state = when (quickActionSheetBehavior.state) {
                BottomSheetBehavior.STATE_EXPANDED -> BottomSheetBehavior.STATE_COLLAPSED
                else -> BottomSheetBehavior.STATE_EXPANDED
            }
        }

        quick_action_sheet_handle.setAccessibilityDelegate(HandleAccessibilityDelegate(quickActionSheetBehavior))
    }

    fun bounceSheet() {
        Settings.getInstance(context).incrementAutomaticBounceQuickActionSheetCount()
        scope.launch(Dispatchers.Main) {
            delay(BOUNCE_ANIMATION_DELAY_LENGTH)
            quickActionSheetBehavior.state = BottomSheetBehavior.STATE_EXPANDED
            delay(BOUNCE_ANIMATION_PAUSE_LENGTH)
            quickActionSheetBehavior.state = BottomSheetBehavior.STATE_COLLAPSED
        }
    }

    class HandleAccessibilityDelegate(
        private val quickActionSheetBehavior: QuickActionSheetBehavior<NestedScrollView>
    ) : View.AccessibilityDelegate() {
        private var finalState = BottomSheetBehavior.STATE_COLLAPSED
            get() = when (quickActionSheetBehavior.state) {
                BottomSheetBehavior.STATE_EXPANDED,
                BottomSheetBehavior.STATE_HIDDEN,
                BottomSheetBehavior.STATE_COLLAPSED -> {
                    quickActionSheetBehavior.state
                }
                else -> field
            }
            set(value) {
                field = value
                quickActionSheetBehavior.state = value
            }

        override fun performAccessibilityAction(host: View?, action: Int, args: Bundle?): Boolean {
            finalState = when (action) {
                AccessibilityNodeInfo.ACTION_CLICK ->
                    when (quickActionSheetBehavior.state) {
                        BottomSheetBehavior.STATE_EXPANDED -> BottomSheetBehavior.STATE_COLLAPSED
                        else -> BottomSheetBehavior.STATE_EXPANDED
                    }
                AccessibilityNodeInfo.ACTION_COLLAPSE ->
                    BottomSheetBehavior.STATE_COLLAPSED
                AccessibilityNodeInfo.ACTION_EXPAND ->
                    BottomSheetBehavior.STATE_EXPANDED
                else -> return super.performAccessibilityAction(host, action, args)
            }

            host?.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_CLICKED)

            return true
        }

        override fun onInitializeAccessibilityNodeInfo(host: View?, info: AccessibilityNodeInfo?) {
            super.onInitializeAccessibilityNodeInfo(host, info)
            info?.addAction(
                when (finalState) {
                    BottomSheetBehavior.STATE_COLLAPSED,
                    BottomSheetBehavior.STATE_HIDDEN -> AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND
                    else -> AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE
                }
            )
        }
    }

    companion object {
        const val BOUNCE_ANIMATION_DELAY_LENGTH = 1000L
        const val BOUNCE_ANIMATION_PAUSE_LENGTH = 2000L
    }
}
