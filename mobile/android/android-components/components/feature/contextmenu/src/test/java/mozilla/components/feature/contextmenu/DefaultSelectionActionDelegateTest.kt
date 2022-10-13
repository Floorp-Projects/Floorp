package mozilla.components.feature.contextmenu

import android.content.res.Resources
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.search.SearchAdapter
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyBoolean
import org.mockito.Mockito.anyString
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DefaultSelectionActionDelegateTest {

    val selectedRegularText = "mozilla"
    val selectedEmailText = "test@mozilla.org"
    val selectedPhoneText = "555-5555"
    var lambdaValue: String? = null
    val shareClicked: (String) -> Unit = { lambdaValue = it }
    val emailClicked: (String) -> Unit = { lambdaValue = it }
    val phoneClicked: (String) -> Unit = { lambdaValue = it }

    @Before
    fun setup() {
        lambdaValue = null
    }

    @Test
    fun `are non-private regular actions available`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(false)
        }
        val delegate = DefaultSelectionActionDelegate(
            searchAdapter,
            getTestResources(),
            shareClicked,
            emailClicked,
            phoneClicked,
        )

        assertTrue(delegate.isActionAvailable(SEARCH, selectedRegularText))
        assertTrue(delegate.isActionAvailable(SHARE, selectedRegularText))
        assertFalse(delegate.isActionAvailable(SEARCH_PRIVATELY, selectedRegularText))
        assertFalse(delegate.isActionAvailable(EMAIL, selectedRegularText))
        assertFalse(delegate.isActionAvailable(CALL, selectedRegularText))
    }

    @Test
    fun `are non-private non-share actions available`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(false)
        }
        val delegate = DefaultSelectionActionDelegate(
            searchAdapter,
            getTestResources(),
        )

        assertTrue(delegate.isActionAvailable(SEARCH, selectedRegularText))
        assertFalse(delegate.isActionAvailable(SHARE, selectedRegularText))
        assertFalse(delegate.isActionAvailable(SEARCH_PRIVATELY, selectedRegularText))
    }

    @Test
    fun `is email available when passed in and email text selected`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(false)
        }
        val delegate = DefaultSelectionActionDelegate(
            searchAdapter,
            getTestResources(),
            emailTextClicked = emailClicked,
        )

        assertTrue(delegate.isActionAvailable(EMAIL, selectedEmailText))
        assertTrue(delegate.isActionAvailable(EMAIL, " $selectedEmailText "))
        assertFalse(delegate.isActionAvailable(EMAIL, selectedRegularText))
        assertFalse(delegate.isActionAvailable(EMAIL, selectedPhoneText))
        assertFalse(delegate.isActionAvailable(EMAIL, " $selectedPhoneText "))
    }

    @Test
    fun `is call available when passed in and call text selected`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(false)
        }
        val delegate = DefaultSelectionActionDelegate(
            searchAdapter,
            getTestResources(),
            callTextClicked = phoneClicked,
        )

        assertTrue(delegate.isActionAvailable(CALL, selectedPhoneText))
        assertFalse(delegate.isActionAvailable(CALL, selectedRegularText))
        assertFalse(delegate.isActionAvailable(CALL, selectedEmailText))
    }

    @Test
    fun `are private actions available`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(true)
        }
        val delegate = DefaultSelectionActionDelegate(
            searchAdapter,
            getTestResources(),
            shareClicked,
        )

        assertTrue(delegate.isActionAvailable(SEARCH_PRIVATELY, selectedRegularText))
        assertTrue(delegate.isActionAvailable(SHARE, selectedRegularText))
        assertFalse(delegate.isActionAvailable(SEARCH, selectedRegularText))
    }

    @Test
    fun `when share ID is passed to perform action it should invoke the lambda`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        delegate.performAction(SHARE, "some selected text")

        assertEquals(lambdaValue, "some selected text")
    }

    @Test
    fun `when email ID is passed to perform action it should invoke the lambda`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), emailTextClicked = emailClicked)

        delegate.performAction(EMAIL, selectedEmailText)

        assertEquals(lambdaValue, selectedEmailText)

        delegate.performAction(EMAIL, " $selectedEmailText ")

        assertEquals(lambdaValue, selectedEmailText)
    }

    @Test
    fun `when call ID is passed to perform action it should invoke the lambda`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), callTextClicked = phoneClicked)

        delegate.performAction(CALL, selectedPhoneText)

        assertEquals(lambdaValue, selectedPhoneText)

        delegate.performAction(CALL, " $selectedPhoneText ")

        assertEquals(lambdaValue, selectedPhoneText)
    }

    @Test
    fun `when unknown ID is passed to performAction it should not perform a search`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        delegate.performAction("unrecognized string", "some selected text")

        verify(adapter, times(0)).sendSearch(anyBoolean(), anyString())
    }

    @Test
    fun `when unknown ID is passed to performAction it not consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        val result = delegate.performAction("unrecognized string", "some selected text")

        assertFalse(result)
    }

    @Test
    fun `when search ID is passed to performAction it should perform a search`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        delegate.performAction(SEARCH, "some selected text")

        verify(adapter, times(1)).sendSearch(false, "some selected text")
    }

    @Test
    fun `when search ID is passed to performAction it should consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        val result = delegate.performAction(SEARCH, "some selected text")

        assertTrue(result)
    }

    @Test
    fun `when private search ID is passed to performAction it should perform a private normal search`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        delegate.performAction(SEARCH_PRIVATELY, "some selected text")

        verify(adapter, times(1)).sendSearch(true, "some selected text")
    }

    @Test
    fun `when private search ID is passed to performAction it should consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        val result = delegate.performAction(SEARCH_PRIVATELY, "some selected text")

        assertTrue(result)
    }

    @Test
    fun `when calling performAction check that Facts are emitted`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        assertEquals(0, facts.size)

        delegate.performAction(SEARCH_PRIVATELY, "some selected text")

        assertEquals(1, facts.size)

        delegate.performAction(CALL, selectedPhoneText)

        assertEquals(2, facts.size)
    }
}

fun getTestResources() = mock<Resources> {
    whenever(getString(R.string.mozac_selection_context_menu_search_2)).thenReturn("Search")
    whenever(getString(R.string.mozac_selection_context_menu_search_privately_2))
        .thenReturn("search privately")
    whenever(getString(R.string.mozac_selection_context_menu_share)).thenReturn("share")
    whenever(getString(R.string.mozac_selection_context_menu_email)).thenReturn("email")
    whenever(getString(R.string.mozac_selection_context_menu_call)).thenReturn("call")
}
