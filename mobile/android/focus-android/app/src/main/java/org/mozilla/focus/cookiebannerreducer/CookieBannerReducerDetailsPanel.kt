/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerreducer

import android.content.Context
import android.view.View
import android.widget.FrameLayout
import androidx.core.net.toUri
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.support.ktx.kotlin.toShortUrl
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.CookieBanner
import org.mozilla.focus.R
import org.mozilla.focus.databinding.CookieBannerReducerDetailsBinding
import org.mozilla.focus.ext.components

/**
 * Cookie banner reducer details panel that will be visible when the user
 * clicks on cookie banner reducer item.
 */
class CookieBannerReducerDetailsPanel(
    context: Context,
    cookieBannerReducerStore: CookieBannerReducerStore,
    private val ioScope: CoroutineScope,
    private val tabUrl: String,
    private val goBack: () -> Unit,
    private val defaultCookieBannerInteractor: DefaultCookieBannerReducerInteractor,
) : BottomSheetDialog(context) {

    private var binding: CookieBannerReducerDetailsBinding =
        CookieBannerReducerDetailsBinding.inflate(layoutInflater, null, false)
    private val cookieBannerExceptionStatus =
        cookieBannerReducerStore.state.cookieBannerReducerStatus
    private var siteDomain: String? = null

    init {
        setContentView(binding.root)
        initSiteDomain()
        expandBottomSheet()
        setListeners()
        bindSwitchItem()
        updateViews()
    }

    private fun initSiteDomain() {
        ioScope.launch {
            val host = tabUrl.toUri().host.orEmpty()
            siteDomain = context.components.publicSuffixList.getPublicSuffixPlusOne(host).await()
        }
    }

    private fun updateViews() {
        bindTitle()
        bindDescription()
        bindItemAction()
    }

    private fun expandBottomSheet() {
        val bottomSheet =
            findViewById<View>(R.id.design_bottom_sheet) as FrameLayout
        BottomSheetBehavior.from(bottomSheet).state = BottomSheetBehavior.STATE_EXPANDED
    }

    private fun updateSwitchItem() {
        binding.cookieBannerExceptionDetailsSwitch.visibility = View.VISIBLE
        binding.cookieBannerExceptionDetailsSwitch.binding.switchWidget.setOnClickListener {
            val isChecked =
                binding.cookieBannerExceptionDetailsSwitch.binding.switchWidget.isChecked
            defaultCookieBannerInteractor.handleToggleCookieBannerException(isChecked)
            updateViews()
            dismiss()
        }
    }

    private fun bindItemAction() {
        when (cookieBannerExceptionStatus) {
            CookieBannerReducerStatus.HasException -> {
                binding.cookieBannerExceptionDetailsSwitch.binding.description.text =
                    context.getString(R.string.cookie_banner_exception_panel_switch_state_off)
                updateSwitchItem()
            }
            CookieBannerReducerStatus.NoException -> {
                binding.cookieBannerExceptionDetailsSwitch.binding.description.text =
                    context.getString(R.string.cookie_banner_exception_panel_switch_state_on)
                updateSwitchItem()
            }
            CookieBannerReducerStatus.CookieBannerSiteNotSupported -> updateSiteNotSupportedItem()
            else -> {}
        }
    }

    private fun updateSiteNotSupportedItem() {
        binding.requestSupport.visibility = View.VISIBLE
        binding.cancelButton.visibility = View.VISIBLE
    }

    private fun bindTitle() {
        val titleText = when (cookieBannerExceptionStatus) {
            CookieBannerReducerStatus.HasException -> {
                setExceptionTitle(
                    siteDomain,
                    R.string.cookie_banner_exception_panel_title_state_on_for_site,
                )
            }
            CookieBannerReducerStatus.NoException -> {
                setExceptionTitle(
                    siteDomain,
                    R.string.cookie_banner_exception_panel_title_state_off_for_site,
                )
            }
            CookieBannerReducerStatus.CookieBannerSiteNotSupported -> {
                context.getString(R.string.cookie_banner_exception_panel_switch_title)
            }
            else -> ""
        }
        binding.title.text = titleText
    }

    private fun bindDescription() {
        val detailsText = when (cookieBannerExceptionStatus) {
            CookieBannerReducerStatus.HasException -> context.getString(
                R.string.cookie_banner_exception_panel_description_state_off_for_site2,
                context.getString(R.string.app_name),
            )
            CookieBannerReducerStatus.CookieBannerSiteNotSupported -> context.getString(
                R.string.cookie_banner_exception_panel_description_site_is_not_supported,
            )
            CookieBannerReducerStatus.NoException -> context.getString(
                R.string.cookie_banner_exception_panel_description_state_on_for_site,
                context.getString(R.string.app_name),
            )
            else -> ""
        }
        binding.details.text = detailsText
    }

    private fun setListeners() {
        binding.detailsBack.setOnClickListener {
            goBack.invoke()
            dismiss()
        }
        binding.cancelButton.setOnClickListener {
            CookieBanner.reportSiteCancelButton.record(NoExtras())
            goBack.invoke()
            dismiss()
        }
        binding.requestSupport.setOnClickListener {
            if (!siteDomain.isNullOrEmpty()) {
                defaultCookieBannerInteractor.handleRequestReportSiteDomain(siteDomain!!)
            }
            dismiss()
        }
    }

    private fun setExceptionTitle(domain: String?, titleRes: Int): String {
        val data = domain ?: tabUrl
        val shortUrl = data.toShortUrl(context.components.publicSuffixList)
        return context.getString(
            titleRes,
            shortUrl,
        )
    }

    private fun bindSwitchItem() {
        when (cookieBannerExceptionStatus) {
            CookieBannerReducerStatus.HasException -> {
                binding.cookieBannerExceptionDetailsSwitch.binding.switchWidget.isChecked = false
            }
            CookieBannerReducerStatus.NoException -> {
                binding.cookieBannerExceptionDetailsSwitch.binding.switchWidget.isChecked = true
            }
            else -> {
            }
        }
    }
}
