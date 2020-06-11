package org.mozilla.fenix.collections

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyAll
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.tab.collections.TabCollection
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.home.Tab

@ExperimentalCoroutinesApi
class DefaultCollectionCreationControllerTest {

    private val testCoroutineScope = TestCoroutineScope()
    private lateinit var state: CollectionCreationState
    private lateinit var controller: DefaultCollectionCreationController

    @MockK(relaxed = true) private lateinit var store: CollectionCreationStore
    @MockK(relaxed = true) private lateinit var dismiss: () -> Unit
    @MockK(relaxUnitFun = true) private lateinit var metrics: MetricController
    @MockK(relaxUnitFun = true) private lateinit var tabCollectionStorage: TabCollectionStorage
    @MockK private lateinit var sessionManager: SessionManager

    @Before
    fun before() {
        MockKAnnotations.init(this)

        state = CollectionCreationState(
            tabCollections = emptyList(),
            tabs = emptyList()
        )
        every { store.state } answers { state }

        controller = DefaultCollectionCreationController(
            store, dismiss, metrics,
            tabCollectionStorage, sessionManager, testCoroutineScope
        )
    }

    @Test
    fun `GIVEN tab list WHEN saveCollectionName is called THEN collection should be created`() {
        val session = mockSession(sessionId = "session-1")
        val sessions = listOf(
            session,
            mockSession(sessionId = "session-2")
        )
        every { sessionManager.findSessionById("session-1") } returns session
        every { sessionManager.findSessionById("null-session") } returns null
        every { sessionManager.sessions } returns sessions
        val tabs = listOf(
            Tab("session-1", "", "", "", mediaState = MediaState.State.NONE),
            Tab("null-session", "", "", "", mediaState = MediaState.State.NONE)
        )

        controller.saveCollectionName(tabs, "name")

        verify { dismiss() }
        verify { tabCollectionStorage.createCollection("name", listOf(session)) }
        verify { metrics.track(Event.CollectionSaved(2, 1)) }
    }

