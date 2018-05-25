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
import mozilla.components.support.ktx.android.view.dp

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
            ToolbarConfiguration.SEEDLING -> setupSeedlingToolbar()
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

        toolbar.url = "https://www.mozilla.org/en-US/firefox/"
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
        toolbar.addBrowserAction(reload)

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

        toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"
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

        toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"
    }

    /**
     * A large dark toolbar with padding, flexible space and branding.
     */
    private fun setupSeedlingToolbar() {
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Setup background and size/padding
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.setBackgroundColor(0xFF38383D.toInt())
        toolbar.layoutParams.height = toolbar.dp(104)
        toolbar.setPadding(
                toolbar.dp(58),
                toolbar.dp(24),
                toolbar.dp(58),
                toolbar.dp(24))

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Hide the site security icon and set background to be drawn behind URL (+ page actions)
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.displaySiteSecurityIcon = false

        toolbar.setUrlTextPadding(
                left = toolbar.dp(16),
                right = toolbar.dp(16)
        )

        toolbar.urlBackgroundDrawable = getDrawable(R.drawable.sample_url_background)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add navigation actions
        ////////////////////////////////////////////////////////////////////////////////////////////

        val grid = Toolbar.Action(
                mozilla.components.ui.icons.R.drawable.mozac_ic_grid,
                "Grid") {
            simulateReload(toolbar)
        }

        toolbar.addNavigationAction(grid)

        val back = Toolbar.Action(
                mozilla.components.ui.icons.R.drawable.mozac_ic_back,
                "Back") {
            simulateReload(toolbar)
        }

        toolbar.addNavigationAction(back)

        val forward = Toolbar.Action(
                mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
                "Forward") {
            simulateReload(toolbar)
        }

        toolbar.addNavigationAction(forward)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add a page action for reload and two browser actions
        ////////////////////////////////////////////////////////////////////////////////////////////

        val reload = Toolbar.Action(
                mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
                "Reload") {
            simulateReload(toolbar)
        }

        toolbar.addPageAction(reload)

        val pin = Toolbar.Action(
                mozilla.components.ui.icons.R.drawable.mozac_ic_pin,
                "Pin") {
            simulateReload(toolbar)
        }

        toolbar.addBrowserAction(pin)

        val turbo = Toolbar.Action(
                mozilla.components.ui.icons.R.drawable.mozac_ic_rocket,
                "Turbo") {
            simulateReload(toolbar)
        }

        toolbar.addBrowserAction(turbo)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.url = "https://www.nytimes.com/video"
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
