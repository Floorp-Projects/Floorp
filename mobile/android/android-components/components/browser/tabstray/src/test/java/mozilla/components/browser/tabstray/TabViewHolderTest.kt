/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.expectException
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TabViewHolderTest : TestCase() {

    @Test
    fun `updateSelectedTabIndicator needs to have a provided implementation`() {
        val simpleTabViewHolder = object : TabViewHolder(View(testContext)) {
            override var tab: TabSessionState? = null
            override fun bind(
                tab: TabSessionState,
                isSelected: Boolean,
                styling: TabsTrayStyling,
                delegate: TabsTray.Delegate,
            ) { /* noop */ }
        }

        expectException(UnsupportedOperationException::class) {
            simpleTabViewHolder.updateSelectedTabIndicator(true)
        }
    }

    @Test
    fun `updateSelectedTabIndicator with a provided implementation just works`() {
        val tabViewHolder = object : TabViewHolder(View(testContext)) {
            override var tab: TabSessionState? = null
            override fun bind(
                tab: TabSessionState,
                isSelected: Boolean,
                styling: TabsTrayStyling,
                delegate: TabsTray.Delegate,
            ) { /* noop */ }
            override fun updateSelectedTabIndicator(showAsSelected: Boolean) { /* noop */ }
        }

        // Simply test that this would not fail the test like it would happen above.
        tabViewHolder.updateSelectedTabIndicator(true)
    }
}
