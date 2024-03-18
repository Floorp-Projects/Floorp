/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.style.UnderlineSpan
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.infobanner.InfoBanner
import org.mozilla.fenix.databinding.ComponentTabstray2Binding
import org.mozilla.fenix.databinding.OnboardingInactiveTabsCfrBinding
import org.mozilla.fenix.ext.potentialInactiveTabs
import org.mozilla.fenix.utils.Settings

@OptIn(ExperimentalCoroutinesApi::class)
class TabsTrayInactiveTabsOnboardingBinding(
    private val context: Context,
    private val store: BrowserStore,
    private val tabsTrayBinding: ComponentTabstray2Binding?,
    private val settings: Settings,
    private val navigationInteractor: NavigationInteractor,
) : AbstractBinding<BrowserState>(store) {

    private lateinit var inactiveTabsDialog: Dialog

    @VisibleForTesting
    internal var banner: InfoBanner? = null

    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.map { state -> state.normalTabs.size }
            .distinctUntilChanged()
            .collect {
                val inactiveTabsList =
                    if (settings.inactiveTabsAreEnabled) { store.state.potentialInactiveTabs } else { emptyList() }
                if (inactiveTabsList.isNotEmpty() && shouldShowOnboardingForInactiveTabs()) {
                    createInactiveCFR()
                }
            }
    }

    private fun shouldShowOnboardingForInactiveTabs() =
        settings.shouldShowInactiveTabsOnboardingPopup &&
            settings.canShowCfr

    private fun createInactiveCFR() {
        // Don't create a new dialog if one is already displayed
        if (this::inactiveTabsDialog.isInitialized) return

        val context: Context = context
        val anchorPosition = IntArray(2)
        val popupBinding = OnboardingInactiveTabsCfrBinding.inflate(LayoutInflater.from(context))
        inactiveTabsDialog = Dialog(context)

        inactiveTabsDialog.apply {
            setContentView(popupBinding.root)
            setCancelable(false)
            // removing title or setting it as an empty string does not prevent a11y services from assigning one
            setTitle(" ")
        }
        popupBinding.closeInfoBanner.setOnClickListener {
            inactiveTabsDialog.dismiss()
            settings.shouldShowInactiveTabsOnboardingPopup = false
            TabsTray.inactiveTabsCfrDismissed.record(NoExtras())
        }

        popupBinding.bannerInfoMessage.setOnClickListener {
            inactiveTabsDialog.dismiss()
            settings.shouldShowInactiveTabsOnboardingPopup = false
            navigationInteractor.onTabSettingsClicked()
            TabsTray.inactiveTabsCfrSettings.record(NoExtras())
        }

        val messageText = context.getString(R.string.tab_tray_inactive_onboarding_message)
        val actionText = context.getString(R.string.tab_tray_inactive_onboarding_button_text)
        val spannableStringBuilder = SpannableStringBuilder(messageText)

        spannableStringBuilder.append(" ")
            .append(actionText, UnderlineSpan(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        popupBinding.bannerInfoMessage.text = spannableStringBuilder

        tabsTrayBinding?.tabsTray?.getLocationOnScreen(anchorPosition)

        val (x, y) = anchorPosition

        if (x == 0 && y == 0) {
            return
        }

        popupBinding.root.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED)

        inactiveTabsDialog.window?.apply {
            val attr = attributes
            setGravity(Gravity.START or Gravity.TOP)
            attr.x = x + 15.dpToPx(context.resources.displayMetrics)
            attr.y = y + 20.dpToPx(context.resources.displayMetrics)
            attributes = attr
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        }
        inactiveTabsDialog.show()
        TabsTray.inactiveTabsCfrVisible.record(NoExtras())
    }
}
