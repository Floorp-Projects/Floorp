/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.selection

import android.app.Activity
import android.app.Application
import android.app.Service
import android.view.MenuItem
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.mock
import org.junit.Assert
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class GeckoSelectionActionDelegateTest {

    @Test
    fun `maybe create with non-activity context should return null`() {
        val customDelegate = mock<SelectionActionDelegate>()

        assertNull(GeckoSelectionActionDelegate.maybeCreate(mock<Application>(), customDelegate))
        assertNull(GeckoSelectionActionDelegate.maybeCreate(mock<Service>(), customDelegate))
    }

    @Test
    fun `maybe create with null delegate context should return null`() {
        assertNull(GeckoSelectionActionDelegate.maybeCreate(mock<Activity>(), null))
    }

    @Test
    fun `maybe create with expected inputs should return non-null`() {
        assertNotNull(GeckoSelectionActionDelegate.maybeCreate(mock<Activity>(), mock()))
    }

    @Test
    fun `getAllActions should contain all actions from the custom delegate`() {
        val customActions = arrayOf("1", "2", "3")
        val customDelegate = object : SelectionActionDelegate {
            override fun getAllActions(): Array<String> = customActions
            override fun isActionAvailable(id: String, selectedText: String): Boolean = false
            override fun getActionTitle(id: String): CharSequence? = ""
            override fun performAction(id: String, selectedText: String): Boolean = false
            override fun sortedActions(actions: Array<String>): Array<String> {
                return actions
            }
        }

        val geckoDelegate = TestGeckoSelectionActionDelegate(mock(), customDelegate)

        val actualActions = geckoDelegate.allActions

        customActions.forEach {
            Assert.assertTrue(actualActions.contains(it))
        }
    }

    @Test
    fun `WHEN perform action triggers a security exception THEN false is returned`() {
        val customActions = arrayOf("1", "2", "3")
        val customDelegate = object : SelectionActionDelegate {
            override fun getAllActions(): Array<String> = customActions
            override fun isActionAvailable(id: String, selectedText: String): Boolean = false
            override fun getActionTitle(id: String): CharSequence? = ""
            override fun performAction(id: String, selectedText: String): Boolean {
                throw SecurityException("test")
            }
            override fun sortedActions(actions: Array<String>): Array<String> {
                return actions
            }
        }

        val geckoDelegate = TestGeckoSelectionActionDelegate(mock(), customDelegate)
        assertFalse(geckoDelegate.performAction("test", mock()))
    }
}

/**
 * Test object that overrides visibility for [getAllActions]
 */
class TestGeckoSelectionActionDelegate(
    activity: Activity,
    customDelegate: SelectionActionDelegate,
) : GeckoSelectionActionDelegate(activity, customDelegate) {
    public override fun getAllActions() = super.getAllActions()
    public override fun performAction(id: String, item: MenuItem): Boolean {
        return super.performAction(id, item)
    }
}
