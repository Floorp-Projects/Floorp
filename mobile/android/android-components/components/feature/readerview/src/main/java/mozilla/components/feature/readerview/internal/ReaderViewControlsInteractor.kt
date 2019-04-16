/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.readerview.internal

import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.ReaderViewFeature.Config.ColorScheme
import mozilla.components.feature.readerview.ReaderViewFeature.Config.FontType
import mozilla.components.feature.readerview.view.ReaderViewControlsView

/**
 * Interactor that implements [ReaderViewControlsView.Listener] and notifies the feature about actions the user
 * performed via the [ReaderViewFeature.Config] (e.g. "font changed").
 */
internal class ReaderViewControlsInteractor(
    private val view: ReaderViewControlsView,
    private val config: ReaderViewFeature.Config
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

    override fun onFontSizeIncreased() {
        config.fontSize += 1
    }

    override fun onFontSizeDecreased() {
        config.fontSize -= 1
    }

    override fun onColorSchemeChanged(scheme: ColorScheme) {
        config.colorScheme = scheme
    }
}
