/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.tips.providers

import android.content.Context
import android.content.Intent
import android.net.Uri
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MozillaProductDetector
import org.mozilla.fenix.components.metrics.MozillaProductDetector.MozillaProducts.FENIX
import org.mozilla.fenix.components.metrics.MozillaProductDetector.MozillaProducts.FENIX_NIGHTLY
import org.mozilla.fenix.components.metrics.MozillaProductDetector.MozillaProducts.FIREFOX_NIGHTLY
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.components.tips.TipProvider
import org.mozilla.fenix.components.tips.TipType
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.SupportUtils

/**
 * Tip explaining to users the migration of Fenix channels
 */
class MigrationTipProvider(private val context: Context) : TipProvider {

    override val tip: Tip? =
        when (context.packageName) {
            FENIX.productName -> firefoxPreviewMovedTip()
            FIREFOX_NIGHTLY.productName -> getNightlyMigrationTip()
            FENIX_NIGHTLY.productName -> getNightlyMigrationTip()
            else -> null
        }

    override val shouldDisplay: Boolean = context.settings().shouldDisplayFenixMovingTip()

    private fun firefoxPreviewMovedTip(): Tip =
        Tip(
            type = TipType.Button(
                text = context.getString(R.string.tip_firefox_preview_moved_button_2),
                action = ::getFirefoxMovedButtonAction
            ),
            identifier = getIdentifier(),
            title = context.getString(R.string.tip_firefox_preview_moved_header),
            description = context.getString(R.string.tip_firefox_preview_moved_description),
            learnMoreURL = SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.FENIX_MOVING)
        )

    private fun firefoxPreviewMovedPreviewInstalledTip(): Tip =
        Tip(
            type = TipType.Button(
                text = context.getString(R.string.tip_firefox_preview_moved_button_preview_installed),
                action = ::getFirefoxMovedButtonAction
            ),
            identifier = getIdentifier(),
            title = context.getString(R.string.tip_firefox_preview_moved_header_preview_installed),
            description = context.getString(R.string.tip_firefox_preview_moved_description_preview_installed),
            learnMoreURL = SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.FENIX_MOVING)
        )

    private fun firefoxPreviewMovedPreviewNotInstalledTip(): Tip =
        Tip(
            type = TipType.Button(
                text = context.getString(R.string.tip_firefox_preview_moved_button_preview_not_installed),
                action = ::getFirefoxMovedButtonAction
            ),
            identifier = getIdentifier(),
            title = context.getString(R.string.tip_firefox_preview_moved_header_preview_not_installed),
            description = context.getString(R.string.tip_firefox_preview_moved_description_preview_not_installed),
            learnMoreURL = SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.FENIX_MOVING)
        )

    private fun getNightlyMigrationTip(): Tip? {
        return if (MozillaProductDetector.packageIsInstalled(context, FENIX.productName)) {
            firefoxPreviewMovedPreviewInstalledTip()
        } else {
            firefoxPreviewMovedPreviewNotInstalledTip()
        }
    }

    private fun getFirefoxMovedButtonAction() {
        when (context.packageName) {
            FENIX.productName -> context.startActivity(
                Intent(Intent.ACTION_VIEW, Uri.parse(SupportUtils.FIREFOX_BETA_PLAY_STORE_URL))
            )
            FIREFOX_NIGHTLY.productName -> getNightlyMigrationAction()
            FENIX_NIGHTLY.productName -> getNightlyMigrationAction()
            else -> { }
        }
    }

    private fun getNightlyMigrationAction() {
        return if (MozillaProductDetector.packageIsInstalled(context, FENIX.productName)) {
            context.startActivity(context.packageManager.getLaunchIntentForPackage(FENIX.productName))
        } else {
            context.startActivity(Intent(
                Intent.ACTION_VIEW, Uri.parse(SupportUtils.FIREFOX_NIGHTLY_PLAY_STORE_URL)
            ))
        }
    }

    private fun getIdentifier(): String {
        return when (context.packageName) {
            FENIX.productName -> context.getString(R.string.pref_key_migrating_from_fenix_tip)
            FIREFOX_NIGHTLY.productName -> context.getString(R.string.pref_key_migrating_from_firefox_nightly_tip)
            FENIX_NIGHTLY.productName -> context.getString(R.string.pref_key_migrating_from_fenix_nightly_tip)
            else -> { "" }
        }
    }
}
