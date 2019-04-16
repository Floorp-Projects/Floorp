/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.readerview.internal

import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.view.ReaderViewControlsView

/**
 * Presenter implementation that will update the view whenever the feature is started.
 */
internal class ReaderViewControlsPresenter(
    private val view: ReaderViewControlsView,
    private val config: ReaderViewFeature.Config
) {
    /**
     * Sets the initial state of the ReaderView controls and makes the controls visible.
     */
    fun show() {
        view.apply {
            setColorScheme(config.colorScheme)
            setFont(config.fontType)
            setFontSize(config.fontSize)
            showControls()
        }
    }

    /**
     * Hides the controls.
     */
    fun hide() {
        view.hideControls()
    }
}
