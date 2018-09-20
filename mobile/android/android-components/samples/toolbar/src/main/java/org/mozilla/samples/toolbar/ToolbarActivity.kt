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
import kotlinx.coroutines.experimental.Job
import kotlinx.coroutines.experimental.android.UI
import kotlinx.coroutines.experimental.delay
import kotlinx.coroutines.experimental.launch
import mozilla.components.browser.domains.DomainAutoCompleteProvider
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.ktx.android.content.res.pxToDp
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText

/**
 * This sample application shows how to use and customize the browser-toolbar component.
 */
class ToolbarActivity : AppCompatActivity() {
    private val autoCompleteProvider: DomainAutoCompleteProvider = DomainAutoCompleteProvider()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        autoCompleteProvider.initialize(this)

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

        toolbar.setAutocompleteFilter { value, view ->
            view?.let {
                val result = autoCompleteProvider.autocomplete(value)
                view.applyAutocompleteResult(
                        InlineAutocompleteEditText.AutocompleteResult(result.text, result.source, result.size, { result.url }))
            }
        }
    }

    override fun onPause() {
        super.onPause()

        toolbar.hideKeyboard()
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
        // Add "back" and "forward" navigation actions
        ////////////////////////////////////////////////////////////////////////////////////////////

        val back = BrowserToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            "Back") {
            simulateReload()
        }

        toolbar.addNavigationAction(back)

        val forward = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
                "Forward") {
            simulateReload()
        }

        toolbar.addNavigationAction(forward)

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
        toolbar.layoutParams.height = toolbar.resources.pxToDp(104)
        toolbar.setPadding(
                resources.pxToDp(58),
                resources.pxToDp(24),
                resources.pxToDp(58),
                resources.pxToDp(24))

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Hide the site security icon and set padding around the URL
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.displaySiteSecurityIcon = false

        toolbar.setUrlTextPadding(
                left = resources.pxToDp(16),
                right = resources.pxToDp(16)
        )

        toolbar.browserActionMargin = toolbar.resources.pxToDp(16)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add a custom "URL box" (url + page actions) background view that also acts as a custom
        // progress bar.
        ////////////////////////////////////////////////////////////////////////////////////////////

        val urlBoxProgress = UrlBoxProgressView(this)

        toolbar.urlBoxView = urlBoxProgress
        toolbar.urlBoxMargin = toolbar.resources.pxToDp(16)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add navigation actions
        ////////////////////////////////////////////////////////////////////////////////////////////

        val grid = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_grid,
                "Grid",
                background = R.drawable.button_background) {
            // Do nothing
        }

        toolbar.addNavigationAction(grid)

        val back = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_back,
                "Back",
                visible = ::canGoBack,
                background = R.drawable.button_background) {
            goBack()
            simulateReload(urlBoxProgress)
            toolbar.invalidateActions()
        }

        toolbar.addNavigationAction(back)

        val forward = BrowserToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
                "Forward",
                visible = ::canGoForward,
                background = R.drawable.button_background) {
            goForward()
            simulateReload(urlBoxProgress)
            toolbar.invalidateActions()
        }

        toolbar.addNavigationAction(forward)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add a custom page action for reload and two browser actions
        ////////////////////////////////////////////////////////////////////////////////////////////

        val reload = ReloadPageAction(
            reloadImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            reloadContentDescription = "Reload",
            stopImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            stopContentDescription = "Stop",
            isLoading = { loading },
            background = R.drawable.pageaction_background
        ) {
            if (loading) {
                job?.cancel()
            } else {
                simulateReload(urlBoxProgress)
            }
        }

        toolbar.addPageAction(reload)

        val pin = BrowserToolbar.ToggleButton(
            imageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_pin,
            imageResourceSelected = mozilla.components.ui.icons.R.drawable.mozac_ic_pin_filled,
            contentDescription = "Pin",
            contentDescriptionSelected = "Unpin",
            background = R.drawable.toggle_background) {
            // Do nothing
        }

        toolbar.addBrowserAction(pin)

        val turbo = BrowserToolbar.ToggleButton(
            imageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_rocket,
            imageResourceSelected = mozilla.components.ui.icons.R.drawable.mozac_ic_rocket_filled,
            contentDescription = "Turbo: Off",
            contentDescriptionSelected = "Turbo: On",
            background = R.drawable.toggle_background) {
            // Do nothing
        }

        toolbar.addBrowserAction(turbo)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Add a fixed size element for spacing and a branding image
        ////////////////////////////////////////////////////////////////////////////////////////////

        val space = Toolbar.ActionSpace(toolbar.resources.pxToDp(128))

        toolbar.addBrowserAction(space)

        val brand = Toolbar.ActionImage(R.drawable.brand)

        toolbar.addBrowserAction(brand)

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Intercept clicks on the URL to not open editing mode
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.onUrlClicked = {
            // Return false to not go to editing mode and then handle the event manually here...
            true
        }

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL and set custom text to be displayed if no URL is set
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.hint = "Search or enter address"
        toolbar.url = "https://www.nytimes.com/video"

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Whenever an URL was entered.. pretend to load it and re-insert it into the toolbar
        ////////////////////////////////////////////////////////////////////////////////////////////

        toolbar.setOnUrlCommitListener { url ->
            simulateReload(urlBoxProgress)

            if (url.isNotEmpty()) {
                toolbar.url = url
            }
        }
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

    private var job: Job? = null

    private var loading: Boolean = false

    private fun simulateReload(view: UrlBoxProgressView? = null) {
        job?.cancel()

        loading = true

        job = launch(UI) {
            try {
                loop@ for (progress in 0..100 step 5) {
                    if (!isActive) {
                        break@loop
                    }

                    if (view == null) {
                        toolbar.displayProgress(progress)
                    } else {
                        view.progress = progress
                    }

                    delay(progress * 5)
                }
            } catch (t: Throwable) {
                if (view == null) {
                    toolbar.displayProgress(0)
                } else {
                    view.progress = 0
                }

                throw t
            } finally {
                loading = false

                // Update toolbar buttons to reflect loading state
                toolbar.invalidateActions()
            }
        }

        // Update toolbar buttons to reflect loading state
        toolbar.invalidateActions()
    }
}
