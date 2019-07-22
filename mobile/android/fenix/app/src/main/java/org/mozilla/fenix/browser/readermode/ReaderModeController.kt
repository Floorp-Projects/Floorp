/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.readermode

import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper

/**
 * An interface that exposes the hide and show reader view functions of a ReaderViewFeature
 */
interface ReaderModeController {
    fun hideReaderView()
    fun showReaderView()
    fun showControls()
}

class DefaultReaderModeController(
    private val readerViewFeature: ViewBoundFeatureWrapper<ReaderViewFeature>
) : ReaderModeController {
    override fun hideReaderView() {
        readerViewFeature.withFeature { it.hideReaderView() }
    }

    override fun showReaderView() {
        readerViewFeature.withFeature { it.showReaderView() }
    }

    override fun showControls() {
        readerViewFeature.withFeature { it.showControls() }
    }
}
