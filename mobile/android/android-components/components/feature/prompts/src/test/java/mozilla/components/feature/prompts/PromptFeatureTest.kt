/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.app.Activity
import android.app.Activity.RESULT_CANCELED
import android.app.Activity.RESULT_OK
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.view.View
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Level.NONE
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Method.HOST
import mozilla.components.concept.engine.prompt.PromptRequest.Color
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.TextPrompt
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.feature.prompts.address.AddressDelegate
import mozilla.components.feature.prompts.address.AddressPicker
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.creditcard.CreditCardDelegate
import mozilla.components.feature.prompts.creditcard.CreditCardPicker
import mozilla.components.feature.prompts.creditcard.CreditCardSaveDialogFragment
import mozilla.components.feature.prompts.dialog.ChoiceDialogFragment
import mozilla.components.feature.prompts.dialog.ConfirmDialogFragment
import mozilla.components.feature.prompts.dialog.MultiButtonDialogFragment
import mozilla.components.feature.prompts.dialog.PromptDialogFragment
import mozilla.components.feature.prompts.dialog.SaveLoginDialogFragment
import mozilla.components.feature.prompts.file.FilePicker.Companion.FILE_PICKER_ACTIVITY_REQUEST_CODE
import mozilla.components.feature.prompts.login.LoginDelegate
import mozilla.components.feature.prompts.login.LoginPicker
import mozilla.components.feature.prompts.share.ShareDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import java.lang.ref.WeakReference
import java.security.InvalidParameterException
import java.util.Date

