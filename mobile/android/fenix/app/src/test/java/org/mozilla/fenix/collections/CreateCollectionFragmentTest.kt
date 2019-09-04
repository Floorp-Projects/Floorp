package org.mozilla.fenix.collections

import androidx.fragment.app.testing.FragmentScenario
import androidx.fragment.app.testing.launchFragment
import androidx.test.ext.junit.runners.AndroidJUnit4
import assertk.assertThat
import assertk.assertions.isNotNull
import assertk.assertions.isNull
import assertk.assertions.isTrue
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication

import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class CreateCollectionFragmentTest {

    private lateinit var scenario: FragmentScenario<CreateCollectionFragment>

    @Before
    fun setUp() {
        scenario = launchFragment<CreateCollectionFragment>()
    }
    @Test
    fun `fragment is loading`() {
        assertThat(scenario).isNotNull()
    }

    @Test
    fun `fragment is recreating`() {
        scenario.recreate()
        assertThat(scenario).isNotNull()
    }

    @Test
    fun `creation dialog shows and can be dismissed`() {
        with(scenario) {
            onFragment { fragment ->
                assertThat(fragment.dialog).isNotNull()
                assertThat(fragment.requireDialog().isShowing).isTrue()
                fragment.dismiss()
                assertThat(fragment.dialog).isNull()
            }
        }
    }
}
