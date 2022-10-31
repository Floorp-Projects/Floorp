package mozilla.components.feature.privatemode.notification

import android.content.Context
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class PrivateNotificationFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var context: Context
    private lateinit var store: BrowserStore
    private lateinit var feature: PrivateNotificationFeature<AbstractPrivateNotificationService>

    @Before
    fun setup() {
        context = mock()
        whenever(context.applicationContext).thenReturn(context)
        whenever(context.packageName).thenReturn(testContext.packageName)

        store = BrowserStore()

        feature = PrivateNotificationFeature(context, store, AbstractPrivateNotificationService::class)
    }

    @Test
    fun `service should be started if pre-existing private session is present`() = runTest(StandardTestDispatcher()) {
        val privateSession = createTab("https://firefox.com", private = true)
        val intent = argumentCaptor<Intent>()

        store.dispatch(TabListAction.AddTabAction(privateSession)).join()

        feature.start()
        runCurrent()
        verify(context, times(1)).startService(intent.capture())

        val expected = Intent(testContext, AbstractPrivateNotificationService::class.java)
        assertEquals(expected.component, intent.value.component)
        assertTrue(expected.filterEquals(intent.value))
    }

    @Test
    fun `service should be started when private session is added`() = runTestOnMain {
        val privateSession = createTab("https://firefox.com", private = true)

        feature.start()
        verify(context, never()).startService(any())

        store.dispatch(TabListAction.AddTabAction(privateSession)).join()
        verify(context, times(1)).startService(any())
        Unit
    }

    @Test
    fun `service should not be started multiple times`() = runTestOnMain {
        val privateSession1 = createTab("https://firefox.com", private = true)
        val privateSession2 = createTab("https://mozilla.org", private = true)

        feature.start()

        store.dispatch(TabListAction.AddTabAction(privateSession1)).join()
        store.dispatch(TabListAction.AddTabAction(privateSession2)).join()

        verify(context, times(1)).startService(any())
        Unit
    }

    @Test
    fun `notification service should not be started when normal sessions are added`() = runTestOnMain {
        val normalSession = createTab("https://firefox.com")
        val customSession = createCustomTab("https://firefox.com")

        feature.start()
        verify(context, never()).startService(any())

        store.dispatch(TabListAction.AddTabAction(normalSession)).join()
        verify(context, never()).startService(any())

        store.dispatch(CustomTabListAction.AddCustomTabAction(customSession)).join()
        verify(context, never()).startService(any())
        Unit
    }

    @Test
    fun `notification service should not be started when custom sessions are added`() = runTestOnMain {
        val privateCustomSession = createCustomTab("https://firefox.com").let {
            it.copy(content = it.content.copy(private = true))
        }
        val customSession = createCustomTab("https://firefox.com")

        feature.start()
        verify(context, never()).startService(any())

        store.dispatch(CustomTabListAction.AddCustomTabAction(privateCustomSession)).join()
        verify(context, never()).startService(any())

        store.dispatch(CustomTabListAction.AddCustomTabAction(customSession)).join()
        verify(context, never()).startService(any())
        Unit
    }
}
