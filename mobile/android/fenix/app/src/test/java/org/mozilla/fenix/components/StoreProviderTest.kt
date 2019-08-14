/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class StoreProviderTest {

    private class BasicState : State

    private val basicStore = Store(BasicState()) { state, _: Action -> state }

    @Test
    fun `factory returns store provider`() {
        var createCalled = false
        val factory = StoreProviderFactory {
            createCalled = true
            basicStore
        }

        assertFalse(createCalled)

        assertEquals(basicStore, factory.create(StoreProvider::class.java).store)

        assertTrue(createCalled)
    }

    @Test
    fun `get returns store`() {
        val activity = Robolectric.buildActivity(FragmentActivity::class.java)
            .create()
            .start()
            .resume()
            .get()

        val fragment = Fragment()

        activity.supportFragmentManager.beginTransaction().apply {
            add(fragment, "test")
            commitNow()
        }

        val store = StoreProvider.get(fragment) { basicStore }
        assertEquals(basicStore, store)
    }

    @Test
    fun `get only calls createStore if needed`() {
        val activity = Robolectric.buildActivity(FragmentActivity::class.java)
            .create()
            .start()
            .resume()
            .get()

        val fragment = Fragment()

        activity.supportFragmentManager.beginTransaction().apply {
            add(fragment, "test")
            commitNow()
        }

        var createCalled = false
        val createStore = {
            createCalled = true
            basicStore
        }

        StoreProvider.get(fragment, createStore)
        assertTrue(createCalled)

        createCalled = false
        StoreProvider.get(fragment, createStore)
        assertFalse(createCalled)
    }
}
