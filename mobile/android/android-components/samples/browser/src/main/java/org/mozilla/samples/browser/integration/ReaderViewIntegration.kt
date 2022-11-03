/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.integration

import android.content.Context
import android.graphics.drawable.Drawable
import androidx.core.content.ContextCompat
import com.google.android.material.floatingactionbutton.FloatingActionButton
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.samples.browser.R
import mozilla.components.ui.colors.R as colorsR
import mozilla.components.ui.icons.R as iconsR

@Suppress("UndocumentedPublicClass")
class ReaderViewIntegration(
    context: Context,
    engine: Engine,
    store: BrowserStore,
    toolbar: BrowserToolbar,
    view: ReaderViewControlsView,
    readerViewAppearanceButton: FloatingActionButton,
) : LifecycleAwareFeature, UserInteractionHandler {

    private var readerViewButtonVisible = false

    private val readerViewButton: BrowserToolbar.ToggleButton = BrowserToolbar.ToggleButton(
        image = getReaderDrawable(context),
        imageSelected = getReaderDrawable(context).mutate().apply {
            setTint(ContextCompat.getColor(context, colorsR.color.photonBlue40))
        },
        contentDescription = context.getString(R.string.mozac_reader_view_description),
        contentDescriptionSelected = context.getString(R.string.mozac_reader_view_description_selected),
        selected = store.state.selectedTab?.readerState?.active ?: false,
        visible = { readerViewButtonVisible },
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

    private val feature = ReaderViewFeature(context, engine, store, view) { available, active ->
        readerViewButtonVisible = available
        readerViewButton.setSelected(active)

        if (active) readerViewAppearanceButton.show() else readerViewAppearanceButton.hide()
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

private fun getReaderDrawable(context: Context): Drawable {
    val drawable = iconsR.drawable.mozac_ic_reader_mode
    return ContextCompat.getDrawable(context, drawable)!!
}
