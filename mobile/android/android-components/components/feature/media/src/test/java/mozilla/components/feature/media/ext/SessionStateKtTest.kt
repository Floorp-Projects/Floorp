package mozilla.components.feature.media.ext

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SessionStateKtTest {
    private val bitmap: Bitmap = mock()
    private val getArtwork: (suspend () -> Bitmap?) = { bitmap }
    private val getArtworkNull: (suspend () -> Bitmap?) = { null }

    @Test
    fun `getNonPrivateIcon returns null when in private mode`() = runTest {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(true)

        val result = sessionState.getNonPrivateIcon(getArtwork)

        assertEquals(result, null)
    }

    @Test
    fun `getNonPrivateIcon returns bitmap when not in private mode`() = runTest {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(false)

        val result = sessionState.getNonPrivateIcon(getArtwork)

        assertEquals(result, bitmap)
    }

    @Test
    fun `getNonPrivateIcon returns content icon when not in private mode`() = runTest {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val icon: Bitmap = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(false)
        whenever(contentState.icon).thenReturn(icon)

        val result = sessionState.getNonPrivateIcon(null)

        assertEquals(result, icon)
    }

    @Test
    fun `getNonPrivateIcon returns content icon when getArtwork return null`() = runTest {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val icon: Bitmap = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(false)
        whenever(contentState.icon).thenReturn(icon)

        val result = sessionState.getNonPrivateIcon(getArtworkNull)

        assertEquals(result, icon)
    }

    @Test
    fun `getTitleOrUrl returns null when in private mode`() {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(true)

        val result = sessionState.getTitleOrUrl(testContext, "test")

        assertEquals(result, "A site is playing media")
    }

    @Test
    fun `getTitleOrUrl returns metadata title when not in private mode`() {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(false)

        val result = sessionState.getTitleOrUrl(testContext, "test")

        assertEquals(result, "test")
    }

    @Test
    fun `getTitleOrUrl returns title when not in private mode`() {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val contentTitle = "content title"
        val contentUrl = "content url"

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(false)
        whenever(contentState.title).thenReturn(contentTitle)
        whenever(contentState.url).thenReturn(contentUrl)

        val result = sessionState.getTitleOrUrl(testContext, null)

        assertEquals(result, contentTitle)
    }

    @Test
    fun `getTitleOrUrl returns url when not in private mode`() {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val contentTitle = ""
        val contentUrl = "content url"

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(false)
        whenever(contentState.title).thenReturn(contentTitle)
        whenever(contentState.url).thenReturn(contentUrl)

        val result = sessionState.getTitleOrUrl(testContext, null)

        assertEquals(result, contentUrl)
    }

    @Test
    fun `getNotPrivateUrl returns null when in private mode`() {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val contentUrl = "content url"

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(true)
        whenever(contentState.url).thenReturn(contentUrl)

        val result = sessionState.nonPrivateUrl

        assertEquals(result, "")
    }

    @Test
    fun `nonPrivateUrl returns null when not in private mode`() {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val contentUrl = "content url"

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(false)
        whenever(contentState.url).thenReturn(contentUrl)

        val result = sessionState.nonPrivateUrl

        assertEquals(result, contentUrl)
    }

    @Test
    fun `nonPrivateIcon returns null when in private mode`() {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val icon: Bitmap = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(true)
        whenever(contentState.icon).thenReturn(icon)

        val result = sessionState.nonPrivateIcon

        assertEquals(result, null)
    }

    @Test
    fun `nonPrivateIcon returns null when not in private mode`() {
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val icon: Bitmap = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.private).thenReturn(false)
        whenever(contentState.icon).thenReturn(icon)

        val result = sessionState.nonPrivateIcon

        assertEquals(result, icon)
    }
}
