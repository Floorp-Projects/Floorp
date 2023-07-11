/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share
import android.content.Context
import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.geckoview.GeckoSession
import java.io.IOException

@RunWith(FenixRobolectricTestRunner::class)
class SaveToPDFMiddlewareTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @get:Rule
    val mainCoroutineTestRule = MainCoroutineRule()

    // Only ERROR_PRINT_SETTINGS_SERVICE_NOT_AVAILABLE is available for testing
    class MockGeckoPrintException() : GeckoSession.GeckoPrintException()

    @Test
    fun `GIVEN a save to pdf request WHEN it fails unexpectedly THEN unknown failure telemetry is sent`() = runTestOnMain {
        val exceptionToThrow = RuntimeException("reader save to pdf failed")
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                secondArg<(Throwable) -> Unit>().invoke(exceptionToThrow)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(
            EngineAction.SaveToPdfExceptionAction("14", exceptionToThrow),
        )
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.saveToPdfFailure.testGetValue()?.firstOrNull()
        assertNotNull(response)
        val reason = response?.extra?.get("reason")
        assertEquals("unknown", reason)
        val source = response?.extra?.get("source")
        assertEquals("unknown", source)
    }

    @Test
    fun `GIVEN a save to pdf request WHEN it fails due to io THEN io failure telemetry is sent`() = runTestOnMain {
        val exceptionToThrow = IOException()
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                secondArg<(Throwable) -> Unit>().invoke(exceptionToThrow)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(EngineAction.SaveToPdfExceptionAction("14", exceptionToThrow))
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.saveToPdfFailure.testGetValue()?.firstOrNull()
        assertNotNull(response)
        val reason = response?.extra?.get("reason")
        assertEquals("io_error", reason)
        val source = response?.extra?.get("source")
        assertEquals("unknown", source)
    }

    @Test
    fun `GIVEN a save to pdf request WHEN it fails due to print exception THEN print exception failure telemetry is sent`() = runTestOnMain {
        val exceptionToThrow = MockGeckoPrintException()
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                secondArg<(Throwable) -> Unit>().invoke(exceptionToThrow)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(EngineAction.SaveToPdfExceptionAction("14", exceptionToThrow))
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.saveToPdfFailure.testGetValue()?.firstOrNull()
        assertNotNull(response)
        val reason = response?.extra?.get("reason")
        assertEquals("no_settings_service", reason)
        val source = response?.extra?.get("source")
        assertEquals("unknown", source)
    }

    @Test
    fun `GIVEN a save to pdf request WHEN it completes THEN completed telemetry is sent`() = runTestOnMain {
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                firstArg<(Boolean) -> Unit>().invoke(false)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(EngineAction.SaveToPdfCompleteAction("14"))
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.saveToPdfCompleted.testGetValue()
        assertNotNull(response)
        val source = response?.firstOrNull()?.extra?.get("source")
        assertEquals("non-pdf", source)
    }

    @Test
    fun `GIVEN a save to pdf request WHEN it the action begins THEN tapped telemetry is sent`() = runTestOnMain {
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                firstArg<(Boolean) -> Unit>().invoke(false)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(EngineAction.SaveToPdfAction("14"))
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.saveToPdfTapped.testGetValue()
        assertNotNull(response)
        val source = response?.firstOrNull()?.extra?.get("source")
        assertEquals("non-pdf", source)
    }

    @Test
    fun `GIVEN a save as pdf exception THEN should calculate the correct failure reason for telemetry`() = runTestOnMain {
        val mockContext: Context = mock()
        val middleware = SaveToPDFMiddleware(mockContext)
        val noSettingsService = middleware.telemetryErrorReason(MockGeckoPrintException())
        assertEquals("no_settings_service", noSettingsService)
        val ioException = middleware.telemetryErrorReason(IOException())
        assertEquals("io_error", ioException)
        val other = middleware.telemetryErrorReason(Exception())
        assertEquals("unknown", other)
    }

    @Test
    fun `GIVEN a save as pdf page type THEN should calculate the correct page source for telemetry`() = runTestOnMain {
        val mockContext: Context = mock()
        val middleware = SaveToPDFMiddleware(mockContext)
        assertEquals("pdf", middleware.telemetrySource(isPdfViewer = true))
        assertEquals("non-pdf", middleware.telemetrySource(isPdfViewer = false))
        assertEquals("unknown", middleware.telemetrySource(isPdfViewer = null))
    }

    @Test
    fun `GIVEN a print request WHEN it fails unexpectedly THEN unknown failure telemetry is sent`() = runTestOnMain {
        val exceptionToThrow = RuntimeException("No Print Spooler")
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                secondArg<(Throwable) -> Unit>().invoke(exceptionToThrow)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(
            EngineAction.PrintContentExceptionAction("14", true, exceptionToThrow),
        )
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.printFailure.testGetValue()?.firstOrNull()
        assertNotNull(response)
        val reason = response?.extra?.get("reason")
        assertEquals("unknown", reason)
        val source = response?.extra?.get("source")
        assertEquals("unknown", source)
    }

    @Test
    fun `GIVEN a print request WHEN it fails due to print exception THEN print exception failure telemetry is sent`() = runTestOnMain {
        val exceptionToThrow = MockGeckoPrintException()
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                secondArg<(Throwable) -> Unit>().invoke(exceptionToThrow)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(EngineAction.PrintContentExceptionAction("14", true, exceptionToThrow))
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.printFailure.testGetValue()?.firstOrNull()
        assertNotNull(response)
        val reason = response?.extra?.get("reason")
        assertEquals("no_settings_service", reason)
        val source = response?.extra?.get("source")
        assertEquals("unknown", source)
    }

    @Test
    fun `GIVEN a print request WHEN it completes THEN completed telemetry is sent`() = runTestOnMain {
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                firstArg<(Boolean) -> Unit>().invoke(true)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(EngineAction.PrintContentCompletedAction("14"))
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.printCompleted.testGetValue()
        assertNotNull(response)
        val source = response?.firstOrNull()?.extra?.get("source")
        assertEquals("pdf", source)
    }

    @Test
    fun `GIVEN a print request WHEN it the action begins THEN tapped telemetry is sent`() = runTestOnMain {
        val middleware = SaveToPDFMiddleware(testContext)
        val mockEngineSession: EngineSession = mockk<EngineSession>().apply {
            every {
                checkForPdfViewer(any(), any())
            } answers {
                firstArg<(Boolean) -> Unit>().invoke(false)
            }
        }
        val browserStore = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "14",
                        engineSession = mockEngineSession,
                    ),
                ),
            ),
        )
        browserStore.dispatch(EngineAction.PrintContentAction("14"))
        browserStore.waitUntilIdle()
        testScheduler.advanceUntilIdle()
        val response = Events.printTapped.testGetValue()
        assertNotNull(response)
        val source = response?.firstOrNull()?.extra?.get("source")
        assertEquals("non-pdf", source)
    }
}
