/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.content.DialogInterface.BUTTON_NEGATIVE
import android.content.DialogInterface.BUTTON_POSITIVE
import android.os.Parcelable
import android.view.LayoutInflater
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.Companion.TYPE_GROUP
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.Companion.TYPE_MENU
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.Companion.TYPE_MENU_SEPARATOR
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.Companion.TYPE_MULTIPLE
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.Companion.TYPE_SINGLE
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.GroupViewHolder
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.MenuViewHolder
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.MultipleViewHolder
import mozilla.components.feature.prompts.dialog.ChoiceAdapter.SingleViewHolder
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment.Companion.MENU_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment.Companion.MULTIPLE_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment.Companion.SINGLE_CHOICE_DIALOG_TYPE
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment.Companion.newInstance
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.initMocks

@RunWith(AndroidJUnit4::class)
class ChoiceDialogFragmentTest {

    @Mock private lateinit var mockFeature: Prompter
    private val item = Choice(id = "", label = "item1")
    private val subItem = Choice(id = "", label = "sub-item1")
    private val separator = Choice(id = "", label = "item1", isASeparator = true)

    @Before
    fun setup() {
        initMocks(this)
    }

    @Test
    fun `Build single choice dialog`() {

        val fragment = spy(newInstance(arrayOf(), "sessionId", SINGLE_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        assertNotNull(dialog)
    }

    @Test
    fun `cancelling the dialog cancels the feature`() {
        val fragment = spy(newInstance(arrayOf(), "sessionId", SINGLE_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        doNothing().`when`(fragment).dismiss()

        assertNotNull(dialog)

        dialog.show()

        fragment.onCancel(dialog)

        verify(mockFeature).onCancel("sessionId")
    }

    @Test
    fun `Build menu choice dialog`() {

        val fragment = spy(newInstance(arrayOf(), "sessionId", MENU_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        assertNotNull(dialog)
    }

    @Test
    fun `Build multiple choice dialog`() {

        val fragment = spy(newInstance(arrayOf(), "sessionId", MULTIPLE_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        assertNotNull(dialog)
    }

    @Test(expected = Exception::class)
    fun `Building a unknown dialog type will throw an exception`() {

        val fragment = spy(newInstance(arrayOf(), "sessionId", -1))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        fragment.onCreateDialog(null)
    }

    @Test
    fun `Will show a single choice item`() {

        val choices = arrayOf(item)

        val fragment = spy(newInstance(choices, "sessionId", SINGLE_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_SINGLE) as SingleViewHolder
        val labelView = holder.labelView
        adapter.bindViewHolder(holder, 0)

        assertEquals(1, adapter.itemCount)
        assertEquals("item1", labelView.text)
    }

    @Test
    fun `Will show a menu choice item`() {

        val choices = arrayOf(item)

        val fragment = spy(newInstance(choices, "sessionId", MENU_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_MENU) as MenuViewHolder
        val labelView = holder.labelView
        adapter.bindViewHolder(holder, 0)

        assertEquals(1, adapter.itemCount)
        assertEquals("item1", labelView.text)
    }

    @Test
    fun `Will show a menu choice separator item`() {

        val choices = arrayOf(separator)

        val fragment = spy(newInstance(choices, "sessionId", MENU_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_MENU_SEPARATOR)
        adapter.bindViewHolder(holder, 0)

        assertEquals(1, adapter.itemCount)
        assertNotNull(holder.itemView)
    }

    @Test(expected = Exception::class)
    fun `Will throw an exception to try to create a invalid choice type item`() {

        val choices = arrayOf(separator)

        val fragment = spy(newInstance(choices, "sessionId", MENU_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)
        adapter.onCreateViewHolder(LinearLayout(testContext), -1)
    }

    @Test
    fun `Will adapter will return correct view type `() {

        val choices = arrayOf(
            item,
            Choice(id = "", label = "item1", children = arrayOf()),
            Choice(id = "", label = "menu", children = arrayOf()),
            Choice(id = "", label = "separator", children = arrayOf(), isASeparator = true),
            Choice(id = "", label = "multiple choice")
        )

        var fragment = spy(newInstance(choices, "sessionId", SINGLE_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        var adapter = getAdapterFrom(fragment)
        var type = adapter.getItemViewType(0)

        assertEquals(type, TYPE_SINGLE)

        fragment = spy(newInstance(choices, "sessionId", MULTIPLE_CHOICE_DIALOG_TYPE))
        doReturn(appCompatContext).`when`(fragment).requireContext()

        adapter = getAdapterFrom(fragment)

        type = adapter.getItemViewType(1)
        assertEquals(type, TYPE_GROUP)

        fragment = spy(newInstance(choices, "sessionId", MENU_CHOICE_DIALOG_TYPE))
        doReturn(appCompatContext).`when`(fragment).requireContext()

        adapter = getAdapterFrom(fragment)

        type = adapter.getItemViewType(2)
        assertEquals(type, TYPE_MENU)

        type = adapter.getItemViewType(3)
        assertEquals(type, TYPE_MENU_SEPARATOR)

        fragment = spy(newInstance(choices, "sessionId", MULTIPLE_CHOICE_DIALOG_TYPE))
        doReturn(appCompatContext).`when`(fragment).requireContext()

        adapter = getAdapterFrom(fragment)

        type = adapter.getItemViewType(4)
        assertEquals(type, TYPE_MULTIPLE)
    }

    @Test
    fun `Will show a multiple choice item`() {

        val choices =
                arrayOf(Choice(id = "", label = "item1", children = arrayOf(Choice(id = "", label = "sub-item1"))))

        val fragment = spy(newInstance(choices, "sessionId", MULTIPLE_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)

        val holder =
            adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_MULTIPLE) as MultipleViewHolder
        val groupHolder = adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_GROUP) as GroupViewHolder

        adapter.bindViewHolder(holder, 0)
        adapter.bindViewHolder(groupHolder, 1)

        assertEquals(2, adapter.itemCount)
        assertEquals("item1", holder.labelView.text)
        assertEquals("sub-item1", groupHolder.labelView.text.trim())
    }

    @Test
    fun `Will show a multiple choice item with selected element`() {

        val choices = arrayOf(
                Choice(
                        id = "", label = "item1",
                        children = arrayOf(Choice(id = "", label = "sub-item1", selected = true))
                )
        )

        val fragment = spy(newInstance(choices, "sessionId", MULTIPLE_CHOICE_DIALOG_TYPE))

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)

        assertEquals(2, adapter.itemCount)

        val groupHolder = adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_GROUP) as GroupViewHolder
        val holder =
            adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_MULTIPLE) as MultipleViewHolder

        adapter.bindViewHolder(groupHolder, 0)
        adapter.bindViewHolder(holder, 1)

        assertEquals("item1", (groupHolder.labelView as TextView).text)
        assertEquals("sub-item1", holder.labelView.text.trim())
        assertEquals(true, holder.labelView.isChecked)
    }

    @Test
    fun `Clicking on single choice item notifies the feature`() {

        val choices = arrayOf(item)

        val fragment = spy(newInstance(choices, "sessionId", SINGLE_CHOICE_DIALOG_TYPE))

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()
        doNothing().`when`(fragment).dismiss()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val adapter = getAdapterFrom(fragment)

        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_SINGLE) as SingleViewHolder

        adapter.bindViewHolder(holder, 0)

        holder.itemView.performClick()

        verify(mockFeature).onConfirm("sessionId", choices.first())
        dialog.dismiss()
        verify(mockFeature).onCancel("sessionId")
    }

    @Test
    fun `Clicking on menu choice item notifies the feature`() {

        val choices = arrayOf(item)

        val fragment = spy(newInstance(choices, "sessionId", MENU_CHOICE_DIALOG_TYPE))

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()
        doNothing().`when`(fragment).dismiss()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val adapter = getAdapterFrom(fragment)

        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_MENU)

        adapter.bindViewHolder(holder, 0)

        holder.itemView.performClick()

        verify(mockFeature).onConfirm("sessionId", choices.first())
        dialog.dismiss()
        verify(mockFeature).onCancel("sessionId")
    }

    @Test
    fun `Clicking on multiple choice item notifies the feature`() {

        val choices =
                arrayOf(Choice(id = "", label = "item1", children = arrayOf(subItem)))
        val fragment = spy(newInstance(choices, "sessionId", MULTIPLE_CHOICE_DIALOG_TYPE))

        fragment.feature = mockFeature
        doReturn(appCompatContext).`when`(fragment).requireContext()
        doNothing().`when`(fragment).dismiss()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val adapter = dialog.findViewById<RecyclerView>(R.id.recyclerView).adapter as ChoiceAdapter

        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_MULTIPLE)

        adapter.bindViewHolder(holder, 1)

        holder.itemView.performClick()

        assertTrue(fragment.mapSelectChoice.isNotEmpty())

        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onConfirm("sessionId", fragment.mapSelectChoice.keys.toTypedArray())

        val negativeButton = dialog.getButton(BUTTON_NEGATIVE)
        negativeButton.performClick()

        verify(mockFeature, times(2)).onCancel("sessionId")
    }

    @Test
    fun `Clicking on selected multiple choice item will notify feature`() {

        val choices =
                arrayOf(item.copy(selected = true))
        val fragment = spy(newInstance(choices, "sessionId", MULTIPLE_CHOICE_DIALOG_TYPE))

        fragment.feature = mockFeature
        doReturn(appCompatContext).`when`(fragment).requireContext()
        doNothing().`when`(fragment).dismiss()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val adapter = dialog.findViewById<RecyclerView>(R.id.recyclerView).adapter as ChoiceAdapter

        val holder =
            adapter.onCreateViewHolder(LinearLayout(testContext), TYPE_MULTIPLE) as MultipleViewHolder

        adapter.bindViewHolder(holder, 0)

        assertTrue(holder.labelView.isChecked)

        holder.itemView.performClick()

        assertTrue(fragment.mapSelectChoice.isEmpty())

        val positiveButton = (dialog as AlertDialog).getButton(BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onConfirm("sessionId", fragment.mapSelectChoice.keys.toTypedArray())
    }

    @Test
    fun `single choice item with multiple sub-menu groups`() {

        val choices = arrayOf(
            Choice(
                id = "group1",
                label = "group1",
                children = arrayOf(Choice(id = "item_group_1", label = "item group 1"))
            ),
            Choice(
                id = "group2",
                label = "group2",
                children = arrayOf(Choice(id = "item_group_2", label = "item group 2"))
            )
        )

        val fragment = spy(newInstance(choices, "sessionId", SINGLE_CHOICE_DIALOG_TYPE))
        fragment.feature = mockFeature
        doReturn(appCompatContext).`when`(fragment).requireContext()
        doNothing().`when`(fragment).dismiss()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val adapter = dialog.findViewById<RecyclerView>(R.id.recyclerView).adapter as ChoiceAdapter

        val groupViewHolder = adapter.onCreateViewHolder(LinearLayout(testContext), adapter.getItemViewType(0))
            as GroupViewHolder

        adapter.bindViewHolder(groupViewHolder, 0)

        assertFalse(groupViewHolder.labelView.isEnabled)
        assertEquals(groupViewHolder.labelView.text, "group1")

        val singleViewHolder =
            adapter.onCreateViewHolder(LinearLayout(testContext), adapter.getItemViewType(1)) as SingleViewHolder

        adapter.bindViewHolder(singleViewHolder, 1)

        with(singleViewHolder) {
            assertTrue(labelView.isEnabled)

            val choiceGroup1 = choices[0].children!![0]
            assertEquals(labelView.text, choiceGroup1.label)

            itemView.performClick()
            verify(mockFeature).onConfirm("sessionId", choiceGroup1)
        }
    }

    @Test
    fun `scroll to selected item`() {
        // array of 20 choices; 10th one is selected
        val choices = Array(20) { index ->
            if (index == 10) {
                item.copy(selected = true, label = "selected")
            } else {
                item.copy(label = "item$index")
            }
        }
        val fragment = newInstance(choices, "sessionId", SINGLE_CHOICE_DIALOG_TYPE)
        val inflater = LayoutInflater.from(testContext)
        val dialog = fragment.createDialogContentView(inflater)
        val recyclerView = dialog.findViewById<RecyclerView>(R.id.recyclerView)
        val layoutManager = recyclerView.layoutManager as LinearLayoutManager
        // these two lines are a bit of a hack to get the layout manager to draw the view.
        recyclerView.measure(0, 0)
        recyclerView.layout(0, 0, 100, 250)

        val selectedItemIndex = layoutManager.findLastCompletelyVisibleItemPosition()
        assertEquals(10, selectedItemIndex)
    }

    @Test
    fun `test toArrayOfChoices`() {
        val parcelables = Array<Parcelable>(1) { Choice(id = "id", label = "label") }
        val choice = parcelables.toArrayOfChoices()
        assertNotNull(choice)
    }

    private fun getAdapterFrom(fragment: ChoiceDialogFragment): ChoiceAdapter {
        val inflater = LayoutInflater.from(testContext)
        val view = fragment.createDialogContentView(inflater)
        val recyclerViewId = R.id.recyclerView

        return view.findViewById<RecyclerView>(recyclerViewId).adapter as ChoiceAdapter
    }
}
