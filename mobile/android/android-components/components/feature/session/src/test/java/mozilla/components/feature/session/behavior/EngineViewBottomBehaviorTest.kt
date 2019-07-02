/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.behavior

import android.content.Context
import android.graphics.Bitmap
import android.view.View
import android.widget.EditText
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class EngineViewBottomBehaviorTest {

    @Test
    fun `EngineView clipping and toolbar offset are kept in sync`() {
        val behavior = EngineViewBottomBehavior(mock(), null)

        val engineView: EngineView = spy(FakeEngineView(testContext))
        val toolbar: View = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(42f).`when`(toolbar).translationY
        behavior.onDependentViewChanged(mock(), engineView.asView(), toolbar)
        verify(engineView).setVerticalClipping(58)
    }

    @Test
    fun `Behavior does not depend on normal views`() {
        val behavior = EngineViewBottomBehavior(mock(), null)

        assertFalse(behavior.layoutDependsOn(mock(), mock(), TextView(testContext)))
        assertFalse(behavior.layoutDependsOn(mock(), mock(), EditText(testContext)))
        assertFalse(behavior.layoutDependsOn(mock(), mock(), ImageView(testContext)))
    }

    @Test
    fun `Behavior depends on BrowserToolbar`() {
        val behavior = EngineViewBottomBehavior(mock(), null)

        assertTrue(behavior.layoutDependsOn(mock(), mock(), BrowserToolbar(testContext)))
    }
}

class FakeEngineView(context: Context) : TextView(context), EngineView {
    override fun render(session: EngineSession) {}

    override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) {}

    override fun setVerticalClipping(clippingHeight: Int) {}

    override fun release() {}
}

class BrowserToolbar(context: Context) : TextView(context)
