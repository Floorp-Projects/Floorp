/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import android.app.Dialog
import android.content.DialogInterface
import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.addCallback
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.rememberNestedScrollInteropConnection
import androidx.navigation.fragment.NavHostFragment.Companion.findNavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.isScreenReaderEnabled
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.shopping.di.ReviewQualityCheckMiddlewareProvider
import org.mozilla.fenix.shopping.store.BottomSheetDismissSource
import org.mozilla.fenix.shopping.store.BottomSheetViewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore
import org.mozilla.fenix.shopping.ui.ReviewQualityCheckBottomSheet
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A bottom sheet fragment displaying Review Quality Check information.
 */
class ReviewQualityCheckFragment : BottomSheetDialogFragment() {

    private var behavior: BottomSheetBehavior<View>? = null
    private val bottomSheetStateFeature =
        ViewBoundFeatureWrapper<ReviewQualityCheckBottomSheetStateFeature>()
    private val store by lazyStore { viewModelScope ->
        ReviewQualityCheckStore(
            middleware = ReviewQualityCheckMiddlewareProvider.provideMiddleware(
                settings = requireComponents.settings,
                browserStore = requireComponents.core.store,
                appStore = requireComponents.appStore,
                context = requireContext().applicationContext,
                scope = viewModelScope,
            ),
        )
    }
    private var dismissSource: BottomSheetDismissSource =
        BottomSheetDismissSource.CLICK_OUTSIDE
    private val bottomSheetCallback = object : BottomSheetBehavior.BottomSheetCallback() {
        override fun onStateChanged(bottomSheet: View, newState: Int) {
            // The bottom sheet enters STATE_HIDDEN when onDismiss is called. Because this is not
            // the case when the user clicks outside, we should check for any other dismissal sources.
            if (newState == BottomSheetBehavior.STATE_HIDDEN &&
                dismissSource == BottomSheetDismissSource.CLICK_OUTSIDE
            ) {
                dismissSource = BottomSheetDismissSource.SLIDE
            }
        }

        override fun onSlide(bottomSheet: View, slideOffset: Float) {
            // no-op
        }
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
                val bottomSheet =
                    findViewById<View?>(com.google.android.material.R.id.design_bottom_sheet)
                bottomSheet?.setBackgroundResource(android.R.color.transparent)
                behavior = BottomSheetBehavior.from(bottomSheet).apply {
                    addBottomSheetCallback(bottomSheetCallback)
                    setPeekHeightToHalfScreenHeight()
                }
            }
            (this as? BottomSheetDialog)?.onBackPressedDispatcher?.addCallback(this@ReviewQualityCheckFragment) {
                dismissSource = BottomSheetDismissSource.BACK_PRESSED
                findNavController(this@ReviewQualityCheckFragment).navigateUp()
            }
        }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        behavior?.setPeekHeightToHalfScreenHeight()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setContent {
            FirefoxTheme {
                ReviewQualityCheckBottomSheet(
                    store = store,
                    onRequestDismiss = {
                        dismissSource = it
                        behavior?.state = BottomSheetBehavior.STATE_HIDDEN
                    },
                    modifier = Modifier.nestedScroll(rememberNestedScrollInteropConnection()),
                )
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        behavior?.removeBottomSheetCallback(bottomSheetCallback)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        bottomSheetStateFeature.set(
            feature = ReviewQualityCheckBottomSheetStateFeature(
                store,
                requireContext().isScreenReaderEnabled,
            ) { bottomSheetState ->
                if (bottomSheetState == BottomSheetViewState.FULL_VIEW) {
                    behavior?.state = BottomSheetBehavior.STATE_EXPANDED
                }
                store.dispatch(
                    ReviewQualityCheckAction.BottomSheetDisplayed(bottomSheetState),
                )
            },
            owner = viewLifecycleOwner,
            view = view,
        )
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        requireComponents.appStore.dispatch(AppAction.ShoppingAction.ShoppingSheetStateUpdated(expanded = false))
        store.dispatch(ReviewQualityCheckAction.BottomSheetClosed(dismissSource))
    }

    private fun BottomSheetBehavior<View>.setPeekHeightToHalfScreenHeight() {
        peekHeight = resources.displayMetrics.heightPixels / 2
    }
}
