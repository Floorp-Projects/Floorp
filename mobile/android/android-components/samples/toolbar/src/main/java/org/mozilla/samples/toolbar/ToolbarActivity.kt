/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.toolbar

import android.content.res.Resources
import android.graphics.Color
import android.os.Bundle
import android.view.View
import androidx.annotation.DrawableRes
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.Observer
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.activity_toolbar.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.autocomplete.CustomDomainsProvider
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.android.view.hideKeyboard

/**
 * This sample application shows how to use and customize the browser-toolbar component.
 */
@Suppress("TooManyFunctions", "LargeClass")
class ToolbarActivity : AppCompatActivity() {
    private val shippedDomainsProvider = ShippedDomainsProvider()
    private val customDomainsProvider = CustomDomainsProvider()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        shippedDomainsProvider.initialize(this)
        customDomainsProvider.initialize(this)

        setContentView(R.layout.activity_toolbar)

        val configuration = getToolbarConfiguration(intent)

        when (configuration) {
            ToolbarConfiguration.DEFAULT -> setupDefaultToolbar()
            ToolbarConfiguration.FOCUS_TABLET -> setupFocusTabletToolbar()
            ToolbarConfiguration.FOCUS_PHONE -> setupFocusPhoneToolbar()
            ToolbarConfiguration.SEEDLING -> setupSeedlingToolbar()
            ToolbarConfiguration.CUSTOM_MENU -> setupCustomMenu()
            ToolbarConfiguration.PRIVATE_MODE -> setupDefaultToolbar(private = true)
        }

        val recyclerView: RecyclerView = findViewById(R.id.recyclerView)
        recyclerView.adapter = ConfigurationAdapter(configuration)
        recyclerView.layoutManager = LinearLayoutManager(this, RecyclerView.VERTICAL, false)

