/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
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
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.core.view.isVisible
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.databinding.QuicksettingsProtectionsPanelBinding
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.trackingprotection.CookieBannerUIMode
import org.mozilla.fenix.trackingprotection.ProtectionsState
import org.mozilla.fenix.utils.Settings

/**
 * MVI View that displays the tracking protection, cookie banner handling toggles and the navigation
 * to additional tracking protection details.
 *
 * @property containerView [ViewGroup] in which this View will inflate itself.
 * @param trackingProtectionDivider trackingProtectionDivider The divider line between tracking protection layout
 * and other views from [QuickSettingsSheetDialogFragment].
 * @property interactor [ProtectionsInteractor] which will have delegated to all user interactions.
 * @property settings [Settings] application settings.
 */
class ProtectionsView(
    val containerView: ViewGroup,
    private val trackingProtectionDivider: View,
    val interactor: ProtectionsInteractor,
    val settings: Settings,
) {

    /**
     * Allows changing what this View displays.
     */
    fun update(state: ProtectionsState) {
        bindTrackingProtectionInfo(state.isTrackingProtectionEnabled)
        bindCookieBannerProtection(state.cookieBannerUIMode)
        binding.trackingProtectionSwitch.isVisible = settings.shouldUseTrackingProtection
        binding.cookieBannerItem.isVisible = shouldShowCookieBanner &&
            state.cookieBannerUIMode != CookieBannerUIMode.HIDE

        binding.trackingProtectionDetails.setOnClickListener {
            interactor.onTrackingProtectionDetailsClicked()
        }
        updateDividerVisibility()
    }

    private fun updateDividerVisibility() {
        trackingProtectionDivider.isVisible = !(
            !binding.trackingProtectionSwitch.isVisible &&
                !binding.trackingProtectionDetails.isVisible &&
                !binding.cookieBannerItem.isVisible
            )
    }

    @VisibleForTesting
    internal fun updateDetailsSection(show: Boolean) {
        binding.trackingProtectionDetails.isVisible = show
        updateDividerVisibility()
    }

    private fun bindTrackingProtectionInfo(isTrackingProtectionEnabled: Boolean) {
        binding.trackingProtectionSwitch.isChecked = isTrackingProtectionEnabled
        binding.trackingProtectionSwitch.setOnCheckedChangeListener { _, isChecked ->
            interactor.onTrackingProtectionToggled(isChecked)
        }
    }

    @VisibleForTesting
    internal val binding = QuicksettingsProtectionsPanelBinding.inflate(
        LayoutInflater.from(containerView.context),
        containerView,
        true,
    )

    private val shouldShowCookieBanner: Boolean
        get() = settings.shouldShowCookieBannerUI && settings.shouldUseCookieBanner

    private fun bindCookieBannerProtection(cookieBannerMode: CookieBannerUIMode) {
        val context = binding.cookieBannerItem.context
        val label = context.getString(R.string.preferences_cookie_banner_reduction)

        binding.cookieBannerItem.apply {
            setContent {
                FirefoxTheme {
                    if (cookieBannerMode == CookieBannerUIMode.REQUEST_UNSUPPORTED_SITE_SUBMITTED) {
                        CookieBannerItem(
                            label = label,
                            cookieBannerUIMode = cookieBannerMode,
                        )
                    } else {
                        CookieBannerItem(
                            label = label,
                            cookieBannerUIMode = cookieBannerMode,
                            endIconPainter = painterResource(R.drawable.ic_arrowhead_right),
                            onClick = { interactor.onCookieBannerHandlingDetailsClicked() },
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun CookieBannerItem(
    label: String,
    cookieBannerUIMode: CookieBannerUIMode,
    endIconPainter: Painter? = null,
    onClick: (() -> Unit)? = null,
) {
    var rowModifier = Modifier
        .defaultMinSize(minHeight = 48.dp)
        .padding(horizontal = 16.dp)

    onClick?.let {
        rowModifier = rowModifier.then(
            Modifier.clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
            ) {
                onClick.invoke()
            },
        )
    }

    Row(
        modifier = rowModifier,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        cookieBannerUIMode.icon?.let {
            Icon(
                painter = painterResource(it),
                contentDescription = null,
                modifier = Modifier.padding(horizontal = 0.dp),
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }

        Column(
            modifier = Modifier
                .padding(horizontal = 8.dp, vertical = 6.dp)
                .weight(1f),
        ) {
            Text(
                text = label,
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.subtitle1,
                maxLines = 1,
            )
            cookieBannerUIMode.description?.let {
                Text(
                    text = stringResource(it),
                    color = FirefoxTheme.colors.textSecondary,
                    style = FirefoxTheme.typography.body2,
                    maxLines = 1,
                )
            }
        }
        endIconPainter?.let {
            Icon(
                modifier = Modifier
                    .padding(end = 0.dp)
                    .size(24.dp),
                painter = it,
                contentDescription = null,
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun CookieBannerItemPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            CookieBannerItem(
                label = "Cookie Banner Reduction",
                cookieBannerUIMode = CookieBannerUIMode.ENABLE,
                endIconPainter = painterResource(R.drawable.ic_arrowhead_right),
                onClick = { println("list item click") },
            )
        }
    }
}
