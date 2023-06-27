/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.privacy

import android.content.Context
import android.view.View
import android.widget.FrameLayout
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.view.isVisible
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import mozilla.components.browser.icons.IconRequest
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.focus.R
import org.mozilla.focus.cookiebannerreducer.CookieBannerReducerItem
import org.mozilla.focus.cookiebannerreducer.CookieBannerReducerStore
import org.mozilla.focus.databinding.DialogTrackingProtectionSheetBinding
import org.mozilla.focus.engine.EngineSharedPreferencesListener.TrackerChanged
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.installedDate
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ui.theme.FocusTheme

@SuppressWarnings("LongParameterList")
class TrackingProtectionPanel(
    context: Context,
    private val lifecycleOwner: LifecycleOwner,
    private val cookieBannerReducerStore: CookieBannerReducerStore,
    private val tabUrl: String,
    private val blockedTrackersCount: Int,
    private val isTrackingProtectionOn: Boolean,
    private val isConnectionSecure: Boolean,
    private val toggleTrackingProtection: (Boolean) -> Unit,
    private val updateTrackingProtectionPolicy: (String?, Boolean) -> Unit,
    private val showConnectionInfo: () -> Unit,
    private val showCookieBannerExceptionsDetailsPanel: () -> Unit,
) : BottomSheetDialog(context) {

    private var binding: DialogTrackingProtectionSheetBinding =
        DialogTrackingProtectionSheetBinding.inflate(layoutInflater, null, false)

    init {
        initWindow()
        setContentView(binding.root)
        expand()
        updateTitle()
        updateConnectionState()
        updateTrackingProtection()
        updateTrackersBlocked()
        updateTrackersState()
        updateCookieBannerException()
        setListeners()
    }

    private fun initWindow() {
        this.window?.decorView?.let {
            it.setViewTreeLifecycleOwner(lifecycleOwner)
            it.setViewTreeSavedStateRegistryOwner(
                lifecycleOwner as SavedStateRegistryOwner,
            )
        }
    }

    private fun expand() {
        val bottomSheet =
            findViewById<View>(com.google.android.material.R.id.design_bottom_sheet) as FrameLayout
        BottomSheetBehavior.from(bottomSheet).state = BottomSheetBehavior.STATE_EXPANDED
    }

    private fun updateTitle() {
        binding.siteTitle.text = tabUrl.tryGetHostFromUrl()
        context.components.icons.loadIntoView(
            binding.siteFavicon,
            IconRequest(tabUrl, isPrivate = true),
        )
    }

    private fun updateCookieBannerException() {
        binding.cookieBannerException.apply {
            setContent {
                FocusTheme {
                    val cookieBannerExceptionStatus =
                        cookieBannerReducerStore.observeAsComposableState { state ->
                            state.cookieBannerReducerStatus
                        }.value
                    val shouldShowCookieBannerItem =
                        cookieBannerReducerStore.observeAsComposableState { state ->
                            state.shouldShowCookieBannerItem
                        }.value
                    if (shouldShowCookieBannerItem == true) {
                        binding.cookieBannerException.visibility = View.VISIBLE
                    } else {
                        binding.cookieBannerException.visibility = View.GONE
                    }

                    if (cookieBannerExceptionStatus != null) {
                        CookieBannerReducerItem(
                            cookieBannerReducerStatus = cookieBannerExceptionStatus,
                            preferenceOnClickListener = ::showCookieBannerExceptionsDetailsPanel.invoke(),
                        )
                    }
                }
            }
            isTransitionGroup = true
        }
    }

    private fun updateConnectionState() {
        binding.securityInfo.text = if (isConnectionSecure) {
            context.getString(R.string.secure_connection)
        } else {
            context.getString(R.string.insecure_connection)
        }

        val nextIcon = AppCompatResources.getDrawable(context, R.drawable.mozac_ic_chevron_right_24)

        val securityIcon = if (isConnectionSecure) {
            AppCompatResources.getDrawable(context, R.drawable.mozac_ic_lock_24)
        } else {
            AppCompatResources.getDrawable(context, R.drawable.mozac_ic_warning_fill_24)
        }

        binding.securityInfo.putCompoundDrawablesRelativeWithIntrinsicBounds(
            start = securityIcon,
            end = nextIcon,
            top = null,
            bottom = null,
        )
    }

    private fun updateTrackingProtection() {
        val description = if (isTrackingProtectionOn) {
            context.getString(R.string.enhanced_tracking_protection_state_on)
        } else {
            context.getString(R.string.enhanced_tracking_protection_state_off)
        }

        val icon = if (isTrackingProtectionOn) {
            R.drawable.mozac_ic_shield_24
        } else {
            R.drawable.mozac_ic_shield_slash_24
        }

        val iconContentDescription = context.getString(R.string.enhanced_tracking_protection)
        binding.enhancedTracking.apply {
            updateDescription(description)
            updateIcon(icon = icon, iconContentDescription = iconContentDescription)
            binding.switchWidget.isChecked = isTrackingProtectionOn
        }
    }

    private fun updateTrackersBlocked() {
        binding.trackersCount.text = blockedTrackersCount.toString()
        binding.trackersCountNote.text =
            context.getString(R.string.trackers_count_note, context.installedDate)
    }

    private fun updateTrackersState() {
        val settings = context.settings

        with(binding) {
            advertising.isVisible = isTrackingProtectionOn
            analytics.isVisible = isTrackingProtectionOn
            social.isVisible = isTrackingProtectionOn
            content.isVisible = isTrackingProtectionOn
            trackersAndScriptsHeading.isVisible = isTrackingProtectionOn

            advertising.isChecked = settings.shouldBlockAdTrackers()
            analytics.isChecked = settings.shouldBlockAnalyticTrackers()
            social.isChecked = settings.shouldBlockSocialTrackers()
            content.isChecked = settings.shouldBlockOtherTrackers()
        }
    }

    private fun setListeners() {
        with(binding) {
            enhancedTracking.binding.switchWidget.setOnCheckedChangeListener { _, isChecked ->
                toggleTrackingProtection.invoke(isChecked)
                dismiss()
            }
            advertising.onClickListener {
                updateTrackingProtectionPolicy(
                    TrackerChanged.ADVERTISING.tracker,
                    advertising.isChecked,
                )
            }

            analytics.onClickListener {
                updateTrackingProtectionPolicy(
                    TrackerChanged.ANALYTICS.tracker,
                    analytics.isChecked,
                )
            }

            social.onClickListener {
                updateTrackingProtectionPolicy(TrackerChanged.SOCIAL.tracker, social.isChecked)
            }

            content.onClickListener {
                updateTrackingProtectionPolicy(TrackerChanged.CONTENT.tracker, content.isChecked)
            }

            securityInfo.setOnClickListener {
                showConnectionInfo.invoke()
            }
        }
    }
}
