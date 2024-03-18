/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.view.View
import kotlinx.coroutines.isActive
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import org.mozilla.focus.ext.components
import org.mozilla.focus.fragment.UrlInputFragment
import org.mozilla.focus.input.InputToolbarIntegration
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class InputToolbarIntegrationTest {
    private lateinit var toolbar: BrowserToolbar

    @Mock
    private lateinit var fragment: UrlInputFragment

    @Mock
    private lateinit var fragmentView: View

    private lateinit var inputToolbarIntegration: InputToolbarIntegration

    private val appStore: AppStore = testContext.components.appStore

    @Before
    fun setUp() {
        MockitoAnnotations.openMocks(this)

        toolbar = BrowserToolbar(testContext)
        whenever(fragment.resources).thenReturn(testContext.resources)
        whenever(fragment.context).thenReturn(testContext)
        whenever(fragment.view).thenReturn(fragmentView)

        inputToolbarIntegration = InputToolbarIntegration(
            toolbar,
            fragment,
            mock(),
            mock(),
        )
    }

    @Test
    fun `GIVEN app fresh install WHEN input toolbar integration is starting THEN start browsing scope is populated`() {
        appStore.dispatch(AppAction.ShowStartBrowsingCfrChange(true)).joinBlocking()

        assertNull(inputToolbarIntegration.startBrowsingCfrScope)

        inputToolbarIntegration.start()

        assertNotNull(inputToolbarIntegration.startBrowsingCfrScope)
    }

    @Test
    fun `GIVEN app fresh install WHEN input toolbar integration is stoping THEN start browsing scope is canceled`() {
        inputToolbarIntegration.start()

        assertTrue(inputToolbarIntegration.startBrowsingCfrScope?.isActive ?: true)

        inputToolbarIntegration.stop()

        assertFalse(inputToolbarIntegration.startBrowsingCfrScope?.isActive ?: false)
    }
}
