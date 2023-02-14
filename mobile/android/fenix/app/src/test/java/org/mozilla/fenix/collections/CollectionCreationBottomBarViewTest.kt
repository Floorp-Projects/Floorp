/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageButton
import android.widget.TextView
import androidx.core.view.isVisible
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class CollectionCreationBottomBarViewTest {

    private lateinit var bottomBarView: CollectionCreationBottomBarView
    private lateinit var interactor: CollectionCreationInteractor
    private lateinit var layout: ViewGroup
    private lateinit var iconButton: ImageButton
    private lateinit var textView: TextView
    private lateinit var saveButton: Button

    @Before
    fun setup() {
        interactor = mockk(relaxed = true)
        layout = spyk()
        iconButton = ImageButton(testContext)
        textView = TextView(testContext)
        saveButton = Button(testContext)

        bottomBarView = CollectionCreationBottomBarView(
            interactor,
            layout,
            iconButton,
            textView,
            saveButton,
        )
    }

    @Test
    fun testIconButtonUpdateForSelectTabs() {
        bottomBarView.update(SaveCollectionStep.SelectTabs, CollectionCreationState())

        verify { layout.setOnClickListener(null) }
        verify { layout.isClickable = false }

        assertEquals("Close", iconButton.contentDescription)
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_YES, iconButton.importantForAccessibility)

        iconButton.performClick()
        verify { interactor.close() }
    }

    @Test
    fun testIconButtonUpdateForSelectCollection() {
        bottomBarView.update(SaveCollectionStep.SelectCollection, CollectionCreationState())

        assertEquals(null, iconButton.contentDescription)
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, iconButton.importantForAccessibility)

        layout.performClick()
        verify { interactor.addNewCollection() }

        iconButton.performClick()
        verify { interactor.addNewCollection() }
    }

    @Test
    fun testTextViewUpdateForSelectTabs() {
        bottomBarView.update(
            SaveCollectionStep.SelectTabs,
            CollectionCreationState(
                selectedTabs = emptySet(),
            ),
        )
        assertEquals("Select tabs to save", textView.text)

        bottomBarView.update(
            SaveCollectionStep.SelectTabs,
            CollectionCreationState(
                selectedTabs = setOf(mockk()),
            ),
        )
        assertEquals("1 tab selected", textView.text)

        bottomBarView.update(
            SaveCollectionStep.SelectTabs,
            CollectionCreationState(
                selectedTabs = setOf(mockk(), mockk()),
            ),
        )
        assertEquals("2 tabs selected", textView.text)
    }

    @Test
    fun testTextViewUpdateForSelectCollection() {
        bottomBarView.update(SaveCollectionStep.SelectCollection, CollectionCreationState())

        assertEquals("Add new collection", textView.text)
    }

    @Test
    fun testSaveButtonUpdateForSelectTabs() {
        val collection = mockk<TabCollection>()
        val tabs = setOf<Tab>(mockk(), mockk())

        bottomBarView.update(
            SaveCollectionStep.SelectTabs,
            CollectionCreationState(
                selectedTabCollection = null,
                selectedTabs = emptySet(),
            ),
        )
        assertFalse(saveButton.isVisible)

        bottomBarView.update(
            SaveCollectionStep.SelectTabs,
            CollectionCreationState(
                selectedTabCollection = collection,
                selectedTabs = emptySet(),
            ),
        )
        assertFalse(saveButton.isVisible)

        bottomBarView.update(
            SaveCollectionStep.SelectTabs,
            CollectionCreationState(
                selectedTabCollection = null,
                selectedTabs = tabs,
            ),
        )
        assertTrue(saveButton.isVisible)
        saveButton.performClick()
        verify { interactor.saveTabsToCollection(tabs.toList()) }

        bottomBarView.update(
            SaveCollectionStep.SelectTabs,
            CollectionCreationState(
                selectedTabCollection = collection,
                selectedTabs = tabs,
            ),
        )
        assertTrue(saveButton.isVisible)
        saveButton.performClick()
        verify { interactor.selectCollection(collection, tabs.toList()) }
    }

    @Test
    fun testSaveButtonUpdateForSelectCollection() {
        bottomBarView.update(SaveCollectionStep.SelectCollection, CollectionCreationState())

        assertFalse(saveButton.isVisible)
    }
}
