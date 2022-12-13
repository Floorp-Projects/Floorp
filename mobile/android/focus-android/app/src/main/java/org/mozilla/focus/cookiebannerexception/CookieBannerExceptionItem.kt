/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerexception

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
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
private fun CookieBannerExceptionItemPreviewHasException() {
    FocusTheme {
        CookieBannerExceptionItem(true) {}
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun CookieBannerExceptionItemPreviewHasNotException() {
    FocusTheme {
        CookieBannerExceptionItem(false) {}
    }
}

/**
 * Displays the cookie banner exception item from Tracking Protection panel.
 *
 * @param hasException if the site hasException
 * @param preferenceOnClickListener Callback that will redirect the user to cookie banner item details.
 */
@Composable
fun CookieBannerExceptionItem(hasException: Boolean, preferenceOnClickListener: () -> Unit) {
    Row(
        modifier = Modifier
            .clickable { preferenceOnClickListener() }
            .defaultMinSize(minHeight = 48.dp)
            .background(
                colorResource(R.color.settings_background),
                shape = RectangleShape,
            ),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val painter = if (hasException) {
            painterResource(id = R.drawable.ic_cookies_disable)
        } else {
            painterResource(id = R.drawable.mozac_ic_cookies)
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
            val summary = if (hasException) {
                stringResource(id = R.string.cookie_banner_exception_item_description_state_off)
            } else {
                stringResource(id = R.string.cookie_banner_exception_item_description_state_on)
            }
            Text(
                text = summary,
                maxLines = 1,
                color = colorResource(R.color.disabled),
                fontSize = 12.sp,
                lineHeight = 16.sp,
            )
        }
        Icon(
            modifier = Modifier
                .padding(end = 0.dp)
                .size(24.dp),
            tint = focusColors.onPrimary,
            painter = painterResource(id = R.drawable.mozac_ic_arrowhead_right),
            contentDescription = null,
        )
    }
}
