/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.toolbar

import android.os.Bundle
import android.support.v4.content.ContextCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.LinearLayoutManager
import android.view.View
import kotlinx.android.synthetic.main.activity_toolbar.*
import kotlinx.coroutines.experimental.android.UI
import kotlinx.coroutines.experimental.delay
import kotlinx.coroutines.experimental.launch
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar

/**
 * This sample application shows how to use and customize the browser-toolbar component.
 */
class ToolbarActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_toolbar)

        val configuration = getToolbarConfiguration(intent)

        when (configuration) {
            ToolbarConfiguration.DEFAULT -> setupDefaultToolbar()
            ToolbarConfiguration.FOCUS_TABLET -> setupFocusTabletToolbar()
            ToolbarConfiguration.FOCUS_PHONE -> setupFocusPhoneToolbar()
        }

        recyclerView.adapter = ConfigurationAdapter(configuration)
        recyclerView.layoutManager = LinearLayoutManager(this, LinearLayoutManager.VERTICAL, false)
    }

    /**
     * A very simple toolbar with mostly default values.
     */
    private fun setupDefaultToolbar() {
        toolbar.setBackgroundColor(
                ContextCompat.getColor(this, mozilla.components.ui.colors.R.color.photonBlue80))

        toolbar.displayUrl("https://www.mozilla.org/en-US/firefox/")
    }

    /**
     * A toolbar that looks like Firefox Focus on tablets.
     */
    private fun setupFocusTabletToolbar() {
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Use the iconic gradient background
        ////////////////////////////////////////////////////////////////////////////////////////////

        val background = ContextCompat.getDrawable(this, R.drawable.focus_background)
        toolbar.background = background

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add a "reload" browser action that simulates reloading the current page
        ////////////////////////////////////////////////////////////////////////////////////////////

        val reload = Toolbar.Action(
            mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            "Reload") {
            simulateReload(toolbar)
        }
        toolbar.addDisplayAction(reload)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Create a menu that looks like the one in Firefox Focus
        ////////////////////////////////////////////////////////////////////////////////////////////

        val share = SimpleBrowserMenuItem("Share…") { /* Do nothing */ }
        val homeScreen = SimpleBrowserMenuItem("Add to Home screen") { /* Do nothing */ }
        val open = SimpleBrowserMenuItem("Open in…") { /* Do nothing */ }
        val settings = SimpleBrowserMenuItem("Settings") { /* Do nothing */ }

        val builder = BrowserMenuBuilder(listOf(share, homeScreen, open, settings))
        toolbar.setMenuBuilder(builder)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.displayUrl("https://www.mozilla.org/en-US/firefox/mobile/")
    }

    /**
     * A toolbar that looks like Firefox Focus on phones.
     */
    private fun setupFocusPhoneToolbar() {
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Use the iconic gradient background
        ////////////////////////////////////////////////////////////////////////////////////////////

        val background = ContextCompat.getDrawable(this, R.drawable.focus_background)
        toolbar.background = background

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Create a "mini" toolbar to be shown inside the menu (forward, reload)
        ////////////////////////////////////////////////////////////////////////////////////////////

        val forward = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            "Forward") {
            simulateReload(toolbar)
        }

        val reload = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            "Reload") {
            simulateReload(toolbar)
        }

        val menuToolbar = BrowserMenuItemToolbar(listOf(forward, reload))

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Create a custom "menu item" implementation that resembles Focus' global content blocking switch.
        ////////////////////////////////////////////////////////////////////////////////////////////

        val blocking = object : BrowserMenuItem {
            // Always display this item. This lambda is executed when the user clicks on the menu
            // button to determine whether this item should be shown.
            override val visible = { true }

            override fun getLayoutResource() = R.layout.focus_blocking_switch

            override fun bind(menu: BrowserMenu, view: View) {
                // Nothing to do here.
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Create a menu that looks like the one in Firefox Focus
        ////////////////////////////////////////////////////////////////////////////////////////////

        val share = SimpleBrowserMenuItem("Share…") { /* Do nothing */ }
        val homeScreen = SimpleBrowserMenuItem("Add to Home screen") { /* Do nothing */ }
        val open = SimpleBrowserMenuItem("Open in…") { /* Do nothing */ }
        val settings = SimpleBrowserMenuItem("Settings") { /* Do nothing */ }

        val builder = BrowserMenuBuilder(listOf(menuToolbar, blocking, share, homeScreen, open, settings))
        toolbar.setMenuBuilder(builder)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.displayUrl("https://www.mozilla.org/en-US/firefox/mobile/")
    }

    private fun simulateReload(toolbar: BrowserToolbar) {
        launch(UI) {
            for (progress in 0..100 step 10) {
                toolbar.displayProgress(progress)

                delay(progress * 10)
            }
        }
    }
}