    @Test
    fun `GIVEN name collection WHEN backPressed is called THEN next step should be dispatched`() {
        state = state.copy(tabCollections = listOf(mockk()))
        controller.backPressed(SaveCollectionStep.NameCollection)
        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectCollection)) }

        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk(), mockk()))
        controller.backPressed(SaveCollectionStep.NameCollection)
        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectTabs)) }

        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk()))
        controller.backPressed(SaveCollectionStep.NameCollection)
        verify { dismiss() }
    }

    @Test
    fun `GIVEN select collection WHEN backPressed is called THEN next step should be dispatched`() {
        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk(), mockk()))
        controller.backPressed(SaveCollectionStep.SelectCollection)
        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectTabs)) }

        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk()))
        controller.backPressed(SaveCollectionStep.SelectCollection)
        verify { dismiss() }
    }

    @Test
    fun `GIVEN last step WHEN backPressed is called THEN dismiss should be called`() {
        controller.backPressed(SaveCollectionStep.SelectTabs)
        verify { dismiss() }

        controller.backPressed(SaveCollectionStep.RenameCollection)
        verify { dismiss() }
    }

    @Test
    fun `GIVEN collection WHEN renameCollection is called THEN collection should be renamed`() = testCoroutineScope.runBlockingTest {
        val collection = mockk<TabCollection>()

        controller.renameCollection(collection, "name")
        advanceUntilIdle()

        verifyAll {
            dismiss()
            tabCollectionStorage.renameCollection(collection, "name")
            metrics.track(Event.CollectionRenamed)
        }
    }

    @Test
    fun `WHEN select all is called THEN add all should be dispatched`() {
        controller.selectAllTabs()
        verify { store.dispatch(CollectionCreationAction.AddAllTabs) }

        controller.deselectAllTabs()
        verify { store.dispatch(CollectionCreationAction.RemoveAllTabs) }

        controller.close()
        verify { dismiss() }
    }

    @Test
    fun `WHEN select tab is called THEN add tab should be dispatched`() {
        val tab = mockk<Tab>()

        controller.addTabToSelection(tab)
        verify { store.dispatch(CollectionCreationAction.TabAdded(tab)) }

        controller.removeTabFromSelection(tab)
        verify { store.dispatch(CollectionCreationAction.TabRemoved(tab)) }
    }

    @Test
    fun `WHEN selectCollection is called THEN add tabs should be added to collection`() {
        val session = mockSession(sessionId = "session-1")
        val sessions = listOf(
            session,
            mockSession(sessionId = "session-2")
        )
        every { sessionManager.findSessionById("session-1") } returns session
        every { sessionManager.sessions } returns sessions
        val tabs = listOf(
            Tab("session-1", "", "", "", mediaState = MediaState.State.NONE)
        )
        val collection = mockk<TabCollection>()

        controller.selectCollection(collection, tabs)

        verify { dismiss() }
        verify { tabCollectionStorage.addTabsToCollection(collection, listOf(session)) }
        verify { metrics.track(Event.CollectionTabsAdded(2, 1)) }
    }

    @Test
    fun `GIVEN previous step was SelectTabs or RenameCollection WHEN stepBack is called THEN null should be returned`() {
        assertNull(controller.stepBack(SaveCollectionStep.SelectTabs))
        assertNull(controller.stepBack(SaveCollectionStep.RenameCollection))
    }

    @Test
    fun `GIVEN previous step was SelectCollection AND more than one tab is open WHEN stepBack is called THEN SelectTabs should be returned`() {
        state = state.copy(tabs = listOf(mockk(), mockk()))

        assertEquals(SaveCollectionStep.SelectTabs, controller.stepBack(SaveCollectionStep.SelectCollection))
    }

    @Test
    fun `GIVEN previous step was SelectCollection AND one or fewer tabs are open WHEN stepbback is called THEN null should be returned`() {
        state = state.copy(tabs = listOf(mockk()))
        assertNull(controller.stepBack(SaveCollectionStep.SelectCollection))

        state = state.copy(tabs = emptyList())
        assertNull(controller.stepBack(SaveCollectionStep.SelectCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is empty AND more than one tab is open WHEN stepBack is called THEN SelectTabs should be returned`() {
        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk(), mockk()))

        assertEquals(SaveCollectionStep.SelectTabs, controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is empty AND one or fewer tabs are open WHEN stepBack is called THEN null should be returned`() {
        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk()))
        assertNull(controller.stepBack(SaveCollectionStep.NameCollection))

        state = state.copy(tabCollections = emptyList(), tabs = emptyList())
        assertNull(controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is not empty WHEN stepBack is called THEN SelectCollection should be returned`() {
        state = state.copy(tabCollections = listOf(mockk()))
        assertEquals(SaveCollectionStep.SelectCollection, controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `GIVEN list of collections WHEN default collection number is required THEN return next default number`() {
        val collections = mutableListOf<TabCollection>(
            mockk {
                every { title } returns "Collection 1"
            },
            mockk {
                every { title } returns "Collection 2"
            },
            mockk {
                every { title } returns "Collection 3"
            }
        )
        state = state.copy(tabCollections = collections)
        assertEquals(4, controller.getDefaultCollectionNumber())

        collections.add(mockk {
            every { title } returns "Collection 5"
        })
        state = state.copy(tabCollections = collections)
        assertEquals(6, controller.getDefaultCollectionNumber())

        collections.add(mockk {
            every { title } returns "Random name"
        })
        state = state.copy(tabCollections = collections)
        assertEquals(6, controller.getDefaultCollectionNumber())

        collections.add(mockk {
            every { title } returns "Collection 10 10"
        })
        state = state.copy(tabCollections = collections)
        assertEquals(6, controller.getDefaultCollectionNumber())
    }

    @Test
    fun `WHEN adding a new collection THEN dispatch NameCollection step changed`() {
        controller.addNewCollection()

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.NameCollection, 1)) }
    }

    @Test
    fun `GIVEN empty list of collections WHEN saving tabs to collection THEN dispatch NameCollection step changed`() {
        controller.saveTabsToCollection(ArrayList())

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.NameCollection, 1)) }
    }

    @Test
    fun `GIVEN list of collections WHEN saving tabs to collection THEN dispatch NameCollection step changed`() {
        state = state.copy(tabCollections = listOf(
            mockk {
                every { title } returns "Collection 1"
            },
            mockk {
                every { title } returns "Random Collection"
            }
        ))

        controller.saveTabsToCollection(ArrayList())

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectCollection, 2)) }
    }

    @Test
    fun `normalSessionSize only counts non-private non-custom sessions`() {
        val normal1 = mockSession()
        val normal2 = mockSession()
        val normal3 = mockSession()

        val private1 = mockSession(isPrivate = true)
        val private2 = mockSession(isPrivate = true)

        val custom1 = mockSession(isCustom = true)
        val custom2 = mockSession(isCustom = true)
        val custom3 = mockSession(isCustom = true)

        val privateCustom = mockSession(isPrivate = true, isCustom = true)

        every { sessionManager.sessions } returns listOf(normal1, private1, private2, custom1,
            normal2, normal3, custom2, custom3, privateCustom)

        assertEquals(3, controller.normalSessionSize(sessionManager))
    }

    private fun mockSession(
        sessionId: String? = null,
        isPrivate: Boolean = false,
        isCustom: Boolean = false
    ) = mockk<Session> {
        sessionId?.let { every { id } returns it }
        every { private } returns isPrivate
        every { isCustomTabSession() } returns isCustom
    }
}
