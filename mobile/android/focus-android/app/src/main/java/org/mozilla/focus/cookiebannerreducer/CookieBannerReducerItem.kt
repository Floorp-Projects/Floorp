/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerreducer

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.focus.R
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun CookieBannerReducerItemPreviewSiteIsNotSupported() {
    FocusTheme {
        CookieBannerReducerItem(cookieBannerReducerStatus = CookieBannerReducerStatus.CookieBannerSiteNotSupported) {}
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun CookieBannerReducerItemPreviewHasException() {
    FocusTheme {
        CookieBannerReducerItem(cookieBannerReducerStatus = CookieBannerReducerStatus.HasException) {}
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun CookieBannerReducerItemPreviewHasNotException() {
    FocusTheme {
        CookieBannerReducerItem(cookieBannerReducerStatus = CookieBannerReducerStatus.NoException) {}
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun CookieBannerReducerItemPreviewUnsupportedSiteRequestWasSubmitted() {
    FocusTheme {
        CookieBannerReducerItem(
            cookieBannerReducerStatus =
            CookieBannerReducerStatus.CookieBannerUnsupportedSiteRequestWasSubmitted,
        ) {}
    }
}

/**
 * Displays the cookie banner exception item from Tracking Protection panel.
 *
 * @param cookieBannerReducerStatus if the site has a cookie banner, an exception or is not supported
 * @param preferenceOnClickListener Callback that will redirect the user to cookie banner item details.
 */
@Composable
fun CookieBannerReducerItem(
    cookieBannerReducerStatus: CookieBannerReducerStatus,
    preferenceOnClickListener: (() -> Unit)? = null,
) {
    var rowModifier = Modifier
        .defaultMinSize(minHeight = 48.dp)
        .background(
            colorResource(R.color.settings_background),
            shape = RectangleShape,
        )

    if (cookieBannerReducerStatus !is CookieBannerReducerStatus.CookieBannerUnsupportedSiteRequestWasSubmitted
    ) {
        rowModifier = rowModifier.then(
            Modifier.clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
            ) { preferenceOnClickListener?.invoke() },
        )
    }

    Row(
        modifier = rowModifier,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val painter =
            if (cookieBannerReducerStatus is CookieBannerReducerStatus.NoException) {
                painterResource(id = R.drawable.mozac_ic_cookies_24)
            } else {
                painterResource(id = R.drawable.ic_cookies_disable)
            }
        Icon(
            painter = painter,
            contentDescription = null,
            tint = focusColors.onPrimary,
            modifier = Modifier.padding(end = 20.dp),
        )

        Column(
            modifier = Modifier.weight(1f),
        ) {
            Text(
                text = stringResource(R.string.cookie_banner_exception_item_title),
                maxLines = 1,
                color = focusColors.settingsTextColor,
                fontSize = 14.sp,
                lineHeight = 20.sp,
            )
            val summary = when (cookieBannerReducerStatus) {
                CookieBannerReducerStatus.HasException ->
                    stringResource(id = R.string.cookie_banner_exception_item_description_state_off)
                CookieBannerReducerStatus.NoException ->
                    stringResource(id = R.string.cookie_banner_exception_item_description_state_on)
                CookieBannerReducerStatus.CookieBannerSiteNotSupported ->
                    stringResource(id = R.string.cookie_banner_exception_site_not_supported)
                CookieBannerReducerStatus.CookieBannerUnsupportedSiteRequestWasSubmitted ->
                    stringResource(id = R.string.cookie_banner_the_site_was_reported)
            }
            Text(
                text = summary,
                maxLines = 1,
                color = colorResource(R.color.disabled),
                fontSize = 12.sp,
                lineHeight = 16.sp,
            )
        }
        if (
            cookieBannerReducerStatus !is CookieBannerReducerStatus.CookieBannerUnsupportedSiteRequestWasSubmitted
        ) {
            Icon(
                modifier = Modifier
                    .padding(end = 0.dp)
                    .size(24.dp),
                tint = focusColors.onPrimary,
                painter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
                contentDescription = null,
            )
        }
    }
}
