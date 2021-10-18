/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.app.Activity
import android.content.res.Resources
import android.view.View
import android.view.Window
import android.view.WindowManager
import android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
import mozilla.components.browser.engine.gecko.GeckoEngineView
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.jupiter.api.Assertions.assertEquals
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.focus.ext.disableDynamicBehavior
import org.mozilla.focus.ext.enableDynamicBehavior
import org.mozilla.focus.ext.hide
import org.mozilla.focus.ext.showAsFixed
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.widget.FloatingEraseButton
import org.mozilla.focus.widget.FloatingSessionsButton

internal class FullScreenIntegrationTest {
    @Test
    fun `WHEN the integration is started THEN start FullScreenFeature`() {
        val feature: FullScreenFeature = mock()
        val integration = FullScreenIntegration(
            mock(), mock(), null, mock(), mock(), mock(), mock(), mock(), mock(), mock()
        ).apply {
            this.feature = feature
        }

        integration.start()

        verify(feature).start()
    }

    @Test
    fun `WHEN the integration is stopped THEN stop FullScreenFeature`() {
        val feature: FullScreenFeature = mock()
        val integration = FullScreenIntegration(
            mock(), mock(), null, mock(), mock(), mock(), mock(), mock(), mock(), mock()
        ).apply {
            this.feature = feature
        }

        integration.stop()

        verify(feature).stop()
    }

    @Test
    fun `WHEN back is pressed THEN send this to the feature`() {
        val feature: FullScreenFeature = mock()
        val integration = FullScreenIntegration(
            mock(), mock(), null, mock(), mock(), mock(), mock(), mock(), mock(), mock()
        ).apply {
            this.feature = feature
        }

        integration.onBackPressed()

        verify(feature).onBackPressed()
    }

    @Test
    fun `WHEN the viewport changes THEN update layoutInDisplayCutoutMode`() {
        val windowAttributes = WindowManager.LayoutParams()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(windowAttributes).`when`(activityWindow).attributes
        val integration = FullScreenIntegration(
            activity, mock(), null, mock(), mock(), mock(), mock(), mock(), mock(), mock()
        )

        integration.viewportFitChanged(33)

        assertEquals(33, windowAttributes.layoutInDisplayCutoutMode)
    }

    @Test
    @Suppress("DEPRECATION")
    fun `WHEN entering immersive mode THEN hide all system bars`() {
        val decorView: View = mock()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        val integration = FullScreenIntegration(
            activity, mock(), null, mock(), mock(), mock(), mock(), mock(), mock(), mock()
        )

        integration.switchToImmersiveMode()

        verify(activityWindow).addFlags(FLAG_KEEP_SCREEN_ON)
        verify(decorView).systemUiVisibility =
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_FULLSCREEN or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
    }

    @Test
    @Suppress("DEPRECATION")
    fun `GIVEN immersive mode WHEN exitImmersiveModeIfNeeded is called THEN show the system bars`() {
        val windowAttributes = WindowManager.LayoutParams()
        // This flag is checked to infer whether already in immersive mode or not
        windowAttributes.flags = FLAG_KEEP_SCREEN_ON
        val decorView: View = mock()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(windowAttributes).`when`(activityWindow).attributes
        val integration = FullScreenIntegration(
            activity, mock(), null, mock(), mock(), mock(), mock(), mock(), mock(), mock()
        )

        integration.exitImmersiveModeIfNeeded()

        verify(activityWindow).clearFlags(FLAG_KEEP_SCREEN_ON)
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
    }

    @Test
    @Suppress("DEPRECATION")
    fun `GIVEN not in immersive mode WHEN exitImmersiveModeIfNeeded is called THEN don't change anything`() {
        val windowAttributes = WindowManager.LayoutParams()
        // This flag is checked to infer whether already in immersive mode or not
        windowAttributes.flags = 0
        val decorView: View = mock()
        val activityWindow: Window = mock()
        val activity: Activity = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(windowAttributes).`when`(activityWindow).attributes
        val integration = FullScreenIntegration(
            activity, mock(), null, mock(), mock(), mock(), mock(), mock(), mock(), mock()
        )

        integration.exitImmersiveModeIfNeeded()

        verify(activityWindow, never()).clearFlags(anyInt())
        verify(decorView, never()).systemUiVisibility = anyInt()
    }

    @Test
    fun `GIVEN a11y is enabled WHEN enterBrowserFullscreen THEN hide the toolbar and the fabs`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val eraseFab: FloatingEraseButton = mock()
        val sessionsFab: FloatingSessionsButton = mock()
        val settings: Settings = mock()
        doReturn(true).`when`(settings).isAccessibilityEnabled()
        val integration = FullScreenIntegration(
            mock(), mock(), null, mock(), settings, toolbar, mock(), engineView, eraseFab, sessionsFab
        )

        integration.enterBrowserFullscreen()