@RunWith(AndroidJUnit4::class)
class PromptFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var store: BrowserStore
    private lateinit var fragmentManager: FragmentManager
    private lateinit var loginPicker: LoginPicker
    private lateinit var creditCardPicker: CreditCardPicker
    private lateinit var addressPicker: AddressPicker

    private val tabId = "test-tab"
    private fun tab(): TabSessionState? {
        return store.state.tabs.find { it.id == tabId }
    }

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = tabId)
                ),
                customTabs = listOf(
                    createCustomTab("https://www.mozilla.org", id = "custom-tab")
                ),
                selectedTabId = tabId
            )
        )
        loginPicker = mock()
        creditCardPicker = mock()
        addressPicker = mock()
        fragmentManager = mockFragmentManager()
    }

    @Test
    fun `PromptFeature acts on the selected session by default`() {
        val feature = spy(
            PromptFeature(
                fragment = mock(),
                store = store,
                fragmentManager = fragmentManager
            ) { }
        )
        feature.start()

        val promptRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        verify(feature).onPromptRequested(store.state.tabs.first())
    }

    @Test
    fun `PromptFeature acts on a given custom tab session`() {
        val feature = spy(
            PromptFeature(
                fragment = mock(),
                store = store,
                customTabId = "custom-tab",
                fragmentManager = fragmentManager
            ) { }
        )
        feature.start()

        val promptRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction("custom-tab", promptRequest))
            .joinBlocking()
        verify(feature).onPromptRequested(store.state.customTabs.first())
    }

    @Test
    fun `PromptFeature acts on the selected session if there is no custom tab ID`() {
        val feature = spy(
            PromptFeature(
                fragment = mock(),
                store = store,
                customTabId = tabId,
                fragmentManager = fragmentManager
            ) { }
        )

        val promptRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.start()
        verify(feature).onPromptRequested(store.state.tabs.first())
    }

    @Test
    fun `New promptRequests for selected session will cause fragment transaction`() {
        val feature =
            PromptFeature(fragment = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        val singleChoiceRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest))
            .joinBlocking()
        verify(fragmentManager).beginTransaction()
    }

    @Test
    fun `New promptRequests for selected session will not cause fragment transaction if feature is stopped`() {
        val feature =
            PromptFeature(fragment = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()
        feature.stop()

        val singleChoiceRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest))
            .joinBlocking()
        verify(fragmentManager, never()).beginTransaction()
    }

    @Test
    fun `Feature will re-attach to already existing fragment`() {
        val fragment: ChoiceDialogFragment = mock()
        doReturn(tabId).`when`(fragment).sessionId
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(FRAGMENT_TAG)

        val singleChoiceRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest))
            .joinBlocking()

        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()
        verify(fragment).feature = feature
    }

    @Test
    fun `Existing fragment will be removed if session has no prompt request`() {
        val fragment: ChoiceDialogFragment = mock()
        doReturn(tabId).`when`(fragment).sessionId
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(FRAGMENT_TAG)

        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(any())

        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        verify(fragment, never()).feature = feature
        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `Existing fragment will be removed if session does not exist anymore`() {
        val fragment: ChoiceDialogFragment = mock()
        doReturn("invalid-tab").`when`(fragment).sessionId
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(FRAGMENT_TAG)

        val singleChoiceRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction("invalid-tab", singleChoiceRequest))
            .joinBlocking()

        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(any())

        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        verify(fragment, never()).feature = feature
        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `Calling onStop will attempt to dismiss the select prompts`() {
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager
            ) { }
        )

        feature.stop()

        verify(feature).dismissSelectPrompts()
    }

    @Test
    fun `GIVEN loginPickerView is visible WHEN dismissSelectPrompts THEN dismissCurrentLoginSelect called and true returned`() {
        // given
        val loginPickerView: SelectablePromptView<Login> = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                loginDelegate = object : LoginDelegate {
                    override val loginPickerView = loginPickerView
                    override val onManageLogins = {}
                }
            ) { }
        )
        val selectLoginPrompt = mock<PromptRequest.SelectLoginPrompt>()
        whenever(loginPickerView.asView()).thenReturn(mock())
        whenever(loginPickerView.asView().visibility).thenReturn(View.VISIBLE)
        feature.loginPicker = loginPicker
        feature.activePromptRequest = selectLoginPrompt

        // when
        val result = feature.dismissSelectPrompts()

        // then
        verify(feature.loginPicker!!).dismissCurrentLoginSelect(selectLoginPrompt)
        assertEquals(true, result)
    }

    @Test
    fun `GIVEN saveLoginPrompt is visible WHEN prompt is removed from state THEN dismiss saveLoginPrompt`() {
        // given
        val loginUsername = "username"
        val loginPassword = "password"
        val entry: LoginEntry = mock()
        `when`(entry.username).thenReturn(loginUsername)
        `when`(entry.password).thenReturn(loginPassword)
        val promptRequest = PromptRequest.SaveLoginPrompt(2, listOf(entry), { }, { })
        val saveLoginPrompt: SaveLoginDialogFragment = mock()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest))
            .joinBlocking()
        store.waitUntilIdle()

        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                isSaveLoginEnabled = { true },
                loginValidationDelegate = mock()
            ) { }
        )

        feature.start()
        feature.activePrompt = WeakReference(saveLoginPrompt)
        feature.activePromptRequest = promptRequest

        // when
        store.dispatch(ContentAction.ConsumePromptRequestAction(tabId, promptRequest))
            .joinBlocking()

        // then
        verify(saveLoginPrompt).dismissAllowingStateLoss()
    }

    @Test
    fun `GIVEN isSaveLoginEnabled is false WHEN saveLoginPrompt request is handled THEN dismiss saveLoginPrompt`() {
        val promptRequest = spy(
            PromptRequest.SaveLoginPrompt(
                hint = 2,
                logins = emptyList(),
                onConfirm = {},
                onDismiss = {}
            )
        )
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                isSaveLoginEnabled = { false },
            ) {}
        )
        val session = tab()!!

        feature.handleDialogsRequest(promptRequest, session)

        store.waitUntilIdle()

        verify(feature).dismissDialogRequest(promptRequest, session)
    }

    @Test
    fun `GIVEN loginValidationDelegate is null WHEN saveLoginPrompt request is handled THEN dismiss saveLoginPrompt`() {
        val promptRequest = spy(
            PromptRequest.SaveLoginPrompt(
                hint = 2,
                logins = emptyList(),
                onConfirm = {},
                onDismiss = {}
            )
        )
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                isSaveLoginEnabled = { true },
            ) {}
        )
        val session = tab()!!

        feature.handleDialogsRequest(promptRequest, session)

        store.waitUntilIdle()

        verify(feature).dismissDialogRequest(promptRequest, session)
    }

    @Test
    fun `WHEN dismissDialogRequest is called THEN dismiss and consume the prompt request`() {
        val tab = createTab("https://www.mozilla.org", id = tabId)
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    customTabs = listOf(
                        createCustomTab("https://www.mozilla.org", id = "custom-tab")
                    ),
                    selectedTabId = tabId
                )
            )
        )
        val feature = PromptFeature(
            activity = mock(),
            store = store,
            fragmentManager = fragmentManager,
        ) {}

        var onDismissWasCalled = false
        val promptRequest = PromptRequest.SaveLoginPrompt(
            hint = 2,
            logins = emptyList(),
            onConfirm = {},
            onDismiss = { onDismissWasCalled = true }
        )

        feature.dismissDialogRequest(promptRequest, tab)

        store.waitUntilIdle()

        verify(store).dispatch(ContentAction.ConsumePromptRequestAction(tab.id, promptRequest))
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `GIVEN loginPickerView is not visible WHEN dismissSelectPrompts THEN dismissCurrentLoginSelect called and false returned`() {
        // given
        val loginPickerView: SelectablePromptView<Login> = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                loginDelegate = object : LoginDelegate {
                    override val loginPickerView = loginPickerView
                    override val onManageLogins = {}
                }
            ) { }
        )
        val selectLoginPrompt = mock<PromptRequest.SelectLoginPrompt>()
        whenever(loginPickerView.asView()).thenReturn(mock())
        whenever(loginPickerView.asView().visibility).thenReturn(View.GONE)
        feature.loginPicker = loginPicker
        feature.activePromptRequest = selectLoginPrompt

        // when
        val result = feature.dismissSelectPrompts()

        // then
        assertEquals(false, result)
    }

    @Test
    fun `GIVEN PromptFeature WHEN onBackPressed THEN dismissSelectPrompts is called`() {
        // given
        val loginPickerView: SelectablePromptView<Login> = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                loginDelegate = object : LoginDelegate {
                    override val loginPickerView = loginPickerView
                    override val onManageLogins = {}
                }
            ) { }
        )
        val selectLoginPrompt = mock<PromptRequest.SelectLoginPrompt>()
        whenever(loginPickerView.asView()).thenReturn(mock())
        whenever(loginPickerView.asView().visibility).thenReturn(View.VISIBLE)
        feature.loginPicker = loginPicker
        feature.activePromptRequest = selectLoginPrompt

        // when
        val result = feature.onBackPressed()

        // then
        verify(feature).dismissSelectPrompts()
        assertEquals(true, result)
    }

    @Test
    fun `Calling dismissSelectPrompts should dismiss the login picker if the login prompt is active`() {
        val loginPickerView: SelectablePromptView<Login> = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                loginDelegate = object : LoginDelegate {
                    override val loginPickerView = loginPickerView
                    override val onManageLogins = {}
                }
            ) { }
        )
        val selectLoginPrompt = mock<PromptRequest.SelectLoginPrompt>()
        whenever(loginPickerView.asView()).thenReturn(mock())
        whenever(loginPickerView.asView().visibility).thenReturn(View.VISIBLE)

        feature.loginPicker = loginPicker
        feature.activePromptRequest = mock()
        feature.dismissSelectPrompts()
        verify(feature.loginPicker!!, never()).dismissCurrentLoginSelect(any())

        feature.loginPicker = loginPicker
        feature.activePromptRequest = selectLoginPrompt
        feature.dismissSelectPrompts()
        verify(feature.loginPicker!!).dismissCurrentLoginSelect(selectLoginPrompt)
    }

    @Test
    fun `GIVEN creditCardPickerView is visible WHEN dismissSelectPrompts is called THEN dismissSelectCreditCardRequest returns true`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                }
            ) { }
        )
        val selectCreditCardRequest = mock<PromptRequest.SelectCreditCard>()
        feature.creditCardPicker = creditCardPicker
        feature.activePromptRequest = selectCreditCardRequest

        whenever(creditCardPickerView.asView()).thenReturn(mock())
        whenever(creditCardPickerView.asView().visibility).thenReturn(View.VISIBLE)

        val result = feature.dismissSelectPrompts()

        verify(feature.creditCardPicker!!).dismissSelectCreditCardRequest(selectCreditCardRequest)
        assertEquals(true, result)
    }

    @Test
    fun `GIVEN creditCardPickerView is not visible WHEN dismissSelectPrompts is called THEN dismissSelectPrompt returns false`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                }
            ) { }
        )
        val selectCreditCardRequest = mock<PromptRequest.SelectCreditCard>()
        feature.creditCardPicker = creditCardPicker
        feature.activePromptRequest = selectCreditCardRequest

        whenever(creditCardPickerView.asView()).thenReturn(mock())
        whenever(creditCardPickerView.asView().visibility).thenReturn(View.GONE)

        val result = feature.dismissSelectPrompts()

        assertEquals(false, result)
    }

    @Test
    fun `GIVEN an active select credit card request WHEN onBackPressed is called THEN dismissSelectPrompts is called`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                }
            ) { }
        )
        val selectCreditCardRequest = mock<PromptRequest.SelectCreditCard>()
        feature.creditCardPicker = creditCardPicker
        feature.activePromptRequest = selectCreditCardRequest

        whenever(creditCardPickerView.asView()).thenReturn(mock())
        whenever(creditCardPickerView.asView().visibility).thenReturn(View.VISIBLE)

        val result = feature.onBackPressed()

        verify(feature).dismissSelectPrompts()
        assertEquals(true, result)
    }

    @Test
    fun `WHEN dismissSelectPrompts is called THEN the active credit card picker should be dismissed`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                }
            ) { }
        )
        feature.creditCardPicker = creditCardPicker
        feature.activePromptRequest = mock()

        whenever(creditCardPickerView.asView()).thenReturn(mock())
        whenever(creditCardPickerView.asView().visibility).thenReturn(View.VISIBLE)

        feature.dismissSelectPrompts()
        verify(feature.creditCardPicker!!, never()).dismissSelectCreditCardRequest(any())

        val selectCreditCardRequest = mock<PromptRequest.SelectCreditCard>()
        feature.activePromptRequest = selectCreditCardRequest

        feature.dismissSelectPrompts()

        verify(feature.creditCardPicker!!).dismissSelectCreditCardRequest(selectCreditCardRequest)
    }

    @Test
    fun `WHEN dismissSelectPrompts is called THEN the active addressPicker dismiss should be called`() {
        val addressPickerView: SelectablePromptView<Address> = mock()
        val addressDelegate: AddressDelegate = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                addressDelegate = addressDelegate
            ) { }
        )
        feature.addressPicker = addressPicker
        feature.activePromptRequest = mock()

        whenever(addressDelegate.addressPickerView).thenReturn(addressPickerView)
        whenever(addressPickerView.asView()).thenReturn(mock())
        whenever(addressPickerView.asView().visibility).thenReturn(View.VISIBLE)

        feature.dismissSelectPrompts()
        verify(feature.addressPicker!!, never()).dismissSelectAddressRequest(any())

        val selectAddressPromptRequest = mock<PromptRequest.SelectAddress>()
        feature.activePromptRequest = selectAddressPromptRequest

        feature.dismissSelectPrompts()

        verify(feature.addressPicker!!).dismissSelectAddressRequest(selectAddressPromptRequest)

        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `GIVEN addressPickerView is not visible WHEN dismissSelectPrompts is called THEN dismissSelectPrompts returns false`() {
        val addressPickerView: SelectablePromptView<Address> = mock()
        val addressDelegate: AddressDelegate = mock()
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                fragmentManager = fragmentManager,
                addressDelegate = addressDelegate
            ) { }
        )
        val selectAddressRequest = mock<PromptRequest.SelectAddress>()
        feature.addressPicker = addressPicker
        feature.activePromptRequest = selectAddressRequest

        whenever(addressDelegate.addressPickerView).thenReturn(addressPickerView)
        whenever(addressPickerView.asView()).thenReturn(mock())
        whenever(addressPickerView.asView().visibility).thenReturn(View.GONE)

        val result = feature.dismissSelectPrompts()

        assertEquals(false, result)
    }

    @Test
    fun `Calling onCancel will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }

        val singleChoiceRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest))
            .joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(singleChoiceRequest, tab()!!.content.promptRequests[0])
        feature.onCancel(tabId, singleChoiceRequest.uid)

        store.waitUntilIdle()
        assertTrue(tab()?.content?.promptRequests?.isEmpty() ?: false)
    }

    @Test
    fun `Selecting an item in a single choice dialog will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        val singleChoiceRequest = SingleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, singleChoiceRequest))
            .joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(singleChoiceRequest, tab()!!.content.promptRequests[0])
        feature.onConfirm(tabId, singleChoiceRequest.uid, mock<Choice>())

        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `Selecting an item in a menu choice dialog will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        val menuChoiceRequest = MenuChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, menuChoiceRequest))
            .joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(menuChoiceRequest, tab()!!.content.promptRequests[0])
        feature.onConfirm(tabId, menuChoiceRequest.uid, mock<Choice>())

        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `Selecting items on multiple choice dialog will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.start()

        val multipleChoiceRequest = MultipleChoice(arrayOf(), {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, multipleChoiceRequest))
            .joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(multipleChoiceRequest, tab()!!.content.promptRequests[0])
        feature.onConfirm(tabId, multipleChoiceRequest.uid, arrayOf<Choice>())

        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `onNoMoreDialogsChecked will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }

        var onShowNoMoreAlertsWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Alert(
            "title",
            "message",
            false,
            { onShowNoMoreAlertsWasCalled = true },
            { onDismissWasCalled = true }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, promptRequest.uid, false)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onShowNoMoreAlertsWasCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onCancel with an alert request will consume promptRequest and call onDismiss`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled = false
        val promptRequest = Alert("title", "message", false, {}, { onDismissWasCalled = true })

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(onDismissWasCalled)
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `onConfirmTextPrompt will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = TextPrompt(
            "title",
            "message",
            "input",
            false,
            { _, _ -> onConfirmWasCalled = true },
            { onDismissWasCalled = true }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, promptRequest.uid, false to "")
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onConfirmWasCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onCancel with an TextPrompt request will consume promptRequest and call onDismiss`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled = false

        val promptRequest = TextPrompt(
            "title",
            "message",
            "value",
            false,
            { _, _ -> },
            { onDismissWasCalled = true }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `selecting a time will consume promptRequest`() {
        val timeSelectionTypes = listOf(
            PromptRequest.TimeSelection.Type.DATE,
            PromptRequest.TimeSelection.Type.DATE_AND_TIME,
            PromptRequest.TimeSelection.Type.TIME,
            PromptRequest.TimeSelection.Type.MONTH
        )

        timeSelectionTypes.forEach { type ->
            val feature = PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager
            ) { }
            var onClearWasCalled = false
            var selectedDate: Date? = null
            val promptRequest = PromptRequest.TimeSelection(
                "title",
                Date(0),
                null,
                null,
                type,
                { date -> selectedDate = date },
                { onClearWasCalled = true },
                { }
            )

            feature.start()
            store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest))
                .joinBlocking()

            val now = Date()
            feature.onConfirm(tabId, promptRequest.uid, now)
            store.waitUntilIdle()
            assertTrue(tab()!!.content.promptRequests.isEmpty())

            assertEquals(now, selectedDate)
            store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest))
                .joinBlocking()

            feature.onClear(tabId, promptRequest.uid)
            assertTrue(onClearWasCalled)
            feature.stop()
        }
    }

    @Test(expected = InvalidParameterException::class)
    fun `calling handleDialogsRequest with invalid type will throw an exception`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        feature.handleDialogsRequest(mock<PromptRequest.File>(), mock())
    }

    @Test
    fun `onActivityResult with RESULT_OK and isMultipleFilesSelection false will consume PromptRequest`() {
        var onSingleFileSelectionWasCalled = false

        val onSingleFileSelection: (Context, Uri) -> Unit = { _, _ ->
            onSingleFileSelectionWasCalled = true
        }

        val filePickerRequest =
            PromptRequest.File(emptyArray(), false, onSingleFileSelection, { _, _ -> }) { }

        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        val intent = Intent()

        intent.data = mock()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, filePickerRequest))
            .joinBlocking()

        feature.onActivityResult(FILE_PICKER_ACTIVITY_REQUEST_CODE, intent, RESULT_OK)
        store.waitUntilIdle()
        assertTrue(onSingleFileSelectionWasCalled)
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `onActivityResult with RESULT_OK and isMultipleFilesSelection true will consume PromptRequest of the actual session`() {
        var onMultipleFileSelectionWasCalled = false

        val onMultipleFileSelection: (Context, Array<Uri>) -> Unit = { _, _ ->
            onMultipleFileSelectionWasCalled = true
        }

        val filePickerRequest =
            PromptRequest.File(emptyArray(), true, { _, _ -> }, onMultipleFileSelection) {}

        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        val intent = Intent()

        intent.clipData = mock()
        val item = mock<ClipData.Item>()

        doReturn(mock<Uri>()).`when`(item).uri

        intent.clipData?.apply {
            doReturn(1).`when`(this).itemCount
            doReturn(item).`when`(this).getItemAt(0)
        }

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, filePickerRequest))
            .joinBlocking()

        feature.onActivityResult(FILE_PICKER_ACTIVITY_REQUEST_CODE, intent, RESULT_OK)
        store.waitUntilIdle()
        assertTrue(onMultipleFileSelectionWasCalled)
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `onActivityResult with RESULT_CANCELED will consume PromptRequest call onDismiss`() {
        var onDismissWasCalled = false

        val filePickerRequest =
            PromptRequest.File(emptyArray(), true, { _, _ -> }, { _, _ -> }) {
                onDismissWasCalled = true
            }

        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        val intent = Intent()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, filePickerRequest))
            .joinBlocking()

        feature.onActivityResult(FILE_PICKER_ACTIVITY_REQUEST_CODE, intent, RESULT_CANCELED)
        store.waitUntilIdle()
        assertTrue(onDismissWasCalled)
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `WHEN onActivityResult is called with PIN_REQUEST and RESULT_OK THEN onAuthSuccess) is called`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature =
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                },
                isCreditCardAutofillEnabled = { true }
            ) { }
        feature.creditCardPicker = creditCardPicker
        val intent = Intent()

        feature.onActivityResult(PromptFeature.PIN_REQUEST, intent, RESULT_OK)

        verify(creditCardPicker).onAuthSuccess()
    }

    @Test
    fun `WHEN onActivityResult is called with PIN_REQUEST and RESULT_CANCELED THEN onAuthFailure is called`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature =
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                },
                isCreditCardAutofillEnabled = { true }
            ) { }
        feature.creditCardPicker = creditCardPicker
        val intent = Intent()

        feature.onActivityResult(PromptFeature.PIN_REQUEST, intent, RESULT_CANCELED)

        verify(creditCardPicker).onAuthFailure()
    }

    @Test
    fun `GIVEN user successfully authenticates by biometric prompt WHEN onBiometricResult is called THEN onAuthSuccess is called`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature =
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                },
                isCreditCardAutofillEnabled = { true }
            ) { }
        feature.creditCardPicker = creditCardPicker

        feature.onBiometricResult(isAuthenticated = true)

        verify(creditCardPicker).onAuthSuccess()
    }

    @Test
    fun `GIVEN user fails to authenticate by biometric prompt WHEN onBiometricResult is called THEN onAuthFailure) is called`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature =
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                },
                isCreditCardAutofillEnabled = { true }
            ) { }
        feature.creditCardPicker = creditCardPicker

        feature.onBiometricResult(isAuthenticated = false)

        verify(creditCardPicker).onAuthFailure()
    }

    @Test
    fun `Selecting a login confirms the request`() {
        var onDismissWasCalled = false
        var confirmedLogin: Login? = null

        val login =
            Login(guid = "A", origin = "https://www.mozilla.org", username = "username", password = "password")
        val login2 =
            Login(guid = "B", origin = "https://www.mozilla.org", username = "username2", password = "password")

        val loginPickerRequest = PromptRequest.SelectLoginPrompt(
            listOf(login, login2),
            onConfirm = { confirmedLogin = it },
            onDismiss = { onDismissWasCalled = true }
        )

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, loginPickerRequest))
            .joinBlocking()

        loginPickerRequest.onConfirm(login)

        store.waitUntilIdle()

        assertEquals(confirmedLogin, login)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, loginPickerRequest))
            .joinBlocking()
        loginPickerRequest.onDismiss()

        store.waitUntilIdle()

        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `WHEN a credit card is selected THEN confirm the prompt request with the selected credit card`() {
        val creditCard = CreditCardEntry(
            guid = "id",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = "amex"
        )
        var onDismissCalled = false
        var onConfirmCalled = false
        var confirmedCreditCard: CreditCardEntry? = null

        val selectCreditCardRequest = PromptRequest.SelectCreditCard(
            creditCards = listOf(creditCard),
            onDismiss = {
                onDismissCalled = true
            },
            onConfirm = {
                confirmedCreditCard = it
                onConfirmCalled = true
            }
        )

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, selectCreditCardRequest))
            .joinBlocking()

        selectCreditCardRequest.onConfirm(creditCard)

        store.waitUntilIdle()

        assertEquals(creditCard, confirmedCreditCard)
        assertTrue(onConfirmCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, selectCreditCardRequest))
            .joinBlocking()
        selectCreditCardRequest.onDismiss()

        store.waitUntilIdle()

        assertTrue(onDismissCalled)
    }

    @Test
    fun `Calling onConfirmAuthentication will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Authentication(
            uri = "https://www.mozilla.org",
            title = "title",
            message = "message",
            userName = "username",
            password = "password",
            method = HOST,
            level = NONE,
            onlyShowPassword = false,
            previousFailed = false,
            isCrossOrigin = false,
            onConfirm = { _, _ -> onConfirmWasCalled = true },
            onDismiss = { onDismissWasCalled = true }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onConfirm(tabId, promptRequest.uid, "" to "")
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onConfirmWasCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onConfirm on a BeforeUnload request will consume promptRequest`() {
        val fragment: Fragment = mock()
        whenever(fragment.getString(R.string.mozac_feature_prompt_before_unload_dialog_title)).thenReturn(
            ""
        )
        whenever(fragment.getString(R.string.mozac_feature_prompt_before_unload_dialog_body)).thenReturn(
            ""
        )
        whenever(fragment.getString(R.string.mozac_feature_prompts_before_unload_stay)).thenReturn("")
        whenever(fragment.getString(R.string.mozac_feature_prompts_before_unload_leave)).thenReturn(
            ""
        )

        val feature =
            PromptFeature(fragment = fragment, store = store, fragmentManager = fragmentManager) { }

        var onLeaveWasCalled = false

        val promptRequest = PromptRequest.BeforeUnload(
            title = "title",
            onLeave = { onLeaveWasCalled = true },
            onStay = { }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onConfirm(tabId, promptRequest.uid, "" to "")
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onLeaveWasCalled)
    }

    @Test
    fun `Calling onCancel on a authentication request will consume promptRequest and call onDismiss`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled = false

        val promptRequest = Authentication(
            uri = "https://www.mozilla.org",
            title = "title",
            message = "message",
            userName = "username",
            password = "password",
            method = HOST,
            level = NONE,
            onlyShowPassword = false,
            previousFailed = false,
            isCrossOrigin = false,
            onConfirm = { _, _ -> },
            onDismiss = { onDismissWasCalled = true }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onConfirm on a color request will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }

        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptRequest = Color(
            "#e66465",
            {
                onConfirmWasCalled = true
            }
        ) {
            onDismissWasCalled = true
        }

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onConfirm(tabId, promptRequest.uid, "#f6b73c")
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onConfirmWasCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `Calling onConfirm on a popup request will consume promptRequest`() {
        val fragment: Fragment = mock()
        whenever(fragment.getString(R.string.mozac_feature_prompts_popup_dialog_title)).thenReturn("")
        whenever(fragment.getString(R.string.mozac_feature_prompts_allow)).thenReturn("")
        whenever(fragment.getString(R.string.mozac_feature_prompts_deny)).thenReturn("")

        val feature =
            PromptFeature(fragment = fragment, store = store, fragmentManager = fragmentManager) { }
        var onConfirmWasCalled = false

        val promptRequest = PromptRequest.Popup(
            "http://www.popuptest.com/",
            { onConfirmWasCalled = true },
            { }
        ) {}

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onConfirm(tabId, promptRequest.uid, true)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onConfirmWasCalled)
    }

    @Test
    fun `Calling onCancel on a popup request will consume promptRequest`() {
        val fragment: Fragment = mock()
        whenever(fragment.getString(R.string.mozac_feature_prompts_popup_dialog_title)).thenReturn("")
        whenever(fragment.getString(R.string.mozac_feature_prompts_allow)).thenReturn("")
        whenever(fragment.getString(R.string.mozac_feature_prompts_deny)).thenReturn("")

        val feature =
            PromptFeature(fragment = fragment, store = store, fragmentManager = fragmentManager) { }
        var onCancelWasCalled = false

        val promptRequest = PromptRequest.Popup(
            "http://www.popuptest.com/", onAllow = { },
            onDeny = {
                onCancelWasCalled = true
            }
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId, promptRequest.uid, true)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onCancelWasCalled)
    }

    @Test
    fun `Calling onCancel on a BeforeUnload request will consume promptRequest`() {
        val fragment: Fragment = mock()
        whenever(fragment.getString(R.string.mozac_feature_prompt_before_unload_dialog_title)).thenReturn(
            ""
        )
        whenever(fragment.getString(R.string.mozac_feature_prompt_before_unload_dialog_body)).thenReturn(
            ""
        )
        whenever(fragment.getString(R.string.mozac_feature_prompts_before_unload_stay)).thenReturn("")
        whenever(fragment.getString(R.string.mozac_feature_prompts_before_unload_leave)).thenReturn(
            ""
        )

        val feature =
            PromptFeature(fragment = fragment, store = store, fragmentManager = fragmentManager) { }
        var onCancelWasCalled = false

        val promptRequest = PromptRequest.BeforeUnload("http://www.test.com/", { }) {
            onCancelWasCalled = true
        }

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onCancelWasCalled)
    }

    @Test
    fun `Calling onConfirm on a confirm request will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onPositiveButtonWasCalled = false
        var onNegativeButtonWasCalled = false
        var onNeutralButtonWasCalled = false

        val onConfirmPositiveButton: (Boolean) -> Unit = {
            onPositiveButtonWasCalled = true
        }

        val onConfirmNegativeButton: (Boolean) -> Unit = {
            onNegativeButtonWasCalled = true
        }

        val onConfirmNeutralButton: (Boolean) -> Unit = {
            onNeutralButtonWasCalled = true
        }

        val promptRequest = PromptRequest.Confirm(
            "title",
            "message",
            false,
            "positive",
            "negative",
            "neutral",
            onConfirmPositiveButton,
            onConfirmNegativeButton,
            onConfirmNeutralButton
        ) {}

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, promptRequest.uid, true to MultiButtonDialogFragment.ButtonType.POSITIVE)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onPositiveButtonWasCalled)

        feature.promptAbuserDetector.resetJSAlertAbuseState()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, promptRequest.uid, true to MultiButtonDialogFragment.ButtonType.NEGATIVE)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onNegativeButtonWasCalled)

        feature.promptAbuserDetector.resetJSAlertAbuseState()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()
        feature.onConfirm(tabId, promptRequest.uid, true to MultiButtonDialogFragment.ButtonType.NEUTRAL)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onNeutralButtonWasCalled)
    }

    @Test
    fun `Calling onCancel on a confirm request will consume promptRequest`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onCancelWasCalled = false

        val onConfirm: (Boolean) -> Unit = { }

        val onDismiss: () -> Unit = {
            onCancelWasCalled = true
        }

        val promptRequest = PromptRequest.Confirm(
            "title",
            "message",
            false,
            "positive",
            "negative",
            "neutral",
            onConfirm,
            onConfirm,
            onConfirm,
            onDismiss
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        feature.onCancel(tabId, promptRequest.uid)
        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onCancelWasCalled)
    }

    @Test
    fun `When dialogs are being abused prompts are not allowed`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled: Boolean
        val onDismiss = { onDismissWasCalled = true }
        val alertRequest = Alert("", "", false, {}, onDismiss)
        val textRequest = TextPrompt("", "", "", false, { _, _ -> }, onDismiss)
        val confirmRequest =
            PromptRequest.Confirm("", "", false, "+", "-", "", {}, {}, {}, onDismiss)

        val promptRequests = arrayOf<PromptRequest>(alertRequest, textRequest, confirmRequest)

        feature.start()
        feature.promptAbuserDetector.userWantsMoreDialogs(false)

        promptRequests.forEach { request ->
            onDismissWasCalled = false
            store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, request)).joinBlocking()
            verify(fragmentManager, never()).beginTransaction()
            assertTrue(onDismissWasCalled)
        }
    }

    @Test
    fun `When dialogs are being abused but the page is refreshed prompts are allowed`() {
        val feature =
            PromptFeature(activity = mock(), store = store, fragmentManager = fragmentManager) { }
        var onDismissWasCalled = false
        val onDismiss = { onDismissWasCalled = true }
        val alertRequest = Alert("", "", false, {}, onDismiss)

        feature.start()
        feature.promptAbuserDetector.userWantsMoreDialogs(false)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, alertRequest)).joinBlocking()

        verify(fragmentManager, never()).beginTransaction()
        assertTrue(onDismissWasCalled)

        // Simulate reloading page
        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, true)).joinBlocking()
        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, false)).joinBlocking()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, alertRequest)).joinBlocking()

        assertTrue(feature.promptAbuserDetector.shouldShowMoreDialogs)
        verify(fragmentManager).beginTransaction()
    }

    @Test
    fun `User can stop further popups from being displayed on the current page`() {
        val feature = PromptFeature(
            activity = Robolectric.buildActivity(Activity::class.java).setup().get(),
            store = store,
            fragmentManager = fragmentManager
        ) { }

        var onDenyCalled = false
        val onDeny = { onDenyCalled = true }
        val popupPrompt = PromptRequest.Popup("https://firefox.com", onAllow = { }, onDeny = onDeny)

        feature.start()
        assertTrue(feature.promptAbuserDetector.shouldShowMoreDialogs)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, popupPrompt)).joinBlocking()
        verify(fragmentManager, times(1)).beginTransaction()
        feature.onCancel(tabId, popupPrompt.uid, true)
        assertFalse(feature.promptAbuserDetector.shouldShowMoreDialogs)
        assertTrue(onDenyCalled)

        onDenyCalled = false
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, popupPrompt)).joinBlocking()
        verify(fragmentManager, times(1)).beginTransaction()
        assertFalse(feature.promptAbuserDetector.shouldShowMoreDialogs)
        assertTrue(onDenyCalled)
    }

    @Test
    fun `When page is refreshed login dialog is dismissed`() {
        val loginPickerView: SelectablePromptView<Login> = mock()
        val feature =
            PromptFeature(
                activity = mock(), store = store, fragmentManager = fragmentManager,
                loginDelegate = object : LoginDelegate {
                    override val loginPickerView = loginPickerView
                    override val onManageLogins = {}
                }
            ) { }
        feature.loginPicker = loginPicker
        val onLoginDismiss: () -> Unit = {}
        val onLoginConfirm: (Login) -> Unit = {}

        val login = Login(guid = "A", origin = "origin", username = "username", password = "password")
        val selectLoginRequest =
            PromptRequest.SelectLoginPrompt(listOf(login), onLoginConfirm, onLoginDismiss)

        whenever(loginPickerView.asView()).thenReturn(mock())
        whenever(loginPickerView.asView().visibility).thenReturn(View.VISIBLE)

        feature.start()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, selectLoginRequest))
            .joinBlocking()

        verify(loginPicker).handleSelectLoginRequest(selectLoginRequest)

        // Simulate reloading page
        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, true)).joinBlocking()

        verify(loginPicker).dismissCurrentLoginSelect(selectLoginRequest)
    }

    @Test
    fun `WHEN page is refreshed THEN credit card prompt is dismissed`() {
        val creditCardPickerView: SelectablePromptView<CreditCardEntry> = mock()
        val feature =
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                creditCardDelegate = object : CreditCardDelegate {
                    override val creditCardPickerView = creditCardPickerView
                    override val onSelectCreditCard = {}
                    override val onManageCreditCards = {}
                },
                isCreditCardAutofillEnabled = { true }
            ) { }
        feature.creditCardPicker = creditCardPicker
        val onDismiss: () -> Unit = {}
        val onConfirm: (CreditCardEntry) -> Unit = {}
        val creditCard = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = ""
        )
        val selectCreditCardRequest =
            PromptRequest.SelectCreditCard(listOf(creditCard), onConfirm, onDismiss)

        whenever(creditCardPickerView.asView()).thenReturn(mock())
        whenever(creditCardPickerView.asView().visibility).thenReturn(View.VISIBLE)

        feature.start()
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, selectCreditCardRequest))
            .joinBlocking()

        verify(creditCardPicker).handleSelectCreditCardRequest(selectCreditCardRequest)

        // Simulate reloading page
        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, true)).joinBlocking()

        verify(creditCardPicker).dismissSelectCreditCardRequest(selectCreditCardRequest)
    }

    @Test
    fun `Share prompt calls ShareDelegate`() {
        val delegate: ShareDelegate = mock()
        val activity: Activity = mock()
        val feature = spy(
            PromptFeature(
                activity,
                store,
                customTabId = "custom-tab",
                shareDelegate = delegate,
                fragmentManager = fragmentManager
            ) { }
        )
        feature.start()

        val promptRequest = PromptRequest.Share(ShareData("Title", "Text", null), {}, {}, {})
        store.dispatch(ContentAction.UpdatePromptRequestAction("custom-tab", promptRequest))
            .joinBlocking()

        verify(feature).onPromptRequested(store.state.customTabs.first())
        verify(delegate).showShareSheet(
            eq(activity),
            eq(promptRequest.data),
            onDismiss = any(),
            onSuccess = any()
        )
    }

    @Test
    fun `GIVEN credit card autofill enabled and cards available WHEN getting a SelectCreditCard request THEN that request is handled`() {
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                customTabId = "custom-tab",
                fragmentManager = fragmentManager,
                isCreditCardAutofillEnabled = { true }
            ) { }
        )
        feature.creditCardPicker = creditCardPicker
        feature.start()
        val selectCreditCardRequest = PromptRequest.SelectCreditCard(listOf(mock()), {}, {})

        store.dispatch(ContentAction.UpdatePromptRequestAction("custom-tab", selectCreditCardRequest))
            .joinBlocking()

        verify(feature).onPromptRequested(store.state.customTabs.first())
        verify(creditCardPicker).handleSelectCreditCardRequest(selectCreditCardRequest)
    }

    @Test
    fun `GIVEN credit card autofill enabled but no cards available WHEN getting a SelectCreditCard request THEN that request is not acted upon`() {
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                customTabId = "custom-tab",
                fragmentManager = fragmentManager,
                isCreditCardAutofillEnabled = { true }
            ) { }
        )
        feature.creditCardPicker = creditCardPicker
        feature.start()
        val selectCreditCardRequest = PromptRequest.SelectCreditCard(emptyList(), {}, {})

        store.dispatch(ContentAction.UpdatePromptRequestAction("custom-tab", selectCreditCardRequest))
            .joinBlocking()

        verify(feature).onPromptRequested(store.state.customTabs.first())
        verify(creditCardPicker, never()).handleSelectCreditCardRequest(selectCreditCardRequest)
    }

    @Test
    fun `GIVEN credit card autofill disabled and cards available WHEN getting a SelectCreditCard request THEN that request is handled`() {
        val feature = spy(
            PromptFeature(
                mock<Activity>(),
                store,
                customTabId = "custom-tab",
                fragmentManager = fragmentManager,
                isCreditCardAutofillEnabled = { false }
            ) { }
        )
        feature.creditCardPicker = creditCardPicker
        feature.start()
        val selectCreditCardRequest = PromptRequest.SelectCreditCard(listOf(mock()), {}, {})

        store.dispatch(ContentAction.UpdatePromptRequestAction("custom-tab", selectCreditCardRequest))
            .joinBlocking()

        verify(feature).onPromptRequested(store.state.customTabs.first())
        verify(creditCardPicker, never()).handleSelectCreditCardRequest(selectCreditCardRequest)
    }

    @Test
    fun `GIVEN isCreditCardAutofillEnabled is false WHEN SaveCreditCard request is handled THEN dismiss SaveCreditCard`() {
        val creditCardEntry = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = ""
        )
        val promptRequest = spy(
            PromptRequest.SaveCreditCard(
                creditCard = creditCardEntry,
                onConfirm = {},
                onDismiss = {}
            )
        )
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                isCreditCardAutofillEnabled = { false },
            ) {}
        )
        val session = tab()!!

        feature.handleDialogsRequest(promptRequest, session)

        store.waitUntilIdle()

        verify(feature).dismissDialogRequest(promptRequest, session)
    }

    @Test
    fun `GIVEN creditCardValidationDelegate is null WHEN SaveCreditCard request is handled THEN dismiss SaveCreditCard`() {
        val creditCardEntry = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = ""
        )
        val promptRequest = spy(
            PromptRequest.SaveCreditCard(
                creditCard = creditCardEntry,
                onConfirm = {},
                onDismiss = {}
            )
        )
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                isCreditCardAutofillEnabled = { true },
            ) {}
        )
        val session = tab()!!

        feature.handleDialogsRequest(promptRequest, session)

        store.waitUntilIdle()

        verify(feature).dismissDialogRequest(promptRequest, session)
    }

    @Test
    fun `Selecting an item in a share dialog will consume promptRequest`() {
        val delegate: ShareDelegate = mock()
        val feature = PromptFeature(
            activity = mock(),
            store = store,
            fragmentManager = fragmentManager,
            shareDelegate = delegate
        ) { }
        feature.start()

        var onSuccessCalled = false

        val shareRequest = PromptRequest.Share(
            ShareData("Title", "Text", null),
            onSuccess = { onSuccessCalled = true },
            onFailure = {},
            onDismiss = {}
        )
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, shareRequest)).joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(shareRequest, tab()!!.content.promptRequests[0])
        feature.onConfirm(tabId, shareRequest.uid, null)

        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onSuccessCalled)
    }

    @Test
    fun `Dismissing a share dialog will consume promptRequest`() {
        val delegate: ShareDelegate = mock()
        val feature = PromptFeature(
            activity = mock(),
            store = store,
            fragmentManager = fragmentManager,
            shareDelegate = delegate
        ) { }
        feature.start()

        var onDismissCalled = false

        val shareRequest = PromptRequest.Share(
            ShareData("Title", "Text", null),
            onSuccess = {},
            onFailure = {},
            onDismiss = { onDismissCalled = true }
        )
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, shareRequest)).joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(shareRequest, tab()!!.content.promptRequests[0])
        feature.onCancel(tabId, shareRequest.uid)

        store.waitUntilIdle()
        assertTrue(tab()!!.content.promptRequests.isEmpty())
        assertTrue(onDismissCalled)
    }

    @Test
    fun `dialog will be dismissed if tab ID changes`() {
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                shareDelegate = mock()
            ) { }
        )
        feature.start()

        val shareRequest = PromptRequest.Share(
            ShareData("Title", "Text", null),
            onSuccess = {},
            onFailure = {},
            onDismiss = {}
        )
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, shareRequest)).joinBlocking()

        val fragment = mock<PromptDialogFragment>()
        whenever(fragment.shouldDismissOnLoad).thenReturn(true)
        whenever(fragment.sessionId).thenReturn(tabId)
        feature.activePrompt = WeakReference(fragment)

        val secondTabId = "second-test-tab"
        store.dispatch(
            TabListAction.AddTabAction(
                TabSessionState(
                    id = secondTabId,
                    content = ContentState(url = "mozilla.org")
                ),
                select = true
            )
        ).joinBlocking()

        verify(fragment, times(1)).dismiss()
    }

    @Test
    fun `dialog will be dismissed if tab changes`() {
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                shareDelegate = mock()
            ) { }
        )
        feature.start()

        val shareRequest = PromptRequest.Share(
            ShareData("Title", "Text", null),
            onSuccess = {},
            onFailure = {},
            onDismiss = {}
        )
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, shareRequest)).joinBlocking()

        val fragment = mock<PromptDialogFragment>()
        whenever(fragment.shouldDismissOnLoad).thenReturn(true)
        whenever(fragment.sessionId).thenReturn(tabId)
        feature.activePrompt = WeakReference(fragment)

        val newTabId = "test-tab-2"

        store.dispatch(TabListAction.SelectTabAction(newTabId)).joinBlocking()

        verify(fragment, times(1)).dismiss()
    }

    @Test
    fun `dialog will be dismissed if tab URL changes`() {
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                shareDelegate = mock()
            ) { }
        )
        feature.start()

        val shareRequest = PromptRequest.Share(
            ShareData("Title", "Text", null),
            onSuccess = {},
            onFailure = {},
            onDismiss = {}
        )
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, shareRequest)).joinBlocking()

        val fragment = mock<PromptDialogFragment>()
        whenever(fragment.shouldDismissOnLoad).thenReturn(true)
        feature.activePrompt = WeakReference(fragment)

        store.dispatch(ContentAction.UpdateUrlAction(tabId, "mozilla.org")).joinBlocking()
        verify(fragment, times(1)).dismiss()
    }

    @Test
    fun `prompt will always start the save login dialog with an icon`() {
        val feature = PromptFeature(
            activity = mock(),
            store = store,
            fragmentManager = fragmentManager,
            shareDelegate = mock(),
            isSaveLoginEnabled = { true },
            loginValidationDelegate = mock()
        ) { }
        val loginUsername = "username"
        val loginPassword = "password"
        val entry: LoginEntry = mock()
        `when`(entry.username).thenReturn(loginUsername)
        `when`(entry.password).thenReturn(loginPassword)
        val loginsPrompt = PromptRequest.SaveLoginPrompt(2, listOf(entry), { }, { })
        val websiteIcon: Bitmap = mock()
        val contentState: ContentState = mock()
        val session: TabSessionState = mock()
        val sessionId = "sessionId"
        `when`(contentState.icon).thenReturn(websiteIcon)
        `when`(session.content).thenReturn(contentState)
        `when`(session.id).thenReturn(sessionId)

        feature.handleDialogsRequest(
            loginsPrompt, session
        )

        // Only interested in the icon, but it doesn't hurt to be sure we show a properly configured dialog.
        assertTrue(feature.activePrompt!!.get() is SaveLoginDialogFragment)
        val dialogFragment = feature.activePrompt!!.get() as SaveLoginDialogFragment
        assertEquals(loginUsername, dialogFragment.username)
        assertEquals(loginPassword, dialogFragment.password)
        assertEquals(websiteIcon, dialogFragment.icon)
        assertEquals(sessionId, dialogFragment.sessionId)
    }

    @Test
    fun `save login dialog will not be dismissed on page load`() {
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                shareDelegate = mock()
            ) { }
        )
        feature.start()

        val shareRequest = PromptRequest.Share(
            ShareData("Title", "Text", null),
            onSuccess = {},
            onFailure = {},
            onDismiss = {}
        )
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, shareRequest)).joinBlocking()

        val fragment = spy(
            SaveLoginDialogFragment.newInstance(
                tabId,
                shareRequest.uid,
                false,
                0,
                LoginEntry(
                    origin = "https://www.mozilla.org",
                    username = "username",
                    password = "password"
                )
            )
        )
        feature.activePrompt = WeakReference(fragment)

        store.dispatch(ContentAction.UpdateProgressAction(tabId, 0)).joinBlocking()
        store.dispatch(ContentAction.UpdateProgressAction(tabId, 10)).joinBlocking()
        store.dispatch(ContentAction.UpdateProgressAction(tabId, 100)).joinBlocking()

        verify(fragment, times(0)).dismiss()
    }

    @Test
    fun `confirm dialogs will not be automatically dismissed`() {
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                shareDelegate = mock()
            ) { }
        )
        feature.start()

        val promptRequest = PromptRequest.Confirm(
            "title",
            "message",
            false,
            "positive",
            "negative",
            "neutral",
            { },
            { },
            { },
            { }
        )
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest)).joinBlocking()

        val prompt = feature.activePrompt?.get()
        assertNotNull(prompt)
        assertFalse(prompt!!.shouldDismissOnLoad)
    }

    @Test
    fun `A Repost PromptRequest prompt will be shown as a ConfirmDialogFragment`() {
        val feature = PromptFeature(
            // Proper activity here to allow for the feature to properly execute "container.context.getString"
            activity = Robolectric.buildActivity(Activity::class.java).setup().get(),
            store = store,
            fragmentManager = fragmentManager,
            shareDelegate = mock(),
            isSaveLoginEnabled = { true },
            loginValidationDelegate = mock()
        ) { }
        val repostPromptRequest: PromptRequest.Repost = mock()
        doReturn("uid").`when`(repostPromptRequest).uid

        feature.handleDialogsRequest(repostPromptRequest, mock())

        val dialog: ConfirmDialogFragment = feature.activePrompt!!.get() as ConfirmDialogFragment
        assertEquals(testContext.getString(R.string.mozac_feature_prompt_repost_title), dialog.title)
        assertEquals(testContext.getString(R.string.mozac_feature_prompt_repost_message), dialog.message)
        assertEquals(
            testContext.getString(R.string.mozac_feature_prompt_repost_positive_button_text),
            dialog.positiveButtonText
        )
        assertEquals(
            testContext.getString(R.string.mozac_feature_prompt_repost_negative_button_text),
            dialog.negativeButtonText
        )
    }

    @Test
    fun `Positive button on a Repost dialog will call onAccept and consume the dialog`() {
        val feature = PromptFeature(
            activity = Robolectric.buildActivity(Activity::class.java).setup().get(),
            store = store,
            fragmentManager = fragmentManager
        ) { }
        feature.start()

        var acceptCalled = false
        val repostRequest = PromptRequest.Repost(
            { acceptCalled = true }, { }
        )
        store
            .dispatch(ContentAction.UpdatePromptRequestAction(tabId, repostRequest))
            .joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(repostRequest, tab()!!.content.promptRequests[0])
        feature.onConfirm(tabId, repostRequest.uid, null)

        store.waitUntilIdle()
        assertTrue(acceptCalled)
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `Negative button on a Repost dialog will call onDismiss and consume the dialog`() {
        val feature = PromptFeature(
            activity = Robolectric.buildActivity(Activity::class.java).setup().get(),
            store = store,
            fragmentManager = fragmentManager
        ) { }
        feature.start()

        var dismissCalled = false
        val repostRequest = PromptRequest.Repost(
            { }, { dismissCalled = true }
        )
        store
            .dispatch(ContentAction.UpdatePromptRequestAction(tabId, repostRequest))
            .joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(repostRequest, tab()!!.content.promptRequests[0])
        feature.onCancel(tabId, repostRequest.uid)

        store.waitUntilIdle()
        assertTrue(dismissCalled)
        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `WHEN onConfirm is called on a SaveCreditCard dialog THEN a confirm request will consume the dialog`() {
        val feature = PromptFeature(
            activity = mock(),
            store = store,
            fragmentManager = fragmentManager,
            isCreditCardAutofillEnabled = { true },
            creditCardValidationDelegate = mock()
        ) { }
        val creditCardEntry = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = ""
        )

        val request = PromptRequest.SaveCreditCard(
            creditCard = creditCardEntry,
            onConfirm = {},
            onDismiss = {}
        )

        feature.start()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, request)).joinBlocking()

        assertEquals(1, tab()!!.content.promptRequests.size)

        feature.onConfirm(
            sessionId = tabId,
            promptRequestUID = request.uid,
            value = creditCardEntry
        )

        store.waitUntilIdle()

        assertTrue(tab()!!.content.promptRequests.isEmpty())
    }

    @Test
    fun `WHEN a credit card is confirmed to save THEN confirm the prompt request with the selected credit card`() {
        val creditCardEntry = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = ""
        )
        var onDismissCalled = false
        var onConfirmCalled = false
        var confirmedCreditCard: CreditCardEntry? = null

        val request = PromptRequest.SaveCreditCard(
            creditCard = creditCardEntry,
            onConfirm = {
                confirmedCreditCard = it
                onConfirmCalled = true
            },
            onDismiss = {
                onDismissCalled = true
            }
        )

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, request)).joinBlocking()

        request.onConfirm(creditCardEntry)

        store.waitUntilIdle()

        assertEquals(creditCardEntry, confirmedCreditCard)
        assertTrue(onConfirmCalled)

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, request)).joinBlocking()

        request.onDismiss()

        store.waitUntilIdle()

        assertTrue(onDismissCalled)
    }

    @Test
    fun `WHEN the save credit card dialog fragment is created THEN the credit card entry is passed into the instance`() {
        val feature = PromptFeature(
            activity = mock(),
            store = store,
            fragmentManager = fragmentManager,
            isCreditCardAutofillEnabled = { true },
            creditCardValidationDelegate = mock()
        ) { }
        val creditCardEntry = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = ""
        )
        val request = PromptRequest.SaveCreditCard(
            creditCard = creditCardEntry,
            onConfirm = {},
            onDismiss = {}
        )

        val contentState: ContentState = mock()
        val session: TabSessionState = mock()
        val sessionId = "sessionId"

        `when`(session.content).thenReturn(contentState)
        `when`(session.id).thenReturn(sessionId)

        feature.handleDialogsRequest(
            promptRequest = request,
            session = session
        )

        assertTrue(feature.activePrompt!!.get() is CreditCardSaveDialogFragment)

        val dialogFragment = feature.activePrompt!!.get() as CreditCardSaveDialogFragment

        assertEquals(sessionId, dialogFragment.sessionId)
        assertEquals(creditCardEntry, dialogFragment.creditCard)
    }

    @Test
    fun `GIVEN SaveCreditCard prompt is shown WHEN prompt is removed from state THEN dismiss SaveCreditCard prompt`() {
        val creditCardEntry = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = ""
        )
        val promptRequest = PromptRequest.SaveCreditCard(
            creditCard = creditCardEntry,
            onConfirm = {},
            onDismiss = {}
        )
        val dialogFragment: CreditCardSaveDialogFragment = mock()

        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, promptRequest))
            .joinBlocking()
        store.waitUntilIdle()

        val feature = PromptFeature(
            activity = mock(),
            store = store,
            fragmentManager = fragmentManager,
            isCreditCardAutofillEnabled = { true },
            creditCardValidationDelegate = mock()
        ) { }

        feature.start()
        feature.activePrompt = WeakReference(dialogFragment)
        feature.activePromptRequest = promptRequest

        store.dispatch(ContentAction.ConsumePromptRequestAction(tabId, promptRequest))
            .joinBlocking()

        verify(dialogFragment).dismissAllowingStateLoss()
    }

    @Test
    fun `WHEN promptRequest is updated THEN the replaced active prompt will be dismissed`() {
        val feature = spy(
            PromptFeature(
                activity = mock(),
                store = store,
                fragmentManager = fragmentManager,
                shareDelegate = mock()
            ) { }
        )
        feature.start()

        val previousPrompt = SingleChoice(
            choices = arrayOf(),
            onConfirm = {},
            onDismiss = {}
        )
        val updatedPrompt = SingleChoice(
            choices = arrayOf(),
            onConfirm = {},
            onDismiss = {}
        )
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, previousPrompt)).joinBlocking()

        val fragment = mock<ChoiceDialogFragment>()
        whenever(fragment.shouldDismissOnLoad).thenReturn(true)
        feature.activePrompt = WeakReference(fragment)

        store.dispatch(ContentAction.ReplacePromptRequestAction(tabId, previousPrompt.uid, updatedPrompt)).joinBlocking()
        verify(fragment).dismiss()
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(any())
        return fragmentManager
    }
}
