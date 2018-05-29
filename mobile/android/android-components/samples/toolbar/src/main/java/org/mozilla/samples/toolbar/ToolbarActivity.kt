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

        val reload = BrowserToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            "Reload") {
            simulateReload()
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
            simulateReload()
        }

        val reload = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            "Reload") {
            simulateReload()
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
        // Hide the site security icon and set padding around the URL
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.displaySiteSecurityIcon = false

        toolbar.setUrlTextPadding(
                left = toolbar.dp(16),
                right = toolbar.dp(16)
        )

        toolbar.browserActionMargin = toolbar.dp(16)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add a custom "URL box" (url + page actions) background view that also acts as a custom
        // progress bar.
        ////////////////////////////////////////////////////////////////////////////////////////////

        val urlBoxProgress = UrlBoxProgressView(this)

        toolbar.urlBoxView = urlBoxProgress
        toolbar.urlBoxMargin = toolbar.dp(16)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add navigation actions
        ////////////////////////////////////////////////////////////////////////////////////////////

        val grid = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_grid,
                "Grid") {
            simulateReload(urlBoxProgress)
        }

        toolbar.addNavigationAction(grid)

        val back = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_back,
                "Back",
                visible = ::canGoBack) {
            goBack()
            simulateReload(urlBoxProgress)
            toolbar.invalidateActions()
        }

        toolbar.addNavigationAction(back)

        val forward = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
                "Forward",
                visible = ::canGoForward) {
            goForward()
            simulateReload(urlBoxProgress)
            toolbar.invalidateActions()
        }

        toolbar.addNavigationAction(forward)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add a page action for reload and two browser actions
        ////////////////////////////////////////////////////////////////////////////////////////////

        val reload = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
                "Reload") {
            simulateReload(urlBoxProgress)
        }

        toolbar.addPageAction(reload)

        val pin = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_pin,
                "Pin") {
            simulateReload(urlBoxProgress)
        }

        toolbar.addBrowserAction(pin)

        val turbo = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_rocket,
                "Turbo") {
            simulateReload(urlBoxProgress)
        }

        toolbar.addBrowserAction(turbo)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add a fixed size element for spacing and a branding image
        ////////////////////////////////////////////////////////////////////////////////////////////

        val space = Toolbar.ActionSpace(toolbar.dp(128))

        toolbar.addBrowserAction(space)

        val brand = Toolbar.ActionImage(R.drawable.brand)

        toolbar.addBrowserAction(brand)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.url = "https://www.nytimes.com/video"
    }

    // For testing purposes
    private var forward = true
    private var back = true

    private fun canGoForward(): Boolean = forward
    private fun canGoBack(): Boolean = back

    private fun goBack() {
        back = !(forward && back)
        forward = true
    }

    private fun goForward() {
        forward = !(back && forward)
        back = true
    }

    private fun simulateReload(view: UrlBoxProgressView? = null) {
        launch(UI) {
            for (progress in 0..100 step 10) {
                if (view == null) {
                    toolbar.displayProgress(progress)
                } else {
                    view.progress = progress
                }

                delay(progress * 10)
            }
        }
    }
}
