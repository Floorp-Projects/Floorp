/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.content.DialogInterface
import android.os.Looper.getMainLooper
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.prompts.R
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.openMocks
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class ColorPickerDialogFragmentTest {

    @Mock private lateinit var mockFeature: Prompter

    @Before
    fun setup() {
        openMocks(this)
    }

    @Test
    fun `build dialog`() {
        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "uid", true, "#e66465"),
        )

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        dialog.show()

        assertEquals(fragment.sessionId, "sessionId")
        assertEquals(fragment.promptRequestUID, "uid")
        assertEquals(fragment.selectedColor.toHexColor(), "#e66465")
    }

    @Test
    fun `clicking on positive button notifies the feature`() {
        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "uid", false, "#e66465"),
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        fragment.onColorChange("#4f4663".toColor())

        val positiveButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_POSITIVE)
        positiveButton.performClick()
        shadowOf(getMainLooper()).idle()

        verify(mockFeature).onConfirm("sessionId", "uid", "#4f4663")
    }

    @Test
    fun `clicking on negative button notifies the feature`() {
        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "uid", true, "#e66465"),
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        val negativeButton = (dialog as AlertDialog).getButton(DialogInterface.BUTTON_NEGATIVE)
        negativeButton.performClick()
        shadowOf(getMainLooper()).idle()

        verify(mockFeature).onCancel("sessionId", "uid")
    }

    @Test
    fun `touching outside of the dialog must notify the feature onCancel`() {
        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "uid", false, "#e66465"),
        )

        fragment.feature = mockFeature

        doReturn(appCompatContext).`when`(fragment).requireContext()

        fragment.onCancel(mock())

        verify(mockFeature).onCancel("sessionId", "uid")
    }

    @Test
    fun `will show a color item`() {
        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "uid", true, "#e66465"),
        )
        doReturn(appCompatContext).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)

        val selectedColor = appCompatContext.resources
            .obtainTypedArray(R.array.mozac_feature_prompts_default_colors).let {
                it.getColor(0, 0)
            }

        assertEquals(selectedColor, holder.color)
    }

    @Test
    fun `clicking on a item will update the selected color`() {
        val fragment = spy(
            ColorPickerDialogFragment.newInstance("sessionId", "uid", false, "#e66465"),
        )
        doReturn(appCompatContext).`when`(fragment).requireContext()

        val adapter = getAdapterFrom(fragment)
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), 0)
        val colorItem = holder.itemView as TextView
        adapter.bindViewHolder(holder, 0)

        colorItem.performClick()

        val selectedColor = appCompatContext.resources
            .obtainTypedArray(R.array.mozac_feature_prompts_default_colors).let {
                it.getColor(0, 0)
            }
        assertEquals(fragment.selectedColor, selectedColor)
    }

    private fun getAdapterFrom(fragment: ColorPickerDialogFragment): BasicColorAdapter {
        val view = fragment.createDialogContentView()
        val recyclerViewId = R.id.recyclerView

        return view.findViewById<RecyclerView>(recyclerViewId).adapter as BasicColorAdapter
    }
}