        verify(toolbar).hide(engineView)
        verify(eraseFab).visibility = View.GONE
        verify(sessionsFab).visibility = View.GONE
        verify(toolbar, never()).collapse()
        verify(toolbar, never()).disableDynamicBehavior(engineView)
    }

    @Test
    fun `GIVEN a11y is disabled WHEN enterBrowserFullscreen THEN collapse and disable the dynamic toolbar`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val eraseFab: FloatingEraseButton = mock()
        val sessionsFab: FloatingSessionsButton = mock()
        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()
        val integration = FullScreenIntegration(
            mock(), mock(), null, mock(), settings, toolbar, mock(), engineView, eraseFab, sessionsFab
        )

        integration.enterBrowserFullscreen()

        verify(toolbar, never()).hide(engineView)
        verify(eraseFab, never()).visibility = View.GONE
        verify(sessionsFab, never()).visibility = View.GONE
        with(inOrder(toolbar)) {
            verify(toolbar).collapse()
            verify(toolbar).disableDynamicBehavior(engineView)
        }
    }

    @Test
    fun `GIVEN a11y is enabled WHEN exitBrowserFullscreen THEN show the toolbar and the fabs`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val eraseFab: FloatingEraseButton = mock()
        val sessionsFab: FloatingSessionsButton = mock()
        val settings: Settings = mock()
        doReturn(true).`when`(settings).isAccessibilityEnabled()
        val resources: Resources = mock()
        val activity: Activity = mock()
        doReturn(resources).`when`(activity).resources
        val integration = FullScreenIntegration(
            activity, mock(), null, mock(), settings, toolbar, mock(), engineView, eraseFab, sessionsFab
        )

        integration.exitBrowserFullscreen()

        verify(toolbar).showAsFixed(activity, engineView)
        verify(eraseFab).visibility = View.VISIBLE
        verify(sessionsFab).visibility = View.VISIBLE
        verify(toolbar, never()).expand()
        verify(toolbar, never()).enableDynamicBehavior(activity, engineView)
    }

    @Test
    fun `GIVEN a11y is disabled WHEN exitBrowserFullscreen THEN enable the dynamic toolbar and expand it`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val eraseFab: FloatingEraseButton = mock()
        val sessionsFab: FloatingSessionsButton = mock()
        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()
        val resources: Resources = mock()
        val activity: Activity = mock()
        doReturn(resources).`when`(activity).resources
        val integration = FullScreenIntegration(
            activity, mock(), null, mock(), settings, toolbar, mock(), engineView, eraseFab, sessionsFab
        )

        integration.exitBrowserFullscreen()

        verify(toolbar, never()).showAsFixed(activity, engineView)
        verify(eraseFab, never()).visibility = View.VISIBLE
        verify(sessionsFab, never()).visibility = View.VISIBLE
        with(inOrder(toolbar)) {
            verify(toolbar).enableDynamicBehavior(activity, engineView)
            verify(toolbar).expand()
        }
    }

    @Test
    fun `WHEN entering fullscreen THEN put browser in fullscreen, hide system bars and enter immersive mode`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val eraseFab: FloatingEraseButton = mock()
        val sessionsFab: FloatingSessionsButton = mock()
        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()
        val resources: Resources = mock()
        val activityWindow: Window = mock()
        val decorView: View = mock()
        val activity: Activity = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(resources).`when`(activity).resources
        val statusBar: View = mock()
        val integration = spy(
            FullScreenIntegration(
                activity, mock(), null, mock(), settings, toolbar, statusBar, engineView, eraseFab, sessionsFab
            )
        )

        integration.fullScreenChanged(true)

        verify(integration).enterBrowserFullscreen()
        verify(integration).switchToImmersiveMode()
        verify(statusBar).visibility = View.GONE
    }

    @Test
    fun `WHEN exiting fullscreen THEN put browser in fullscreen, hide system bars and enter immersive mode`() {
        val toolbar: BrowserToolbar = mock()
        val engineView: GeckoEngineView = mock()
        doReturn(mock<View>()).`when`(engineView).asView()
        val eraseFab: FloatingEraseButton = mock()
        val sessionsFab: FloatingSessionsButton = mock()
        val settings: Settings = mock()
        doReturn(false).`when`(settings).isAccessibilityEnabled()
        val resources: Resources = mock()
        val activityWindow: Window = mock()
        val decorView: View = mock()
        val windowAttributes = WindowManager.LayoutParams()
        val activity: Activity = mock()
        doReturn(activityWindow).`when`(activity).window
        doReturn(decorView).`when`(activityWindow).decorView
        doReturn(windowAttributes).`when`(activityWindow).attributes
        doReturn(resources).`when`(activity).resources
        val statusBar: View = mock()
        val integration = spy(
            FullScreenIntegration(
                activity, mock(), null, mock(), settings, toolbar, statusBar, engineView, eraseFab, sessionsFab
            )
        )

        integration.fullScreenChanged(false)

        verify(integration).exitBrowserFullscreen()
        verify(integration).exitImmersiveModeIfNeeded()
        verify(statusBar).visibility = View.VISIBLE
    }
}
