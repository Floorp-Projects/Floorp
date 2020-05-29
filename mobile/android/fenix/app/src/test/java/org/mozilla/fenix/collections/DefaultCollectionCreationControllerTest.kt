package org.mozilla.fenix.collections

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.Analytics
import org.mozilla.fenix.components.TabCollectionStorage

@ExperimentalCoroutinesApi
class DefaultCollectionCreationControllerTest {

    private val testCoroutineScope = TestCoroutineScope()

    private lateinit var controller: DefaultCollectionCreationController

    @MockK(relaxed = true) private lateinit var store: CollectionCreationStore
    @MockK(relaxed = true) private lateinit var dismiss: () -> Unit
    @MockK(relaxed = true) private lateinit var analytics: Analytics
    @MockK private lateinit var tabCollectionStorage: TabCollectionStorage
    @MockK private lateinit var tabsUseCases: TabsUseCases
    @MockK private lateinit var sessionManager: SessionManager
    @MockK private lateinit var state: CollectionCreationState

    @Before
    fun before() {
        MockKAnnotations.init(this)

        every { store.state } returns state
        every { state.tabCollections } returns emptyList()
        every { state.tabs } returns emptyList()

        controller = DefaultCollectionCreationController(
            store, dismiss, analytics,
            tabCollectionStorage, sessionManager, testCoroutineScope
        )
    }

    @Test
    fun `GIVEN previous step was SelectTabs or RenameCollection WHEN stepBack is called THEN null should be returned`() {
        assertNull(controller.stepBack(SaveCollectionStep.SelectTabs))
        assertNull(controller.stepBack(SaveCollectionStep.RenameCollection))
    }

    @Test
    fun `GIVEN previous step was SelectCollection AND more than one tab is open WHEN stepBack is called THEN SelectTabs should be returned`() {
        every { state.tabs } returns listOf(mockk(), mockk())

        assertEquals(SaveCollectionStep.SelectTabs, controller.stepBack(SaveCollectionStep.SelectCollection))
    }

    @Test
    fun `GIVEN previous step was SelectCollection AND one or fewer tabs are open WHEN stepbback is called THEN null should be returned`() {
        every { state.tabs } returns listOf(mockk())
        assertNull(controller.stepBack(SaveCollectionStep.SelectCollection))

        every { state.tabs } returns emptyList()
        assertNull(controller.stepBack(SaveCollectionStep.SelectCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is empty AND more than one tab is open WHEN stepBack is called THEN SelectTabs should be returned`() {
        every { state.tabCollections } returns emptyList()
        every { state.tabs } returns listOf(mockk(), mockk())

        assertEquals(SaveCollectionStep.SelectTabs, controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is empty AND one or fewer tabs are open WHEN stepBack is called THEN null should be returned`() {
        every { state.tabCollections } returns emptyList()
        every { state.tabs } returns listOf(mockk())
        assertNull(controller.stepBack(SaveCollectionStep.NameCollection))

        every { state.tabCollections } returns emptyList()
        every { state.tabs } returns emptyList()
        assertNull(controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is not empty WHEN stepBack is called THEN SelectCollection should be returned`() {
        every { state.tabCollections } returns listOf(mockk())

        assertEquals(SaveCollectionStep.SelectCollection, controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `GIVEN list of collections WHEN default collection number is required THEN return next default number`() {
        val collections: MutableList<TabCollection> = ArrayList()
        collections.add(mockk {
            every { title } returns "Collection 1"
        })
        collections.add(mockk {
            every { title } returns "Collection 2"
        })
        collections.add(mockk {
            every { title } returns "Collection 3"
        })
        every { state.tabCollections } returns collections

        assertEquals(4, controller.getDefaultCollectionNumber())

        collections.add(mockk {
            every { title } returns "Collection 5"
        })
        assertEquals(6, controller.getDefaultCollectionNumber())

        collections.add(mockk {
            every { title } returns "Random name"
        })
        assertEquals(6, controller.getDefaultCollectionNumber())

        collections.add(mockk {
            every { title } returns "Collection 10 10"
        })
        assertEquals(6, controller.getDefaultCollectionNumber())
    }

    @Test
    fun `WHEN adding a new collection THEN dispatch NameCollection step changed`() {
        val collections: List<TabCollection> = ArrayList()
        every { state.tabCollections } returns collections

        controller.addNewCollection()

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.NameCollection, 1)) }
    }

    @Test
    fun `GIVEN empty list of collections WHEN saving tabs to collection THEN dispatch NameCollection step changed`() {
        val collections: List<TabCollection> = ArrayList()
        every { state.tabCollections } returns collections

        controller.saveTabsToCollection(ArrayList())

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.NameCollection, 1)) }
    }

    @Test
    fun `GIVEN list of collections WHEN saving tabs to collection THEN dispatch NameCollection step changed`() {
        val collections: MutableList<TabCollection> = ArrayList()
        collections.add(mockk {
            every { title } returns "Collection 1"
        })
        collections.add(mockk {
            every { title } returns "Random Collection"
        })
        every { state.tabCollections } returns collections

        controller.saveTabsToCollection(ArrayList())

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectCollection, 2)) }
    }

    @Test
    fun `normalSessionSize only counts non-private non-custom sessions`() {
        fun session(isPrivate: Boolean, isCustom: Boolean) = mockk<Session>().apply {
            every { private } returns isPrivate
            every { isCustomTabSession() } returns isCustom
        }

        val normal1 = session(isPrivate = false, isCustom = false)
        val normal2 = session(isPrivate = false, isCustom = false)
        val normal3 = session(isPrivate = false, isCustom = false)

        val private1 = session(isPrivate = true, isCustom = false)
        val private2 = session(isPrivate = true, isCustom = false)

        val custom1 = session(isPrivate = false, isCustom = true)
        val custom2 = session(isPrivate = false, isCustom = true)
        val custom3 = session(isPrivate = false, isCustom = true)

        val privateCustom = session(isPrivate = true, isCustom = true)

        every { sessionManager.sessions } returns listOf(normal1, private1, private2, custom1,
            normal2, normal3, custom2, custom3, privateCustom)

        assertEquals(3, controller.normalSessionSize(sessionManager))
    }
}
