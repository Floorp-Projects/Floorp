package mozilla.components.browser.session.storage/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import android.os.SystemClock
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.android.test.rules.WebserverRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.TimeoutException

@RunWith(AndroidJUnit4::class)
class FullRestoreTest {
    @get:Rule
    val webserverRule: WebserverRule = WebserverRule()

    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    /**
     * In this test we use GeckoView Nightly to load a test page and then we save and restore the
     * browsing session, asserting that we end up with the same state as before.
     */
    @Test
    fun loadAndRestore() {
        val engine = createEngine()

        run {
            // -------------------------------------------------------------------------------------
            // Set up
            // -------------------------------------------------------------------------------------

            val store = createStore(engine)
            val tabsUseCases = TabsUseCases(store)

            // -------------------------------------------------------------------------------------
            // Add a tab
            // -------------------------------------------------------------------------------------

            tabsUseCases.addTab(webserverRule.url())

            waitFor { store.state.selectedTab?.content?.title == "Restore Test" }
            waitFor { store.state.selectedTab?.content?.loading == false }
            waitFor { store.state.selectedTab?.engineState?.engineSessionState != null }

            // -------------------------------------------------------------------------------------
            // Save state
            // -------------------------------------------------------------------------------------

            val storage = SessionStorage(context, engine)
            storage.save(store.state)
        }

        run {
            // -------------------------------------------------------------------------------------
            // Restore into new classes
            // -------------------------------------------------------------------------------------

            val storage = SessionStorage(context, engine)

            val newStore = createStore(engine)
            val newUseCases = TabsUseCases(newStore)

            assertNull(newStore.state.selectedTab)

            val browsingSession = storage.restore()
            assertNotNull(browsingSession)
            newUseCases.restore(browsingSession!!)

            waitFor { newStore.state.selectedTab?.engineState != null }
            waitFor { newStore.state.selectedTab?.content?.title == "Restore Test" }
        }
    }

    private fun createEngine(): Engine {
        return runBlocking(Dispatchers.Main) {
            GeckoEngine(context)
        }
    }

    private fun createStore(
        engine: Engine,
    ): BrowserStore {
        return runBlocking(Dispatchers.Main) {
            BrowserStore(middleware = EngineMiddleware.create(engine))
        }
    }
}

private fun waitFor(timeoutMs: Long = 10000, cadence: Long = 100, predicate: () -> Boolean) {
    val start = SystemClock.elapsedRealtime()

    do {
        if (predicate()) {
            return
        }
        Thread.sleep(cadence)
    } while (SystemClock.elapsedRealtime() - start < timeoutMs)

    throw TimeoutException()
}