        ToolbarAutocompleteFeature(toolbar).apply {
            this.addDomainProvider(shippedDomainsProvider)
            this.addDomainProvider(customDomainsProvider)
        }
    }

    override fun onPause() {
        super.onPause()

        toolbar.hideKeyboard()
    }

    /**
     * A very simple toolbar with mostly default values.
     */
    private fun setupDefaultToolbar(private: Boolean = false) {
        toolbar.setBackgroundColor(
                ContextCompat.getColor(this, mozilla.components.ui.colors.R.color.photonBlue80))

        toolbar.private = private

        toolbar.url = "https://www.mozilla.org/en-US/firefox/"
    }

    /**
     * A toolbar that looks like Firefox Focus on tablets.
     */
    private fun setupFocusTabletToolbar() {
        // //////////////////////////////////////////////////////////////////////////////////////////
        // Use the iconic gradient background
        // //////////////////////////////////////////////////////////////////////////////////////////

        val background = ContextCompat.getDrawable(this, R.drawable.focus_background)
        toolbar.background = background

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Add "back" and "forward" navigation actions
        // //////////////////////////////////////////////////////////////////////////////////////////

        val back = BrowserToolbar.Button(
            resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_back),
            "Back"
        ) {
            simulateReload()
        }

        toolbar.addNavigationAction(back)

        val forward = BrowserToolbar.Button(
            resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_forward),
            "Forward"
        ) {
            simulateReload()
        }

        toolbar.addNavigationAction(forward)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Add a "reload" browser action that simulates reloading the current page
        // //////////////////////////////////////////////////////////////////////////////////////////

        val reload = BrowserToolbar.Button(
            resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_refresh),
            "Reload") {
            simulateReload()
        }
        toolbar.addBrowserAction(reload)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Create a menu that looks like the one in Firefox Focus
        // //////////////////////////////////////////////////////////////////////////////////////////

        val fenix = SimpleBrowserMenuItem("POWERED BY MOZILLA")
        val share = SimpleBrowserMenuItem("Share…") { /* Do nothing */ }
        val homeScreen = SimpleBrowserMenuItem("Add to Home screen") { /* Do nothing */ }
        val open = SimpleBrowserMenuItem("Open in…") { /* Do nothing */ }
        val settings = SimpleBrowserMenuItem("Settings") { /* Do nothing */ }

        val builder = BrowserMenuBuilder(listOf(fenix, share, homeScreen, open, settings))
        toolbar.setMenuBuilder(builder)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"
    }

    /**
     * A custom browser menu.
     */
    private fun setupCustomMenu() {

        toolbar.siteSecurityColor = Color.TRANSPARENT to Color.TRANSPARENT
        toolbar.siteSecurityIcon = getDrawable(R.drawable.custom_security_icon)

        toolbar.setBackgroundColor(
            ContextCompat.getColor(this, mozilla.components.ui.colors.R.color.photonBlue80))

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Create a menu with text and icons
        // //////////////////////////////////////////////////////////////////////////////////////////

        val share = BrowserMenuImageText(
            "Share",
            R.drawable.mozac_ic_share
        ) { /* Do nothing */ }

        val search = BrowserMenuImageText(
            "Search",
            R.drawable.mozac_ic_search
        ) { /* Do nothing */ }

        val builder = BrowserMenuBuilder(listOf(share, BrowserMenuDivider(), search))
        toolbar.setMenuBuilder(builder)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.url = "https://www.mozilla.org/"
    }

    /**
     * A toolbar that looks like Firefox Focus on phones.
     */
    private fun setupFocusPhoneToolbar() {
        // //////////////////////////////////////////////////////////////////////////////////////////
        // Use the iconic gradient background
        // //////////////////////////////////////////////////////////////////////////////////////////

        val background = ContextCompat.getDrawable(this, R.drawable.focus_background)
        toolbar.background = background

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Create a "mini" toolbar to be shown inside the menu (forward, reload)
        // //////////////////////////////////////////////////////////////////////////////////////////

        val forward = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            "Forward",
            isEnabled = { canGoForward() }
        ) {
            simulateReload()
        }

        val reload = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            primaryContentDescription = "Reload",
            secondaryImageResource = R.drawable.mozac_ic_stop,
            secondaryContentDescription = "Stop",
            isInPrimaryState = { loading.value != true },
            disableInSecondaryState = false
        ) {
            if (loading.value == true) {
                job?.cancel()
            } else {
                simulateReload()
            }
        }
        // Redraw the reload button when loading state changes
        loading.observe(this, Observer { toolbar.invalidateActions() })

        val menuToolbar = BrowserMenuItemToolbar(listOf(forward, reload))

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Create a custom "menu item" implementation that resembles Focus' global content blocking switch.
        // //////////////////////////////////////////////////////////////////////////////////////////

        val blocking = object : BrowserMenuItem {
            // Always display this item. This lambda is executed when the user clicks on the menu
            // button to determine whether this item should be shown.
            override val visible = { true }

            override fun getLayoutResource() = R.layout.focus_blocking_switch

            override fun bind(menu: BrowserMenu, view: View) {
                // Nothing to do here.
            }
        }

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Create a menu that looks like the one in Firefox Focus
        // //////////////////////////////////////////////////////////////////////////////////////////

        val share = SimpleBrowserMenuItem("Share…") { /* Do nothing */ }
        val homeScreen = SimpleBrowserMenuItem("Add to Home screen") { /* Do nothing */ }
        val open = SimpleBrowserMenuItem("Open in…") { /* Do nothing */ }
        val settings = SimpleBrowserMenuItem("Settings") { /* Do nothing */ }

        val builder = BrowserMenuBuilder(listOf(menuToolbar, blocking, share, homeScreen, open, settings))
        toolbar.setMenuBuilder(builder)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"
    }

    /**
     * A large dark toolbar with padding, flexible space and branding.
     */
    @Suppress("LongMethod", "MagicNumber")
    private fun setupSeedlingToolbar() {
        // //////////////////////////////////////////////////////////////////////////////////////////
        // Setup background and size/padding
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.setBackgroundColor(0xFF38383D.toInt())
        toolbar.layoutParams.height = 104.dpToPx(resources.displayMetrics)
        toolbar.setPadding(
            58.dpToPx(resources.displayMetrics),
            24.dpToPx(resources.displayMetrics),
            58.dpToPx(resources.displayMetrics),
            24.dpToPx(resources.displayMetrics))

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Hide the site security icon and set padding around the URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.displaySiteSecurityIcon = false

        toolbar.setUrlTextPadding(
                left = 16.dpToPx(resources.displayMetrics),
                right = 16.dpToPx(resources.displayMetrics)
        )

        toolbar.browserActionMargin = 16.dpToPx(resources.displayMetrics)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Add a custom "URL box" (url + page actions) background view that also acts as a custom
        // progress bar.
        // //////////////////////////////////////////////////////////////////////////////////////////

        val urlBoxProgress = UrlBoxProgressView(this)

        toolbar.urlBoxView = urlBoxProgress
        toolbar.urlBoxMargin = 16.dpToPx(resources.displayMetrics)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Add navigation actions
        // //////////////////////////////////////////////////////////////////////////////////////////

        val grid = BrowserToolbar.Button(
            resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_grid),
                "Grid",
                background = R.drawable.button_background) {
            // Do nothing
        }

        toolbar.addNavigationAction(grid)

        val back = BrowserToolbar.Button(
            resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_back),
                "Back",
                visible = ::canGoBack,
                background = R.drawable.button_background) {
            goBack()
            simulateReload(urlBoxProgress)
            toolbar.invalidateActions()
        }

        toolbar.addNavigationAction(back)

        val forward = BrowserToolbar.Button(
            resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_forward),
                "Forward",
                visible = ::canGoForward,
                background = R.drawable.button_background) {
            goForward()
            simulateReload(urlBoxProgress)
            toolbar.invalidateActions()
        }

        toolbar.addNavigationAction(forward)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Add a custom page action for reload and two browser actions
        // //////////////////////////////////////////////////////////////////////////////////////////

        val reload = BrowserToolbar.TwoStateButton(
            enabledImage = resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_refresh),
            enabledContentDescription = "Reload",
            disabledImage = resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_stop),
            disabledContentDescription = "Stop",
            isEnabled = { loading.value == true },
            background = R.drawable.pageaction_background
        ) {
            if (loading.value == true) {
                job?.cancel()
            } else {
                simulateReload(urlBoxProgress)
            }
        }

        toolbar.addPageAction(reload)
        // Redraw the reload button when loading state changes
        loading.observe(this, Observer { toolbar.invalidateActions() })

        val pin = BrowserToolbar.ToggleButton(
            image = resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_pin),
            imageSelected = resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_pin),
            contentDescription = "Pin",
            contentDescriptionSelected = "Unpin",
            background = R.drawable.toggle_background) {
            // Do nothing
        }

        toolbar.addBrowserAction(pin)

        val turbo = BrowserToolbar.ToggleButton(
            image = resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_rocket),
            imageSelected = resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_rocket_filled),
            contentDescription = "Turbo: Off",
            contentDescriptionSelected = "Turbo: On",
            background = R.drawable.toggle_background) {
            // Do nothing
        }

        toolbar.addBrowserAction(turbo)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Add a fixed size element for spacing and a branding image
        // //////////////////////////////////////////////////////////////////////////////////////////

        val space = Toolbar.ActionSpace(SPACING_SIZE_DP.dpToPx(resources.displayMetrics))

        toolbar.addBrowserAction(space)

        val brand = Toolbar.ActionImage(resources.getThemedDrawable(R.drawable.brand))

        toolbar.addBrowserAction(brand)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Intercept clicks on the URL to not open editing mode
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.onUrlClicked = {
            // Return false to not go to editing mode and then handle the event manually here...
            true
        }

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL and set custom text to be displayed if no URL is set
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.hint = "Search or enter address"
        toolbar.url = "https://www.nytimes.com/video"

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Whenever an URL was entered.. pretend to load it and re-insert it into the toolbar
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.setOnUrlCommitListener { url ->
            simulateReload(urlBoxProgress)

            if (url.isNotEmpty()) {
                toolbar.url = url
            }

            true
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

    private var loading = MutableLiveData<Boolean>()

    @Suppress("TooGenericExceptionCaught", "LongMethod", "ComplexMethod")
    private fun simulateReload(view: UrlBoxProgressView? = null) {
        job?.cancel()

        loading.value = true

        job = CoroutineScope(Dispatchers.Main).launch {
            try {
                loop@ for (progress in PROGRESS_RANGE step RELOAD_STEP_SIZE) {
                    if (!isActive) {
                        break@loop
                    }

                    if (view == null) {
                        toolbar.displayProgress(progress)
                    } else {
                        view.progress = progress
                    }

                    delay(progress * RELOAD_STEP_SIZE.toLong())
                }
            } catch (t: Throwable) {
                if (view == null) {
                    toolbar.displayProgress(0)
                } else {
                    view.progress = 0
                }

                throw t
            } finally {
                loading.value = false

                // Update toolbar buttons to reflect loading state
                toolbar.invalidateActions()
            }
        }

        // Update toolbar buttons to reflect loading state
        toolbar.invalidateActions()
    }

    private fun Resources.getThemedDrawable(@DrawableRes resId: Int) = getDrawable(resId, theme)

    companion object {
        private val PROGRESS_RANGE = 0..100
        private const val SPACING_SIZE_DP = 128
        private const val RELOAD_STEP_SIZE = 5
    }
}
