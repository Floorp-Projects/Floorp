/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.content.Context
import android.content.DialogInterface
import androidx.recyclerview.widget.RecyclerView
import android.view.ContextThemeWrapper
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.test.core.app.ApplicationProvider
import mozilla.components.feature.prompts.ColorPickerDialogFragment.ColorAdapter
import mozilla.components.feature.prompts.ColorPickerDialogFragment.ColorViewHolder
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ColorPickerDialogFragmentTest {

    private val context: Context
        get() = ContextThemeWrapper(
            ApplicationProvider.getApplicationContext<Context>(),
            androidx.appcompat.R.style.Theme_AppCompat
        )

    @Test
    fun `build dialog`() {

        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "#e66465")
        )

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        dialog.show()

        assertEquals(fragment.sessionId, "sessionId")
        assertEquals(fragment.selectedColor.toHexColor(), "#e66465")
    }

    @Test
    fun `clicking on positive button notifies the feature`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "#e66465")
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        fragment.onColorChange("#4f4663".toColor())

        val positiveButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_POSITIVE)
        positiveButton.performClick()

        verify(mockFeature).onConfirm("sessionId", "#4f4663")
    }

    @Test
    fun `clicking on negative button notifies the feature`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "#e66465")
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val negativeButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_NEGATIVE)
        negativeButton.performClick()

        verify(mockFeature).onCancel("sessionId")
    }

    @Test
    fun `touching outside of the dialog must notify the feature onCancel`() {

        val mockFeature: PromptFeature = mock()

        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "#e66465")
        )

        fragment.feature = mockFeature

        doReturn(context).`when`(fragment).requireContext()

        fragment.onCancel(null)

        verify(mockFeature).onCancel("sessionId")
    }

    @Test
    fun `will show a color item`() {

        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "#e66465")
        )
        doReturn(context).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)
        val holder = adapter.onCreateViewHolder(LinearLayout(context), 0) as ColorViewHolder
        val labelView = holder.itemView as TextView
        adapter.bindViewHolder(holder, 0)

        val selectedColor = adapter.defaultColors.first()
        assertEquals(labelView.tag as Int, selectedColor)
    }

    @Test
    fun `clicking on a item will update the selected color`() {

        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "#e66465")
        )
        doReturn(context).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)
        val holder = adapter.onCreateViewHolder(LinearLayout(context), 0) as ColorViewHolder
        val colorItem = holder.itemView as TextView
        adapter.bindViewHolder(holder, 0)

        colorItem.performClick()

        val selectedColor = adapter.defaultColors.first()
        assertEquals(fragment.selectedColor, selectedColor)
    }

    private fun getAdapterFrom(fragment: ColorPickerDialogFragment): ColorAdapter {
        val view = fragment.createDialogContentView()
        val recyclerViewId = R.id.recyclerView

        return view.findViewById<RecyclerView>(recyclerViewId).adapter as ColorAdapter
    }
}