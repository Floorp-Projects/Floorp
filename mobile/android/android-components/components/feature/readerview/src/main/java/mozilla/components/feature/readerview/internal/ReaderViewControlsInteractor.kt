/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview.internal

import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.ReaderViewFeature.ColorScheme
import mozilla.components.feature.readerview.ReaderViewFeature.FontType
import mozilla.components.feature.readerview.view.MAX_TEXT_SIZE
import mozilla.components.feature.readerview.view.MIN_TEXT_SIZE
import mozilla.components.feature.readerview.view.ReaderViewControlsView

/**
 * Interactor that implements [ReaderViewControlsView.Listener] and notifies the feature about actions the user
 * performed via the [ReaderViewFeature.Config] (e.g. "font changed").
 */
internal class ReaderViewControlsInteractor(
    private val view: ReaderViewControlsView,
    private val config: ReaderViewConfig,
) : ReaderViewControlsView.Listener {
    fun start() {
        view.listener = this
    }

    fun stop() {
        view.listener = null
    }

    override fun onFontChanged(font: FontType) {
        config.fontType = font
    }

    override fun onFontSizeIncreased(): Int {
        if (config.fontSize < MAX_TEXT_SIZE) {
            config.fontSize += 1
        }
        return config.fontSize
    }

    override fun onFontSizeDecreased(): Int {
        if (config.fontSize > MIN_TEXT_SIZE) {
            config.fontSize -= 1
        }
        return config.fontSize
    }

    override fun onColorSchemeChanged(scheme: ColorScheme) {
        config.colorScheme = scheme
    }
}
