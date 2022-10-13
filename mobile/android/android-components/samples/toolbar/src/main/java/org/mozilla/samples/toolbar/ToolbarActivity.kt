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
import androidx.core.content.res.ResourcesCompat
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.Observer
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.DelicateCoroutinesApi
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
import mozilla.components.browser.menu.ext.asCandidateList
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.menu2.BrowserMenuController
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.NestedMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.utils.URLStringUtils
import mozilla.components.ui.tabcounter.TabCounter
import org.mozilla.samples.toolbar.databinding.ActivityToolbarBinding

/**
 * This sample application shows how to use and customize the browser-toolbar component.
 */
@Suppress("LargeClass")
class ToolbarActivity : AppCompatActivity() {
    private val shippedDomainsProvider = ShippedDomainsProvider()
    private val customDomainsProvider = CustomDomainsProvider()
    private lateinit var binding: ActivityToolbarBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityToolbarBinding.inflate(layoutInflater)

        shippedDomainsProvider.initialize(this)
        customDomainsProvider.initialize(this)

        setContentView(binding.root)

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

        ToolbarAutocompleteFeature(binding.toolbar).apply {
            this.addDomainProvider(shippedDomainsProvider)
            this.addDomainProvider(customDomainsProvider)
        }
    }

    override fun onPause() {
        super.onPause()

        binding.toolbar.hideKeyboard()
    }

    /**
     * A very simple toolbar with mostly default values.
     */
    private fun setupDefaultToolbar(private: Boolean = false) {
        binding.toolbar.setBackgroundColor(
            ContextCompat.getColor(this, mozilla.components.ui.colors.R.color.photonBlue80),
        )

        binding.toolbar.private = private

        binding.toolbar.url = "https://www.mozilla.org/en-US/firefox/"
    }

    /**
     * A toolbar that looks like Firefox Focus on tablets.
     */
    private fun setupFocusTabletToolbar() {
        // //////////////////////////////////////////////////////////////////////////////////////////
        // Use the iconic gradient background
        // //////////////////////////////////////////////////////////////////////////////////////////

        val background = ContextCompat.getDrawable(this, R.drawable.focus_background)
        binding.toolbar.background = background

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Add "back" and "forward" navigation actions
        // //////////////////////////////////////////////////////////////////////////////////////////

        val back = BrowserToolbar.Button(
            resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_back)!!,
            "Back",
        ) {
            simulateReload()
        }

        binding.toolbar.addNavigationAction(back)

        val forward = BrowserToolbar.Button(
            resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_forward)!!,
            "Forward",
        ) {
            simulateReload()
        }

        binding.toolbar.addNavigationAction(forward)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Add a "reload" browser action that simulates reloading the current page
        // //////////////////////////////////////////////////////////////////////////////////////////

        val reload = BrowserToolbar.TwoStateButton(
            primaryImage = resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_refresh)!!,
            primaryContentDescription = "Reload",
            secondaryImage = resources.getThemedDrawable(mozilla.components.ui.icons.R.drawable.mozac_ic_stop)!!,
            secondaryContentDescription = "Stop",
            isInPrimaryState = { loading.value != true },
            disableInSecondaryState = false,
        ) {
            if (loading.value == true) {
                job?.cancel()
            } else {
                simulateReload()
            }
        }
        binding.toolbar.addBrowserAction(reload)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Create a menu that looks like the one in Firefox Focus
        // //////////////////////////////////////////////////////////////////////////////////////////

        val fenix = SimpleBrowserMenuItem("POWERED BY MOZILLA")
        val share = SimpleBrowserMenuItem("Share…") { /* Do nothing */ }
        val homeScreen = SimpleBrowserMenuItem("Add to Home screen") { /* Do nothing */ }
        val open = SimpleBrowserMenuItem("Open in…") { /* Do nothing */ }
        val settings = SimpleBrowserMenuItem("Settings") { /* Do nothing */ }

        val items = listOf(fenix, share, homeScreen, open, settings)
        binding.toolbar.display.menuBuilder = BrowserMenuBuilder(items)

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        binding.toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"
    }

    /**
     * A custom browser menu.
     */
    private fun setupCustomMenu() {
        binding.toolbar.setBackgroundColor(
            ContextCompat.getColor(this, mozilla.components.ui.colors.R.color.photonBlue80),
        )

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Create a menu with text and icons
        // //////////////////////////////////////////////////////////////////////////////////////////

        val share = TextMenuCandidate(
            "Share",
            start = DrawableMenuIcon(this, R.drawable.mozac_ic_share),
        ) { /* Do nothing */ }

        val search = TextMenuCandidate(
            "Search",
            start = DrawableMenuIcon(this, R.drawable.mozac_ic_search),
        ) { /* Do nothing */ }

        binding.toolbar.display.menuController = BrowserMenuController(Side.START).apply {
            submitList(listOf(share, DividerMenuCandidate(), search))
        }

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        binding.toolbar.url = "https://www.mozilla.org/"
    }

    /**
     * A toolbar that looks like Firefox Focus on phones.
     */
    private fun setupFocusPhoneToolbar() {
        // //////////////////////////////////////////////////////////////////////////////////////////
        // Use the iconic gradient background
        // //////////////////////////////////////////////////////////////////////////////////////////

        val background = ContextCompat.getDrawable(this, R.drawable.focus_background)
        binding.toolbar.background = background

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Create a "mini" toolbar to be shown inside the menu (forward, reload)
        // //////////////////////////////////////////////////////////////////////////////////////////

        val forward = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            "Forward",
            isEnabled = { canGoForward() },
        ) {
            simulateReload()
        }

        val reload = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            primaryContentDescription = "Reload",
            secondaryImageResource = R.drawable.mozac_ic_stop,
            secondaryContentDescription = "Stop",
            isInPrimaryState = { loading.value != true },
            disableInSecondaryState = false,
        ) {
            if (loading.value == true) {
                job?.cancel()
            } else {
                simulateReload()
            }
        }
        // Redraw the reload button when loading state changes
        loading.observe(this, Observer { binding.toolbar.invalidateActions() })

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

        val items = listOf(menuToolbar, blocking, share, homeScreen, open, settings)
        binding.toolbar.display.menuBuilder = BrowserMenuBuilder(items)
        binding.toolbar.invalidateActions()

        // //////////////////////////////////////////////////////////////////////////////////////////
        // Display a URL
        // //////////////////////////////////////////////////////////////////////////////////////////

        binding.toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"
    }

    private class FakeTabCounterToolbarButton : Toolbar.Action {
        override fun createView(parent: ViewGroup): View = TabCounter(parent.context).apply {
            setCount(2)
            setBackgroundResource(
                parent.context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless),
            )
        }

        override fun bind(view: View) = Unit
    }

    /**
     * A toolbar that looks like the toolbar in Fenix (Light theme).
     */
    @Suppress("MagicNumber")
    fun setupFenixToolbar() {
        binding.toolbar.setBackgroundColor(0xFFFFFFFF.toInt())

        binding.toolbar.display.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
            DisplayToolbar.Indicators.TRACKING_PROTECTION,
            DisplayToolbar.Indicators.EMPTY,
        )

        binding.toolbar.display.colors = binding.toolbar.display.colors.copy(
            securityIconInsecure = 0xFF20123a.toInt(),
            securityIconSecure = 0xFF20123a.toInt(),
            text = 0xFF0c0c0d.toInt(),
            menu = 0xFF20123a.toInt(),
            separator = 0x1E15141a.toInt(),
            trackingProtection = 0xFF20123a.toInt(),
            emptyIcon = 0xFF20123a.toInt(),
            hint = 0x1E15141a.toInt(),
        )

        binding.toolbar.display.urlFormatter = { url ->
            URLStringUtils.toDisplayUrl(url)
        }

        binding.toolbar.display.setUrlBackground(
            ContextCompat.getDrawable(this, R.drawable.fenix_url_background),
        )
        binding.toolbar.display.hint = "Search or enter address"
        binding.toolbar.display.setOnUrlLongClickListener {
            Toast.makeText(this, "Long click!", Toast.LENGTH_SHORT).show()
            true
        }

        val share = TextMenuCandidate("Share…") { /* Do nothing */ }
        val homeScreen = TextMenuCandidate("Add to Home screen") { /* Do nothing */ }
        val open = TextMenuCandidate("Open in…") { /* Do nothing */ }
        val settings = NestedMenuCandidate(
            id = R.id.mozac_browser_toolbar_menu,
            text = "Settings",
            subMenuItems = listOf(
                NestedMenuCandidate(id = R.id.container, text = "Back", subMenuItems = null),
                TextMenuCandidate("Setting 1") { /* Do nothing */ },
                TextMenuCandidate("Setting 2") { /* Do nothing */ },
            ),
        )

        val items = listOf(share, homeScreen, open, settings)
        binding.toolbar.display.menuController = BrowserMenuController().apply {
            submitList(items)
        }

        binding.toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"

        binding.toolbar.addBrowserAction(FakeTabCounterToolbarButton())

        binding.toolbar.display.setOnSiteSecurityClickedListener {
            Toast.makeText(this, "Site security", Toast.LENGTH_SHORT).show()
        }

        binding.toolbar.edit.colors = binding.toolbar.edit.colors.copy(
            text = 0xFF0c0c0d.toInt(),
            clear = 0xFF0c0c0d.toInt(),
            icon = 0xFF0c0c0d.toInt(),
        )

        binding.toolbar.edit.setUrlBackground(
            ContextCompat.getDrawable(this, R.drawable.fenix_url_background),
        )
        binding.toolbar.edit.setIcon(
            ContextCompat.getDrawable(this, R.drawable.mozac_ic_search)!!,
            "Search",
        )

        binding.toolbar.setOnUrlCommitListener { url ->
            simulateReload()
            binding.toolbar.url = url

            true
        }
    }

    /**
     * A toolbar that looks like the toolbar in Fenix in a custom tab.
     */
    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    @Suppress("MagicNumber")
    fun setupFenixCustomTabToolbar() {
        binding.toolbar.setBackgroundColor(0xFFFFFFFF.toInt())

        binding.toolbar.display.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
            DisplayToolbar.Indicators.TRACKING_PROTECTION,
        )

        binding.toolbar.display.colors = binding.toolbar.display.colors.copy(
            securityIconSecure = 0xFF20123a.toInt(),
            securityIconInsecure = 0xFF20123a.toInt(),
            text = 0xFF0c0c0d.toInt(),
            title = 0xFF0c0c0d.toInt(),
            menu = 0xFF20123a.toInt(),
            separator = 0x1E15141a.toInt(),
            trackingProtection = 0xFF20123a.toInt(),
        )

        val share = SimpleBrowserMenuItem("Share…") { /* Do nothing */ }
        val homeScreen = SimpleBrowserMenuItem("Add to Home screen") { /* Do nothing */ }
        val open = SimpleBrowserMenuItem("Open in…") { /* Do nothing */ }
        val settings = SimpleBrowserMenuItem("Settings") { /* Do nothing */ }

        val items = listOf(share, homeScreen, open, settings)
        binding.toolbar.display.menuBuilder = BrowserMenuBuilder(items)
        binding.toolbar.display.menuController = BrowserMenuController().apply {
            submitList(items.asCandidateList(this@ToolbarActivity))
        }

        binding.toolbar.url = "https://www.mozilla.org/en-US/firefox/mobile/"

        val drawableIcon = ContextCompat.getDrawable(this, R.drawable.mozac_ic_close)

        drawableIcon?.apply {
            setTint(0xFF20123a.toInt())
        }.also {
            val button = Toolbar.ActionButton(
                it,
                "Close",
            ) {
                Toast.makeText(this, "Close!", Toast.LENGTH_SHORT).show()
            }
            binding.toolbar.addNavigationAction(button)
        }

        val drawable = ContextCompat.getDrawable(this, R.drawable.mozac_ic_share)?.apply {
            setTint(0xFF20123a.toInt())
        }

        val button = Toolbar.ActionButton(drawable, "Share") {
            Toast.makeText(this, "Share!", Toast.LENGTH_SHORT).show()
        }

        binding.toolbar.addBrowserAction(button)

        binding.toolbar.display.setOnSiteSecurityClickedListener {
            Toast.makeText(this, "Site security", Toast.LENGTH_SHORT).show()
        }

        GlobalScope.launch(Dispatchers.Main) {
            delay(2000)
            binding.toolbar.title = "Mobile browsers for iOS and Android | Firefox"
        }
    }

    // For testing purposes
    private var forward = true
    private var back = true

    private fun canGoForward(): Boolean = forward

    @Suppress("UnusedPrivateMember")
    private fun canGoBack(): Boolean = back

    @Suppress("UnusedPrivateMember")
    private fun goBack() {
        back = !(forward && back)
        forward = true
    }

    @Suppress("UnusedPrivateMember")
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
                        binding.toolbar.displayProgress(progress)
                    } else {
                        view.progress = progress
                    }

                    delay(progress * RELOAD_STEP_SIZE.toLong())
                }
            } catch (t: Throwable) {
                if (view == null) {
                    binding.toolbar.displayProgress(0)
                } else {
                    view.progress = 0
                }

                throw t
            } finally {
                loading.value = false

                // Update toolbar buttons to reflect loading state
                binding.toolbar.invalidateActions()
            }
        }

        // Update toolbar buttons to reflect loading state
        binding.toolbar.invalidateActions()
    }

    private fun Resources.getThemedDrawable(@DrawableRes resId: Int) = ResourcesCompat.getDrawable(this, resId, theme)

    companion object {
        private val PROGRESS_RANGE = 0..100
        private const val RELOAD_STEP_SIZE = 5
    }
}
