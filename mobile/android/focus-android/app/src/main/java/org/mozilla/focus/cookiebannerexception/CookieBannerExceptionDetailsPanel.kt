/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerexception

import android.content.Context
import android.view.View
import android.widget.FrameLayout
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import mozilla.components.support.ktx.kotlin.toShortUrl
import org.mozilla.focus.R
import org.mozilla.focus.databinding.CookieBannerExceptionDetailsBinding
import org.mozilla.focus.ext.components

class CookieBannerExceptionDetailsPanel(
    context: Context,
    cookieBannerExceptionStore: CookieBannerExceptionStore,
    private val tabUrl: String,
    private val goBack: () -> Unit,
    private val defaultCookieBannerInteractor: DefaultCookieBannerExceptionInteractor,
) : BottomSheetDialog(context) {

    private var binding: CookieBannerExceptionDetailsBinding =
        CookieBannerExceptionDetailsBinding.inflate(layoutInflater, null, false)

    init {
        setContentView(binding.root)
        expandBottomSheet()
        setListeners()
        updateViews(cookieBannerExceptionStore.state.hasException)
        bindSwitchItem(cookieBannerExceptionStore.state.hasException)
        updateSwitchItem()
    }

    private fun updateViews(hasException: Boolean) {
        bindTitle(hasException)
        bindDescription(hasException)
        bindSwitchItemDescription(hasException)
    }

    private fun expandBottomSheet() {
        val bottomSheet =
            findViewById<View>(R.id.design_bottom_sheet) as FrameLayout
        BottomSheetBehavior.from(bottomSheet).state = BottomSheetBehavior.STATE_EXPANDED
    }

    private fun updateSwitchItem() {
        binding.cookieBannerExceptionDetailsSwitch.binding.switchWidget.setOnClickListener {
            val isChecked = binding.cookieBannerExceptionDetailsSwitch.binding.switchWidget.isChecked
            defaultCookieBannerInteractor.handleToggleCookieBannerException(isChecked)
            updateViews(!isChecked)
        }
    }

    private fun bindSwitchItem(hasException: Boolean) {
        binding.cookieBannerExceptionDetailsSwitch.binding.switchWidget.isChecked = !hasException
    }

    private fun bindSwitchItemDescription(hasException: Boolean) {
        val stringID =
            if (hasException) {
                R.string.cookie_banner_exception_panel_switch_state_off
            } else {
                R.string.cookie_banner_exception_panel_switch_state_on
            }
        binding.cookieBannerExceptionDetailsSwitch.binding.description.text = context.getString(stringID)
    }

    private fun bindTitle(hasException: Boolean) {
        val stringID =
            if (hasException) {
                R.string.cookie_banner_exception_panel_title_state_on_for_site
            } else {
                R.string.cookie_banner_exception_panel_title_state_off_for_site
            }
        val shortUrl = tabUrl.toShortUrl(context.components.publicSuffixList)
        binding.title.text = context.getString(stringID, shortUrl)
    }

    private fun bindDescription(hasException: Boolean) {
        val stringID =
            if (hasException) {
                R.string.cookie_banner_exception_panel_description_state_off_for_site
            } else {
                R.string.cookie_banner_exception_panel_description_state_on_for_site
            }
        binding.details.text = context.getString(stringID, context.getString(R.string.app_name))
    }

    private fun setListeners() {
        binding.detailsBack.setOnClickListener {
            goBack.invoke()
            dismiss()
        }
    }
}
