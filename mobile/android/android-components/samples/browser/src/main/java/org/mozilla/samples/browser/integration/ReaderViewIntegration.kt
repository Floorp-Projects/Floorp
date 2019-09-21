/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.integration

import android.content.Context
import androidx.core.content.ContextCompat
import com.google.android.material.floatingactionbutton.FloatingActionButton
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.samples.browser.R

class ReaderViewIntegration(
    context: Context,
    engine: Engine,
    sessionManager: SessionManager,
    toolbar: BrowserToolbar,
    view: ReaderViewControlsView,
    readerViewAppearanceButton: FloatingActionButton
) : LifecycleAwareFeature, BackHandler {

    private var readerViewButtonVisible = false

    private val readerViewButton: BrowserToolbar.ToggleButton = BrowserToolbar.ToggleButton(
        image = context.getDrawable(R.drawable.mozac_ic_reader_mode)!!,
        imageSelected = context.getDrawable(R.drawable.mozac_ic_reader_mode)!!.mutate().apply {
            setTint(ContextCompat.getColor(context, R.color.photonBlue40))
        },
        contentDescription = context.getString(R.string.mozac_reader_view_description),
        contentDescriptionSelected = context.getString(R.string.mozac_reader_view_description_selected),
        selected = sessionManager.selectedSession?.readerMode ?: false,
        visible = { readerViewButtonVisible }
    ) { enabled ->
        if (enabled) {
            feature.showReaderView()
            readerViewAppearanceButton.show()
        } else {
            feature.hideReaderView()
            feature.hideControls()
            readerViewAppearanceButton.hide()
        }
    }

    init {
        toolbar.addPageAction(readerViewButton)
        readerViewAppearanceButton.setOnClickListener { feature.showControls() }
    }

    private val feature = ReaderViewFeature(context, engine, sessionManager, view) { available ->
        readerViewButtonVisible = available

        // We've got an update on reader view availability e.g. because the page
        // was refreshed or a new session selected. Let's make sure to also update
        // the selected state of the reader mode toolbar button and show the
        // appearance controls button if needed.
        val readerModeActive = sessionManager.selectedSession?.readerMode ?: false
        readerViewButton.setSelected(readerModeActive)

        if (readerModeActive) readerViewAppearanceButton.show() else readerViewAppearanceButton.hide()

        toolbar.invalidateActions()
    }

    override fun start() {
        feature.start()
    }

    override fun stop() {
        feature.stop()
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }
}
