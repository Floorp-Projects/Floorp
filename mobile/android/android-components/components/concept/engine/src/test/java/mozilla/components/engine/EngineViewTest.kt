/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.engine

import android.content.Context
import android.widget.FrameLayout
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.lang.ClassCastException

@RunWith(RobolectricTestRunner::class)
class EngineViewTest {
    @Test
    fun `asView method returns underlying Android view`() {
        val engineView = createDummyEngineView(RuntimeEnvironment.application)

        val view = engineView.asView()

        assertTrue(view is FrameLayout)
    }

    @Test(expected = ClassCastException::class)
    fun `asView method fails if class is not a view`() {
        val engineView = BrokenEngineView()
        engineView.asView()
    }

    private fun createDummyEngineView(context: Context): EngineView = DummyEngineView(context)

    open class DummyEngineView(context: Context?) : FrameLayout(context), EngineView {
        override fun render(session: EngineSession) {}
    }

    // Class it not actually a View!
    open class BrokenEngineView : EngineView {
        override fun render(session: EngineSession) {}
    }
}
