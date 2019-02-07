/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.NavigationUI
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.intent.IntentProcessor
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.browser.BrowserFragment
import org.mozilla.fenix.ext.components

open class HomeActivity : AppCompatActivity() {
    val themeManager = DefaultThemeManager().also {
        it.onThemeChange = { theme ->
            setTheme(theme)
            recreate()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_home)

        setTheme(themeManager.currentTheme)
        DefaultThemeManager.applyStatusBarTheme(window, themeManager, this)

        if (intent?.extras?.getBoolean(OPEN_TO_BROWSER) == true) {
            openToBrowser()
        }

        val host = supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment
        val hostNavController = host.navController
        val appBarConfiguration = AppBarConfiguration.Builder(setOf(R.id.libraryFragment)).build()
        val navigationToolbar = findViewById<Toolbar>(R.id.navigationToolbar)
        setSupportActionBar(navigationToolbar)
        NavigationUI.setupWithNavController(navigationToolbar, hostNavController, appBarConfiguration)
    }

    override fun onCreateView(
        parent: View?,
        name: String,
        context: Context,
        attrs: AttributeSet
    ): View? =
        when (name) {
            EngineView::class.java.name -> components.core.engine.createView(context, attrs).asView()
            else -> super.onCreateView(parent, name, context, attrs)
        }

    override fun onBackPressed() {
        supportFragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is BackHandler && it.onBackPressed()) {
                return
            }
        }

        super.onBackPressed()
    }

    private fun openToBrowser() {
        val sessionId = SafeIntent(intent).getStringExtra(IntentProcessor.ACTIVE_SESSION_ID)
        val host = supportFragmentManager.findFragmentById(R.id.container) as NavHostFragment

        host.navController.navigate(R.id.action_global_browser, Bundle().apply {
            putString(BrowserFragment.SESSION_ID, sessionId)
        })
    }

    companion object {
        const val OPEN_TO_BROWSER = "open_to_browser"
    }
}
