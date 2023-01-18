/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class FenixSnackbarBehaviorTest {
    private val snackbarParams = CoordinatorLayout.LayoutParams(0, 0)
    private val snackbarContainer = FrameLayout(testContext)
    private val dependency = View(testContext)
    private val parent = CoordinatorLayout(testContext)

    @Before
    fun setup() {
        snackbarContainer.layoutParams = snackbarParams
        parent.addView(dependency)
    }

    @Test
    fun `GIVEN no valid anchors are shown WHEN the snackbar is shown THEN don't anchor it`() {
        val behavior = FenixSnackbarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        behavior.layoutDependsOn(parent, snackbarContainer, dependency)

        assertSnackbarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN the dynamic download dialog is shown WHEN the snackbar is shown THEN place the snackbar above the dialog`() {
        dependency.id = R.id.viewDynamicDownloadDialog
        val behavior = FenixSnackbarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        behavior.layoutDependsOn(parent, snackbarContainer, dependency)

        assertSnackbarPlacementAboveAnchor()
    }

    @Test
    fun `GIVEN a bottom toolbar is shown WHEN the snackbar is shown THEN place the snackbar above the toolbar`() {
        dependency.id = R.id.toolbar
        val behavior = FenixSnackbarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        behavior.layoutDependsOn(parent, snackbarContainer, dependency)

        assertSnackbarPlacementAboveAnchor()
    }

    @Test
    fun `GIVEN a toolbar and a dynamic download dialog are shown WHEN the snackbar is shown THEN place the snackbar above the dialog`() {
        listOf(R.id.viewDynamicDownloadDialog, R.id.toolbar).forEach {
            parent.addView(View(testContext).apply { id = it })
        }
        val behavior = FenixSnackbarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        behavior.layoutDependsOn(parent, snackbarContainer, dependency)

        assertSnackbarPlacementAboveAnchor(parent.findViewById(R.id.viewDynamicDownloadDialog))
    }

    @Test
    fun `GIVEN the snackbar is anchored to the dynamic download dialog and a bottom toolbar is shown WHEN the dialog is not shown anymore THEN place the snackbar above the toolbar`() {
        val dialog = View(testContext)
            .apply { id = R.id.viewDynamicDownloadDialog }
            .also { parent.addView(it) }
        val toolbar = View(testContext)
            .apply { id = R.id.toolbar }
            .also { parent.addView(it) }
        val behavior = FenixSnackbarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        // Test the scenario where the dialog is invisible.
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarPlacementAboveAnchor(dialog)
        dialog.visibility = View.GONE
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarPlacementAboveAnchor(toolbar)

        // Test the scenario where the dialog is removed from parent.
        dialog.visibility = View.VISIBLE
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarPlacementAboveAnchor(dialog)
        parent.removeView(dialog)
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarPlacementAboveAnchor(toolbar)
    }

    @Test
    fun `GIVEN the snackbar is anchored to the bottom toolbar WHEN the toolbar is not shown anymore THEN place the snackbar at the bottom`() {
        val toolbar = View(testContext)
            .apply { id = R.id.toolbar }
            .also { parent.addView(it) }
        val behavior = FenixSnackbarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        // Test the scenario where the toolbar is invisible.
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarPlacementAboveAnchor(toolbar)
        toolbar.visibility = View.GONE
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarIsPlacedAtTheBottomOfTheScreen()

        // Test the scenario where the toolbar is removed from parent.
        toolbar.visibility = View.VISIBLE
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarPlacementAboveAnchor(toolbar)
        parent.removeView(toolbar)
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN the snackbar is anchored to the dynamic download dialog and a top toolbar is shown WHEN the dialog is not shown anymore THEN place the snackbar to the bottom`() {
        val dialog = View(testContext)
            .apply { id = R.id.viewDynamicDownloadDialog }
            .also { parent.addView(it) }
        View(testContext)
            .apply { id = R.id.toolbar }
            .also { parent.addView(it) }
        val behavior = FenixSnackbarBehavior<ViewGroup>(testContext, ToolbarPosition.TOP)

        // Test the scenario where the dialog is invisible.
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarPlacementAboveAnchor(dialog)
        dialog.visibility = View.GONE
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarIsPlacedAtTheBottomOfTheScreen()

        // Test the scenario where the dialog is removed from parent.
        dialog.visibility = View.VISIBLE
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarPlacementAboveAnchor(dialog)
        parent.removeView(dialog)
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN the snackbar is anchored based on a top toolbar WHEN the toolbar is not shown anymore THEN place the snackbar at the bottom`() {
        val toolbar = View(testContext)
            .apply { id = R.id.toolbar }
            .also { parent.addView(it) }
        val behavior = FenixSnackbarBehavior<ViewGroup>(testContext, ToolbarPosition.TOP)

        // Test the scenario where the toolbar is invisible.
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarIsPlacedAtTheBottomOfTheScreen()
        toolbar.visibility = View.GONE
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarIsPlacedAtTheBottomOfTheScreen()

        // Test the scenario where the toolbar is removed from parent.
        toolbar.visibility = View.VISIBLE
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarIsPlacedAtTheBottomOfTheScreen()
        parent.removeView(toolbar)
        behavior.layoutDependsOn(parent, snackbarContainer, dependency)
        assertSnackbarIsPlacedAtTheBottomOfTheScreen()
    }

    private fun assertSnackbarPlacementAboveAnchor(anchor: View = dependency) {
        assertEquals(anchor.id, snackbarContainer.params.anchorId)
        assertEquals(Gravity.TOP or Gravity.CENTER_HORIZONTAL, snackbarContainer.params.anchorGravity)
        assertEquals(Gravity.TOP or Gravity.CENTER_HORIZONTAL, snackbarContainer.params.gravity)
    }

    private fun assertSnackbarIsPlacedAtTheBottomOfTheScreen() {
        assertEquals(View.NO_ID, snackbarContainer.params.anchorId)
        assertEquals(Gravity.NO_GRAVITY, snackbarContainer.params.anchorGravity)
        assertEquals(Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL, snackbarContainer.params.gravity)
    }

    private val FrameLayout.params
        get() = layoutParams as CoordinatorLayout.LayoutParams
}
