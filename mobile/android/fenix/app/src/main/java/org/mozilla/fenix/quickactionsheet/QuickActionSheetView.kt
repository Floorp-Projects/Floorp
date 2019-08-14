/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.DrawableRes
import androidx.core.content.edit
import androidx.core.view.isVisible
import androidx.core.widget.NestedScrollView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.layout_quick_action_sheet.*
import kotlinx.android.synthetic.main.layout_quick_action_sheet.view.*
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.BrowserState
import org.mozilla.fenix.utils.Settings

interface QuickActionSheetViewInteractor {
    fun onQuickActionSheetOpened()
    fun onQuickActionSheetClosed()
    fun onQuickActionSheetSharePressed()
    fun onQuickActionSheetDownloadPressed()
    fun onQuickActionSheetBookmarkPressed()
    fun onQuickActionSheetReadPressed()
    fun onQuickActionSheetAppearancePressed()
    fun onQuickActionSheetOpenLinkPressed()
}

/**
 * View for the quick action sheet that slides out from the toolbar.
 */
class QuickActionSheetView(
    override val containerView: ViewGroup,
    private val interactor: QuickActionSheetViewInteractor
) : LayoutContainer, View.OnClickListener {

    val view: NestedScrollView = LayoutInflater.from(containerView.context)
        .inflate(R.layout.component_quick_action_sheet, containerView, true)
        .findViewById(R.id.nestedScrollQuickAction)

    private val quickActionSheet = view.quick_action_sheet as QuickActionSheet
    private val quickActionSheetBehavior = QuickActionSheetBehavior.from(nestedScrollQuickAction)

    init {
        quickActionSheetBehavior.setQuickActionSheetCallback(object :
            QuickActionSheetBehavior.QuickActionSheetCallback {
            override fun onStateChanged(bottomSheet: View, newState: Int) {
                updateImportantForAccessibility(newState)

                if (newState == QuickActionSheetBehavior.STATE_EXPANDED) {
                    interactor.onQuickActionSheetOpened()
                } else if (newState == QuickActionSheetBehavior.STATE_COLLAPSED) {
                    interactor.onQuickActionSheetClosed()
                }
            }

            override fun onSlide(bottomSheet: View, slideOffset: Float) {
                animateOverlay(slideOffset)
            }
        })

        updateImportantForAccessibility(quickActionSheetBehavior.state)

        view.quick_action_share.setOnClickListener(this)
        view.quick_action_downloads.setOnClickListener(this)
        view.quick_action_bookmark.setOnClickListener(this)
        view.quick_action_read.setOnClickListener(this)
        view.quick_action_appearance.setOnClickListener(this)
        view.quick_action_open_app_link.setOnClickListener(this)
    }

    /**
     * Handles clicks from quick action buttons
     */
    override fun onClick(button: View) {
        when (button.id) {
            R.id.quick_action_share -> interactor.onQuickActionSheetSharePressed()
            R.id.quick_action_downloads -> interactor.onQuickActionSheetDownloadPressed()
            R.id.quick_action_bookmark -> interactor.onQuickActionSheetBookmarkPressed()
            R.id.quick_action_read -> interactor.onQuickActionSheetReadPressed()
            R.id.quick_action_appearance -> interactor.onQuickActionSheetAppearancePressed()
            R.id.quick_action_open_app_link -> interactor.onQuickActionSheetOpenLinkPressed()
            else -> return
        }
        quickActionSheetBehavior.state = QuickActionSheetBehavior.STATE_COLLAPSED
    }

    /**
     * Changes alpha of overlay based on new offset of this sheet within [-1,1] range.
     */
    private fun animateOverlay(offset: Float) {
        overlay.alpha = (1 - offset)
    }

    /**
     * Updates the important for accessibility flag on the buttons container,
     * depending on if the sheet is opened or closed.
     */
    private fun updateImportantForAccessibility(state: Int) {
        view.quick_action_buttons_layout.importantForAccessibility = when (state) {
            QuickActionSheetBehavior.STATE_COLLAPSED, QuickActionSheetBehavior.STATE_HIDDEN ->
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
            else ->
                View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
        }
    }

    fun update(state: BrowserState) {
        val quickActionSheetState = state.quickActionSheetState
        view.quick_action_read.isVisible = quickActionSheetState.readable
        view.quick_action_read.isSelected = quickActionSheetState.readerActive
        view.quick_action_read.text = view.context.getString(
            if (quickActionSheetState.readerActive) R.string.quick_action_read_close else R.string.quick_action_read
        )
        notifyReaderModeButton(quickActionSheetState.readable)

        view.quick_action_appearance.isVisible = quickActionSheetState.readerActive

        view.quick_action_bookmark.isSelected = quickActionSheetState.bookmarked
        view.quick_action_bookmark.text = view.context.getString(
            if (quickActionSheetState.bookmarked) {
                R.string.quick_action_bookmark_edit
            } else {
                R.string.quick_action_bookmark
            }
        )

        if (quickActionSheetState.bounceNeeded && Settings.getInstance(view.context).shouldAutoBounceQuickActionSheet) {
            quickActionSheet.bounceSheet()
        }

        view.quick_action_open_app_link.apply {
            visibility = if (quickActionSheetState.isAppLink) View.VISIBLE else View.GONE
        }
    }

    private fun notifyReaderModeButton(readable: Boolean) {
        val settings = Settings.getInstance(view.context).preferences
        val shouldNotifyKey = view.context.getString(R.string.pref_key_reader_mode_notification)

        @DrawableRes
        val readerTwoStateDrawableRes = if (readable && settings.getBoolean(shouldNotifyKey, true)) {
            quickActionSheet.bounceSheet()
            settings.edit { putBoolean(shouldNotifyKey, false) }
            R.drawable.reader_two_state_with_notification
        } else {
            R.drawable.reader_two_state
        }

        view.quick_action_read.putCompoundDrawablesRelativeWithIntrinsicBounds(
            top = view.context.getDrawable(readerTwoStateDrawableRes)
        )
    }
}
