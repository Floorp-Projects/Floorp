/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import android.content.Intent
import android.speech.RecognizerIntent
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.IncompleteRedesignToolbarFeature
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.FragmentHomeBinding
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.toolbar.ToolbarInteractor
import org.mozilla.fenix.search.ExtraAction
import org.mozilla.fenix.utils.ToolbarPopupWindow
import java.lang.ref.WeakReference

/**
 * View class for setting up the home screen toolbar.
 */
class ToolbarView(
    private val binding: FragmentHomeBinding,
    private val context: Context,
    private val interactor: ToolbarInteractor,
    private val searchEngine: SearchEngine?,
) {
    init {
        updateLayout(binding.root)
    }

    /**
     * Setups the home screen toolbar.
     */
    fun build() {
        binding.toolbar.compoundDrawablePadding =
            context.resources.getDimensionPixelSize(R.dimen.search_bar_search_engine_icon_padding)

        binding.toolbarWrapper.setOnClickListener {
            interactor.onNavigateSearch()
        }

        binding.qrActionImage.setOnClickListener {
            interactor.onNavigateSearch(ExtraAction.QR_READER)
        }

        binding.microphoneActionImage.setOnClickListener {
            interactor.onNavigateSearch(ExtraAction.VOICE_SEARCH)
        }

        binding.toolbarWrapper.setOnLongClickListener {
            ToolbarPopupWindow.show(
                WeakReference(it),
                handlePasteAndGo = interactor::onPasteAndGo,
                handlePaste = interactor::onPaste,
                copyVisible = false,
            )
            true
        }
    }

    @Suppress("LongMethod")
    private fun updateLayout(view: View) {
        val speechIntent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)

        when (IncompleteRedesignToolbarFeature(context.settings()).isEnabled) {
            true -> {
                binding.menuButton.isVisible = false
                binding.tabButton.isVisible = false
                binding.qrActionImage.isVisible =
                    searchEngine?.isGeneral == true || searchEngine?.type == SearchEngine.Type.CUSTOM
                binding.microphoneActionImage.isVisible =
                    speechIntent.resolveActivity(context.packageManager) != null &&
                    context.settings().shouldShowVoiceSearch
                binding.browserActionSeparator.isVisible =
                    binding.qrActionImage.isVisible || binding.microphoneActionImage.isVisible
            }

            false -> {
                binding.menuButton.isVisible = true
                binding.tabButton.isVisible = true
                binding.browserActionSeparator.isVisible = false
                binding.qrActionImage.isVisible = false
                binding.microphoneActionImage.isVisible = false
            }
        }

        when (context.settings().toolbarPosition) {
            ToolbarPosition.TOP -> {
                binding.toolbarLayout.layoutParams = CoordinatorLayout.LayoutParams(
                    ConstraintLayout.LayoutParams.MATCH_PARENT,
                    ConstraintLayout.LayoutParams.WRAP_CONTENT,
                ).apply {
                    gravity = Gravity.TOP
                }

                val isTabletAndTabStripEnabled = context.settings().isTabletAndTabStripEnabled
                ConstraintSet().apply {
                    clone(binding.toolbarLayout)
                    clear(binding.bottomBar.id, ConstraintSet.BOTTOM)
                    clear(binding.bottomBarShadow.id, ConstraintSet.BOTTOM)

                    if (isTabletAndTabStripEnabled) {
                        connect(
                            binding.bottomBar.id,
                            ConstraintSet.TOP,
                            binding.tabStripView.id,
                            ConstraintSet.BOTTOM,
                        )
                    } else {
                        connect(
                            binding.bottomBar.id,
                            ConstraintSet.TOP,
                            ConstraintSet.PARENT_ID,
                            ConstraintSet.TOP,
                        )
                    }
                    connect(
                        binding.bottomBarShadow.id,
                        ConstraintSet.TOP,
                        binding.bottomBar.id,
                        ConstraintSet.BOTTOM,
                    )
                    connect(
                        binding.bottomBarShadow.id,
                        ConstraintSet.BOTTOM,
                        ConstraintSet.PARENT_ID,
                        ConstraintSet.BOTTOM,
                    )
                    applyTo(binding.toolbarLayout)
                }

                binding.bottomBar.background = AppCompatResources.getDrawable(
                    view.context,
                    view.context.theme.resolveAttribute(R.attr.bottomBarBackgroundTop),
                )

                binding.homeAppBar.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                    topMargin =
                        context.resources.getDimensionPixelSize(R.dimen.home_fragment_top_toolbar_header_margin) +
                        if (isTabletAndTabStripEnabled) {
                            context.resources.getDimensionPixelSize(R.dimen.tab_strip_height)
                        } else {
                            0
                        }
                }
            }

            ToolbarPosition.BOTTOM -> {}
        }

        binding.toolbarWrapper.updateLayoutParams<ViewGroup.MarginLayoutParams> {
            rightMargin = if (IncompleteRedesignToolbarFeature(context.settings()).isEnabled) {
                context.resources.getDimensionPixelSize(R.dimen.home_fragment_toolbar_margin)
            } else {
                0
            }
        }
    }
}
