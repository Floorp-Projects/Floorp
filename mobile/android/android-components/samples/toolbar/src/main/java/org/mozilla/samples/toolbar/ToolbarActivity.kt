/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.toolbar

import android.content.res.Resources
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
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
import kotlinx.coroutines.GlobalScope
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
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.ui.tabcounter.TabCounter

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
            ToolbarConfiguration.CUSTOM_MENU -> setupCustomMenu()
            ToolbarConfiguration.PRIVATE_MODE -> setupDefaultToolbar(private = true)
            ToolbarConfiguration.FENIX -> setupFenixToolbar()
            ToolbarConfiguration.FENIX_CUSTOMTAB -> setupFenixCustomTabToolbar()
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
        toolbar.display.menuBuilder = builder

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"
    }

    /**
     * A custom browser menu.
     */
    private fun setupCustomMenu() {
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
        toolbar.display.menuBuilder = builder

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
        toolbar.display.menuBuilder = builder

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"
    }

    private class FakeTabCounterToolbarButton : Toolbar.Action {
        override fun createView(parent: ViewGroup): View = TabCounter(parent.context).apply {
            setCount(2)
            setBackgroundResource(
                parent.context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless)
            )
        }

        override fun bind(view: View) = Unit
    }

    /**
     * A toolbar that looks like the toolbar in Fenix (Light theme).
     */
    @Suppress("MagicNumber")
    fun setupFenixToolbar() {
        toolbar.setBackgroundColor(0xFFFFFFFF.toInt())

        toolbar.display.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
            DisplayToolbar.Indicators.TRACKING_PROTECTION,
            DisplayToolbar.Indicators.EMPTY
        )

        toolbar.display.colors = toolbar.display.colors.copy(
            securityIconInsecure = 0xFF20123a.toInt(),
            securityIconSecure = 0xFF20123a.toInt(),
            text = 0xFF0c0c0d.toInt(),
            menu = 0xFF20123a.toInt(),
            separator = 0x1E15141a.toInt(),
            trackingProtection = 0xFF20123a.toInt(),
            emptyIcon = 0xFF20123a.toInt(),
            hint = 0x1E15141a.toInt()
        )

        toolbar.display.setUrlBackground(getDrawable(R.drawable.fenix_url_background))
        toolbar.display.hint = "Search or enter address"
        toolbar.display.setOnUrlLongClickListener {
            Toast.makeText(this, "Long click!", Toast.LENGTH_SHORT).show()
            true
        }

        val share = SimpleBrowserMenuItem("Share…") { /* Do nothing */ }
        val homeScreen = SimpleBrowserMenuItem("Add to Home screen") { /* Do nothing */ }
        val open = SimpleBrowserMenuItem("Open in…") { /* Do nothing */ }
        val settings = SimpleBrowserMenuItem("Settings") { /* Do nothing */ }

        toolbar.display.menuBuilder = BrowserMenuBuilder(listOf(share, homeScreen, open, settings))

        toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"

        toolbar.addBrowserAction(FakeTabCounterToolbarButton())

        toolbar.display.setOnSiteSecurityClickedListener {
            Toast.makeText(this, "Site security", Toast.LENGTH_SHORT).show()
        }

        toolbar.edit.colors = toolbar.edit.colors.copy(
            text = 0xFF0c0c0d.toInt(),
            clear = 0xFF0c0c0d.toInt(),
            icon = 0xFF0c0c0d.toInt()
        )

        toolbar.edit.setUrlBackground(getDrawable(R.drawable.fenix_url_background))
        toolbar.edit.setIcon(getDrawable(R.drawable.mozac_ic_search)!!, "Search")

        toolbar.setOnUrlCommitListener { url ->
            simulateReload()
            toolbar.url = url

            true
        }
    }

    /**
     * A toolbar that looks like the toolbar in Fenix in a custom tab.
     */
    @Suppress("MagicNumber")
    fun setupFenixCustomTabToolbar() {
        toolbar.setBackgroundColor(0xFFFFFFFF.toInt())

        toolbar.display.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
            DisplayToolbar.Indicators.TRACKING_PROTECTION
        )

        toolbar.display.colors = toolbar.display.colors.copy(
            securityIconSecure = 0xFF20123a.toInt(),
            securityIconInsecure = 0xFF20123a.toInt(),
            text = 0xFF0c0c0d.toInt(),
            title = 0xFF0c0c0d.toInt(),
            menu = 0xFF20123a.toInt(),
            separator = 0x1E15141a.toInt(),
            trackingProtection = 0xFF20123a.toInt()
        )

        val share = SimpleBrowserMenuItem("Share…") { /* Do nothing */ }
        val homeScreen = SimpleBrowserMenuItem("Add to Home screen") { /* Do nothing */ }
        val open = SimpleBrowserMenuItem("Open in…") { /* Do nothing */ }
        val settings = SimpleBrowserMenuItem("Settings") { /* Do nothing */ }

        toolbar.display.menuBuilder = BrowserMenuBuilder(listOf(share, homeScreen, open, settings))

        toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"

        val drawableIcon = ContextCompat.getDrawable(this, R.drawable.mozac_ic_close)

        drawableIcon?.apply {
            setTint(0xFF20123a.toInt())
        }.also {
            val button = Toolbar.ActionButton(
                it, "Close"
            ) {
                Toast.makeText(this, "Close!", Toast.LENGTH_SHORT).show()
            }
            toolbar.addNavigationAction(button)
        }

        val drawable = ContextCompat.getDrawable(this, R.drawable.mozac_ic_share)?.apply {
            setTint(0xFF20123a.toInt())
        }

        val button = Toolbar.ActionButton(drawable, "Share") {
            Toast.makeText(this, "Share!", Toast.LENGTH_SHORT).show()
        }

        toolbar.addBrowserAction(button)

        toolbar.display.setOnSiteSecurityClickedListener {
            Toast.makeText(this, "Site security", Toast.LENGTH_SHORT).show()
        }

        GlobalScope.launch(Dispatchers.Main) {
            delay(2000)
            toolbar.title = "Mobile browsers for iOS and Android | Firefox"
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
