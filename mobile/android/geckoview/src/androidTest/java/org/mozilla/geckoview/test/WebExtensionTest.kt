/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.core.IsEqual.equalTo
import org.hamcrest.core.StringEndsWith.endsWith
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Assume.assumeThat
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.GeckoSession.PermissionDelegate
import org.mozilla.geckoview.GeckoSession.ProgressDelegate
import org.mozilla.geckoview.WebExtension.*
import org.mozilla.geckoview.WebExtension.BrowsingDataDelegate.Type.*
import org.mozilla.geckoview.WebExtensionController.EnableSource
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.Setting
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.RejectedPromiseException
import org.mozilla.geckoview.test.util.RuntimeCreator
import org.mozilla.geckoview.test.util.UiThreadUtils
import java.nio.charset.Charset
import java.util.*
import java.util.concurrent.CancellationException
import kotlin.collections.HashMap
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay

@RunWith(AndroidJUnit4::class)
@MediumTest
class WebExtensionTest : BaseSessionTest() {
    companion object {
        private const val TABS_CREATE_BACKGROUND: String =
                "resource://android/assets/web_extensions/tabs-create/"
        private const val TABS_CREATE_2_BACKGROUND: String =
                "resource://android/assets/web_extensions/tabs-create-2/"
        private const val TABS_CREATE_REMOVE_BACKGROUND: String =
                "resource://android/assets/web_extensions/tabs-create-remove/"
        private const val TABS_ACTIVATE_REMOVE_BACKGROUND: String =
                "resource://android/assets/web_extensions/tabs-activate-remove/"
        private const val TABS_REMOVE_BACKGROUND: String =
                "resource://android/assets/web_extensions/tabs-remove/"
        private const val MESSAGING_BACKGROUND: String =
                "resource://android/assets/web_extensions/messaging/"
        private const val MESSAGING_CONTENT: String =
                "resource://android/assets/web_extensions/messaging-content/"
        private const val OPENOPTIONSPAGE_1_BACKGROUND: String =
                "resource://android/assets/web_extensions/openoptionspage-1/"
        private const val OPENOPTIONSPAGE_2_BACKGROUND: String =
                "resource://android/assets/web_extensions/openoptionspage-2/"
        private const val EXTENSION_PAGE_RESTORE: String =
                "resource://android/assets/web_extensions/extension-page-restore/"
        private const val BROWSING_DATA: String =
                "resource://android/assets/web_extensions/browsing-data-built-in/"
    }

    private val controller
            get() = sessionRule.runtime.webExtensionController

    @Before
    fun setup() {
        sessionRule.setPrefsUntilTestEnd(mapOf("extensions.isembedded" to true))
        sessionRule.runtime.webExtensionController.setTabActive(mainSession, true)
    }

    @Test
    fun installBuiltIn() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        // Load the WebExtension that will add a border to the body
        val borderify = sessionRule.waitForResult(controller.installBuiltIn(
                "resource://android/assets/web_extensions/borderify/"
        ))

        assertTrue(borderify.isBuiltIn)

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        // Uninstall WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(borderify))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being uninstalled
        assertBodyBorderEqualTo("")
    }

    private fun assertBodyBorderEqualTo(expected: String) {
        val color = mainSession.evaluateJS("document.body.style.borderColor")
        assertThat("The border color should be '$expected'",
                color as String, equalTo(expected))
    }

    private fun checkDisabledState(extension: WebExtension,
                                   userDisabled: Boolean = false, appDisabled: Boolean = false,
                                   blocklistDisabled: Boolean = false) {

        val enabled = !userDisabled && !appDisabled && !blocklistDisabled

        mainSession.reload()
        sessionRule.waitForPageStop()

        if (!enabled) {
            // Border should be empty because the extension is disabled
            assertBodyBorderEqualTo("")
        } else {
            assertBodyBorderEqualTo("red")
        }

        assertThat("enabled should match",
                extension.metaData.enabled, equalTo(enabled))
        assertThat("userDisabled should match",
                extension.metaData.disabledFlags and DisabledFlags.USER > 0,
                equalTo(userDisabled))
        assertThat("appDisabled should match",
                extension.metaData.disabledFlags and DisabledFlags.APP > 0,
                equalTo(appDisabled))
        assertThat("blocklistDisabled should match",
                extension.metaData.disabledFlags and DisabledFlags.BLOCKLIST > 0,
                equalTo(blocklistDisabled))
    }

    @Test
    fun noDelegateErrorMessage() {
        try {
            sessionRule.evaluateExtensionJS("""
                const [tab] = await browser.tabs.query({ active: true, currentWindow: true });
                await browser.tabs.update(tab.id, { url: "www.google.com" });
            """)
            assertThat("tabs.update should not succeed", true, equalTo(false))
        } catch (ex: RejectedPromiseException) {
            assertThat("Error message matches", ex.message,
                    equalTo("Error: tabs.update is not supported"))
        }

        try {
            sessionRule.evaluateExtensionJS("""
                const [tab] = await browser.tabs.query({ active: true, currentWindow: true });
                await browser.tabs.remove(tab.id);
            """)
            assertThat("tabs.remove should not succeed", true, equalTo(false))
        } catch (ex: RejectedPromiseException) {
            assertThat("Error message matches", ex.message,
                    equalTo("Error: tabs.remove is not supported"))
        }

        try {
            sessionRule.evaluateExtensionJS("""
                await browser.runtime.openOptionsPage();
            """)
            assertThat("runtime.openOptionsPage should not succeed",
                    true, equalTo(false))
        } catch (ex: RejectedPromiseException) {
            assertThat("Error message matches", ex.message,
                    equalTo("Error: runtime.openOptionsPage is not supported"))
        }
    }

    @Test
    fun enableDisable() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        var borderify = sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/borderify.xpi"))
        checkDisabledState(borderify, userDisabled=false, appDisabled=false)

        borderify = sessionRule.waitForResult(controller.disable(borderify, EnableSource.USER))
        checkDisabledState(borderify, userDisabled=true, appDisabled=false)

        borderify = sessionRule.waitForResult(controller.disable(borderify, EnableSource.APP))
        checkDisabledState(borderify, userDisabled=true, appDisabled=true)

        borderify = sessionRule.waitForResult(controller.enable(borderify, EnableSource.APP))
        checkDisabledState(borderify, userDisabled=true, appDisabled=false)

        borderify = sessionRule.waitForResult(controller.enable(borderify, EnableSource.USER))
        checkDisabledState(borderify, userDisabled=false, appDisabled=false)

        borderify = sessionRule.waitForResult(controller.disable(borderify, EnableSource.APP))
        checkDisabledState(borderify, userDisabled=false, appDisabled=true)

        borderify = sessionRule.waitForResult(controller.enable(borderify, EnableSource.APP))
        checkDisabledState(borderify, userDisabled=false, appDisabled=false)

        sessionRule.waitForResult(controller.uninstall(borderify))
        mainSession.reload()
        sessionRule.waitForPageStop()

        // Border should be empty because the extension is not installed anymore
        assertBodyBorderEqualTo("")
    }

    @Test
    fun installWebExtension() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData.description,
                        "Adds a red border to all webpages matching example.com.")
                assertEquals(extension.metaData.name, "Borderify")
                assertEquals(extension.metaData.version, "1.0")
                assertEquals(extension.isBuiltIn, false)
                assertEquals(extension.metaData.enabled, false)
                assertEquals(extension.metaData.signedState,
                        WebExtension.SignedStateFlags.SIGNED)
                assertEquals(extension.metaData.blocklistState,
                        WebExtension.BlocklistStateFlags.NOT_BLOCKED)

                return GeckoResult.allow()
            }
        })

        val borderify = sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/borderify.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        var list = extensionsMap(sessionRule.waitForResult(controller.list()))
        assertEquals(list.size, 2)
        assertTrue(list.containsKey(borderify.id))
        assertTrue(list.containsKey(RuntimeCreator.TEST_SUPPORT_EXTENSION_ID))

        // Uninstall WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(borderify))

        list = extensionsMap(sessionRule.waitForResult(controller.list()))
        assertEquals(list.size, 1)
        assertTrue(list.containsKey(RuntimeCreator.TEST_SUPPORT_EXTENSION_ID))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being uninstalled
        assertBodyBorderEqualTo("")
    }

    @Test
    @Setting.List(Setting(key = Setting.Key.USE_PRIVATE_MODE, value = "true"))
    fun runInPrivateBrowsing() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // Make sure border is empty before running the extension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count=1)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        var borderify = sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/borderify.xpi"))

        // Make sure private mode is enabled
        assertTrue(mainSession.settings.usePrivateMode)
        assertFalse(borderify.metaData.allowedInPrivateBrowsing)
        // Check that the WebExtension was not applied to a private mode page
        assertBodyBorderEqualTo("")

        borderify = sessionRule.waitForResult(
                controller.setAllowedInPrivateBrowsing(borderify, true))

        assertTrue(borderify.metaData.allowedInPrivateBrowsing)
        // Check that the WebExtension was applied to a private mode page now that the extension
        // is enabled in private mode
        mainSession.reload();
        sessionRule.waitForPageStop()
        assertBodyBorderEqualTo("red")

        borderify = sessionRule.waitForResult(
                controller.setAllowedInPrivateBrowsing(borderify, false))

        assertFalse(borderify.metaData.allowedInPrivateBrowsing)
        // Check that the WebExtension was not applied to a private mode page after being
        // not allowed to run in private mode
        mainSession.reload();
        sessionRule.waitForPageStop()
        assertBodyBorderEqualTo("")

        // Uninstall WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(borderify))
        mainSession.reload();
        sessionRule.waitForPageStop()
        assertBodyBorderEqualTo("")
    }

    @Test
    fun optionsPageMetadata() {
        // dummy.xpi is not signed, but it could be
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false
        ))

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count=1)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        val dummy = sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/dummy.xpi"))

        val metadata = dummy.metaData
        assertTrue((metadata.optionsPageUrl ?: "").matches("^moz-extension://[0-9a-f\\-]*/options.html$".toRegex()));
        assertEquals(metadata.openOptionsPageInTab, true);
        assertTrue(metadata.baseUrl.matches("^moz-extension://[0-9a-f\\-]*/$".toRegex()))

        sessionRule.waitForResult(controller.uninstall(dummy))
    }

    @Test
    fun installMultiple() {
        // dummy.xpi is not signed, but it could be
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false
        ))

        // First, make sure the list only contains the test support extension
        var list = extensionsMap(sessionRule.waitForResult(controller.list()))
        assertEquals(list.size, 1)
        assertTrue(list.containsKey(RuntimeCreator.TEST_SUPPORT_EXTENSION_ID))

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count=2)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        // Install in parallell borderify and dummy
        val borderifyResult = controller.install(
                "resource://android/assets/web_extensions/borderify.xpi")
        val dummyResult = controller.install(
                "resource://android/assets/web_extensions/dummy.xpi")

        val (borderify, dummy) = sessionRule.waitForResult(
                GeckoResult.allOf(borderifyResult, dummyResult))

        // Make sure the list is updated accordingly
        list = extensionsMap(sessionRule.waitForResult(controller.list()))
        assertTrue(list.containsKey(borderify.id))
        assertTrue(list.containsKey(dummy.id))
        assertTrue(list.containsKey(RuntimeCreator.TEST_SUPPORT_EXTENSION_ID))
        assertEquals(list.size, 3)

        // Uninstall borderify and verify that it's not in the list anymore
        sessionRule.waitForResult(controller.uninstall(borderify))

        list = extensionsMap(sessionRule.waitForResult(controller.list()))
        assertEquals(list.size, 2)
        assertTrue(list.containsKey(dummy.id))
        assertTrue(list.containsKey(RuntimeCreator.TEST_SUPPORT_EXTENSION_ID))
        assertFalse(list.containsKey(borderify.id))

        // Uninstall dummy and make sure the list is now empty
        sessionRule.waitForResult(controller.uninstall(dummy))

        list = extensionsMap(sessionRule.waitForResult(controller.list()))
        assertEquals(list.size, 1)
        assertTrue(list.containsKey(RuntimeCreator.TEST_SUPPORT_EXTENSION_ID))
    }

    private fun testInstallError(name: String, expectedError: Int) {
        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count = 0)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/$name")
                        .accept({
                            // We should not be able to install unsigned extensions
                            assertTrue(false)
                        }, { exception ->
                            val installException = exception as WebExtension.InstallException
                            assertEquals(installException.code, expectedError)
                        }))
    }

    private fun extensionsMap(extensionList: List<WebExtension>): Map<String, WebExtension> {
        val map = HashMap<String, WebExtension>()
        for (extension in extensionList) {
            map.put(extension.id, extension);
        }
        return map
    }

    @Test
    fun installUnsignedExtensionSignatureNotRequired() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false
        ))

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        val borderify = sessionRule.waitForResult(controller.install(
                "resource://android/assets/web_extensions/borderify-unsigned.xpi")
                .then { extension ->
                    assertEquals(extension!!.metaData.signedState,
                            WebExtension.SignedStateFlags.MISSING)
                    assertEquals(extension.metaData.blocklistState,
                            WebExtension.BlocklistStateFlags.NOT_BLOCKED)
                    assertEquals(extension.metaData.name, "Borderify")
                    GeckoResult.fromValue(extension)
                })

        sessionRule.waitForResult(controller.uninstall(borderify))
    }

    @Test
    fun installUnsignedExtensionSignatureRequired() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to true
        ))
        testInstallError("borderify-unsigned.xpi",
                WebExtension.InstallException.ErrorCodes.ERROR_SIGNEDSTATE_REQUIRED)
    }

    @Test
    fun installExtensionFileNotFound() {
        testInstallError("file-not-found.xpi",
                WebExtension.InstallException.ErrorCodes.ERROR_NETWORK_FAILURE)
    }

    @Test
    fun installExtensionMissingId() {
        testInstallError("borderify-missing-id.xpi",
                WebExtension.InstallException.ErrorCodes.ERROR_CORRUPT_FILE)
    }

    @Test
    fun installDeny() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // Ensure border is empty to start.
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.deny()
            }
        })

        sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/borderify.xpi").accept({
            // We should not be able to install the extension.
            assertTrue(false)
        }, { exception ->
            assertTrue(exception is WebExtension.InstallException)
            val installException = exception as WebExtension.InstallException
            assertEquals(installException.code, WebExtension.InstallException.ErrorCodes.ERROR_USER_CANCELED)
        }));

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not installed and the border is still empty.
        assertBodyBorderEqualTo("")
    }

    @Test
    fun createNotification() {
        sessionRule.delegateUntilTestEnd(object : WebNotificationDelegate {
            @AssertCalled
            override fun onShowNotification(notification: WebNotification) {
            }
        })

        val extension = sessionRule.waitForResult(
                controller.installBuiltIn("resource://android/assets/web_extensions/notification-test/"))

        sessionRule.waitUntilCalled(object : WebNotificationDelegate {
            @AssertCalled(count = 1)
            override fun onShowNotification(notification: WebNotification) {
                assertEquals(notification.title, "Time for cake!")
                assertEquals(notification.text, "Something something cake")
                assertEquals(notification.imageUrl, "https://example.com/img.svg")
                // This should be filled out, Bug 1589693
                assertEquals(notification.source, null)
            }
        })

        sessionRule.waitForResult(
                controller.uninstall(extension))
    }

    // This test
    // - Registers a web extension
    // - Listens for messages and waits for a message
    // - Sends a response to the message and waits for a second message
    // - Verify that the second message has the correct value
    //
    // When `background == true` the test will be run using background messaging, otherwise the
    // test will use content script messaging.
    private fun testOnMessage(background: Boolean) {
        val messageResult = GeckoResult<Void>()

        val prefix = if (background) "testBackground" else "testContent"

        val messageDelegate = object : WebExtension.MessageDelegate {
            var awaitingResponse = false
            var completed = false

            override fun onConnect(port: WebExtension.Port) {
                // Ignored for this test
            }

            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                checkSender(nativeApp, sender, background)

                if (!awaitingResponse) {
                    assertThat("We should receive a message from the WebExtension", message as String,
                            equalTo("${prefix}BrowserMessage"))
                    awaitingResponse = true
                    return GeckoResult.fromValue("${prefix}MessageResponse")
                } else if (!completed) {
                    assertThat("The background script should receive our message and respond",
                            message as String, equalTo("response: ${prefix}MessageResponse"))
                    messageResult.complete(null)
                    completed = true
                }
                return null
            }
        }

        val messaging = installWebExtension(background, messageDelegate)
        sessionRule.waitForResult(messageResult)

        sessionRule.waitForResult(controller.uninstall(messaging))
    }

    // This test
    // - Listen for a new tab request from a web extension
    // - Registers a web extension
    // - Waits for onNewTab request
    // - Verify that request came from right extension
    @Test
    fun testBrowserTabsCreate() {
        val tabsCreateResult = GeckoResult<Void>()
        var tabsExtension: WebExtension? = null
        val tabDelegate = object : WebExtension.TabDelegate {
            override fun onNewTab(source: WebExtension, details: WebExtension.CreateTabDetails): GeckoResult<GeckoSession> {
                assertEquals(details.url, "https://www.mozilla.org/en-US/")
                assertEquals(details.active, true)
                assertEquals(tabsExtension!!, source)
                tabsCreateResult.complete(null)
                return GeckoResult.fromValue(null)
            }
        }

        tabsExtension = sessionRule.waitForResult(controller.installBuiltIn(TABS_CREATE_BACKGROUND))
        tabsExtension.setTabDelegate(tabDelegate)
        sessionRule.waitForResult(tabsCreateResult)

        sessionRule.waitForResult(controller.uninstall(tabsExtension))
    }

    // This test
    // - Listen for a new tab request from a web extension
    // - Registers a web extension
    // - Extension requests creation of new tab with a cookie store id.
    // - Waits for onNewTab request
    // - Verify that request came from right extension
    @Test
    fun testBrowserTabsCreateWithCookieStoreId() {
        sessionRule.setPrefsUntilTestEnd(mapOf("privacy.userContext.enabled" to true));
        val tabsCreateResult = GeckoResult<Void>()
        var tabsExtension: WebExtension? = null
        val tabDelegate = object : WebExtension.TabDelegate {
            override fun onNewTab(source: WebExtension, details: WebExtension.CreateTabDetails): GeckoResult<GeckoSession> {
                assertEquals(details.url, "https://www.mozilla.org/en-US/")
                assertEquals(details.active, true)
                assertEquals(details.cookieStoreId, "1")
                assertEquals(tabsExtension!!.id, source.id)
                tabsCreateResult.complete(null)
                return GeckoResult.fromValue(null)
            }
        }

        tabsExtension = sessionRule.waitForResult(controller.installBuiltIn(TABS_CREATE_2_BACKGROUND))
        tabsExtension.setTabDelegate(tabDelegate)
        sessionRule.waitForResult(tabsCreateResult)

        sessionRule.waitForResult(controller.uninstall(tabsExtension))
    }

    // This test
    // - Create and assign WebExtension TabDelegate to handle creation and closing of tabs
    // - Registers a WebExtension
    // - Extension requests creation of new tab
    // - TabDelegate handles creation of new tab
    // - Extension requests removal of newly created tab
    // - TabDelegate handles closing of newly created tab
    // - Verify that close request came from right extension and targeted session
    @Test
    fun testBrowserTabsCreateBrowserTabsRemove() {
        val onCloseRequestResult = GeckoResult<Void>()
        val tabsExtension = sessionRule.waitForResult(
                controller.installBuiltIn(TABS_CREATE_REMOVE_BACKGROUND))

        tabsExtension.tabDelegate = object : WebExtension.TabDelegate {
            override fun onNewTab(source: WebExtension, details: WebExtension.CreateTabDetails): GeckoResult<GeckoSession> {
                val extensionCreatedSession = sessionRule.createClosedSession(mainSession.settings)

                extensionCreatedSession.webExtensionController.setTabDelegate(tabsExtension, object : WebExtension.SessionTabDelegate {
                    override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                        assertEquals(tabsExtension.id, source!!.id)
                        assertEquals(details.active, true)
                        assertNotEquals(null, extensionCreatedSession)
                        assertEquals(extensionCreatedSession, session)
                        onCloseRequestResult.complete(null)
                        return GeckoResult.allow()
                    }
                })

                return GeckoResult.fromValue(extensionCreatedSession)
            }
        };

        sessionRule.waitForResult(onCloseRequestResult)
        sessionRule.waitForResult(controller.uninstall(tabsExtension))
    }

    // This test
    // - Create and assign WebExtension TabDelegate to handle creation and closing of tabs
    // - Create and opens a new GeckoSession
    // - Set the main session as active tab
    // - Registers a WebExtension
    // - Extension listens for activated tab changes
    // - Set the main session as inactive tab
    // - Set the newly created GeckoSession as active tab
    // - Extension requests removal of newly created tab if tabs.query({active: true})
    //     contains only the newly activated tab
    // - TabDelegate handles closing of newly created tab
    // - Verify that close request came from right extension and targeted session
    @Test
    fun testSetTabActive() {
        val onCloseRequestResult = GeckoResult<Void>()
        val tabsExtension = sessionRule.waitForResult(
                controller.installBuiltIn(TABS_ACTIVATE_REMOVE_BACKGROUND))
        val newTabSession = sessionRule.createOpenSession(mainSession.settings)

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtension.SessionTabDelegate::class,
                { delegate -> newTabSession.webExtensionController.setTabDelegate(tabsExtension, delegate) },
                { newTabSession.webExtensionController.setTabDelegate(tabsExtension, null) },
                object : WebExtension.SessionTabDelegate {

            override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(tabsExtension, source)
                assertEquals(newTabSession, session)
                onCloseRequestResult.complete(null)
                return GeckoResult.allow()
            }
        })

        controller.setTabActive(mainSession, false)
        controller.setTabActive(newTabSession, true)

        sessionRule.waitForResult(onCloseRequestResult)
        sessionRule.waitForResult(controller.uninstall(tabsExtension))
    }

    private fun browsingDataMessage(port: WebExtension.Port, type: String,
                                    since: Long? = null): GeckoResult<JSONObject> {
        val message = JSONObject("{" +
                "\"type\": \"$type\"" +
                "}")
        if (since != null) {
            message.put("since", since)
        }
        return browsingDataCall(port, message)
    }

    private fun browsingDataCall(port: WebExtension.Port,
                                 json: JSONObject): GeckoResult<JSONObject> {
        val uuid = UUID.randomUUID().toString()
        json.put("uuid", uuid)
        port.postMessage(json)

        val response = GeckoResult<JSONObject>()
        port.setDelegate(object : WebExtension.PortDelegate {
            override fun onPortMessage(message: Any, port: WebExtension.Port) {
                assertThat("Response ID Matches.",
                        (message as JSONObject).getString("uuid"), equalTo(uuid))
                response.complete(message)
            }
        })
        return response
    }

    @Test
    fun testBrowsingDataDelegateBuiltIn() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))

        val extension = sessionRule.waitForResult(
                controller.installBuiltIn(BROWSING_DATA))

        val portResult = GeckoResult<WebExtension.Port>()
        extension.setMessageDelegate(object : WebExtension.MessageDelegate {
            override fun onConnect(port: WebExtension.Port) {
                portResult.complete(port)
            }
        }, "browser")

        val TEST_SINCE_VALUE = 59294;

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtension.BrowsingDataDelegate::class,
                { delegate -> extension.browsingDataDelegate = delegate },
                { extension.browsingDataDelegate = null },
                object : WebExtension.BrowsingDataDelegate {
                    override fun onGetSettings(): GeckoResult<WebExtension.BrowsingDataDelegate.Settings>? {
                        return GeckoResult.fromValue(WebExtension.BrowsingDataDelegate.Settings(
                                TEST_SINCE_VALUE,
                                CACHE or COOKIES or DOWNLOADS or HISTORY or LOCAL_STORAGE,
                                CACHE or COOKIES or HISTORY
                        ))
                    }
                })

        val port = sessionRule.waitForResult(portResult)

        // Test browsingData.removeDownloads
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            @AssertCalled
            override fun onClearDownloads(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(1234L))
                return null
            }
        })
        sessionRule.waitForResult(browsingDataMessage(port, "clear-downloads", 1234))

        // Test browsingData.removeFormData
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            @AssertCalled
            override fun onClearFormData(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(1234L))
                return null
            }
        })
        sessionRule.waitForResult(browsingDataMessage(port,"clear-form-data", 1234))

        // Test browsingData.removeHistory
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            @AssertCalled
            override fun onClearHistory(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(1234L))
                return null
            }
        })
        sessionRule.waitForResult(browsingDataMessage(port, "clear-history", 1234))

        // Test browsingData.removePasswords
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            @AssertCalled
            override fun onClearPasswords(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(1234L))
                return null
            }
        })
        sessionRule.waitForResult(browsingDataMessage(port, "clear-passwords", 1234))

        // Test browsingData.remove({ indexedDB: true, localStorage: true, passwords: true })
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            @AssertCalled
            override fun onClearPasswords(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(0L))
                return null
            }
        })
        var response = sessionRule.waitForResult(browsingDataCall(port,
                JSONObject("{" +
                "\"type\": \"clear\"," +
                "\"removalOptions\": {}," +
                "\"dataTypes\": {\"indexedDB\": true, \"localStorage\": true, \"passwords\": true}" +
                "}")))
        assertThat("browsingData.remove should succeed",
                response.getString("type"),
                equalTo("response"))

        // Test browsingData.remove({ indexedDB: true, history: true, passwords: true })
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            @AssertCalled
            override fun onClearPasswords(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(0L))
                return null
            }
            @AssertCalled
            override fun onClearHistory(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(0L))
                return null
            }
        })
        response = sessionRule.waitForResult(browsingDataCall(port,
                JSONObject("{" +
                "\"type\": \"clear\"," +
                "\"removalOptions\": {}," +
                "\"dataTypes\": {\"indexedDB\": true, \"history\": true, \"passwords\": true}" +
                "}")))
        assertThat("browsingData.remove should succeed",
            response.getString("type"),
            equalTo("response"))

        // Test browsingData.remove({ indexedDB: true, history: true, passwords: true })
        // with failure
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            @AssertCalled
            override fun onClearPasswords(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(0L))
                return null
            }
            @AssertCalled
            override fun onClearHistory(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(0L))
                return GeckoResult.fromException(RuntimeException("Not authorized."));
            }
        })
        response = sessionRule.waitForResult(browsingDataCall(port,
                JSONObject("{" +
                "\"type\": \"clear\"," +
                "\"removalOptions\": {}," +
                "\"dataTypes\": {\"indexedDB\": true, \"history\": true, \"passwords\": true}" +
                "}")))
        assertThat("browsingData.remove returns expected error.",
            response.getString("error"),
            equalTo("Not authorized."))

        // Test browsingData.remove({ indexedDB: true, history: true, passwords: true })
        // with multiple failures
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            @AssertCalled
            override fun onClearPasswords(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(0L))
                return GeckoResult.fromException(RuntimeException("Not authorized passwords."));
            }
            @AssertCalled
            override fun onClearHistory(sinceUnixTimestamp: Long): GeckoResult<Void>? {
                assertThat("timestamp should match", sinceUnixTimestamp,
                        equalTo(0L))
                return GeckoResult.fromException(RuntimeException("Not authorized history."));
            }
        })
        response = sessionRule.waitForResult(browsingDataCall(port,
                JSONObject("{" +
                        "\"type\": \"clear\"," +
                        "\"removalOptions\": {}," +
                        "\"dataTypes\": {\"indexedDB\": true, \"history\": true, \"passwords\": true}" +
                        "}")))
        val error = response.getString("error")
        assertThat("browsingData.remove returns expected error.",
                error == "Not authorized passwords." || error == "Not authorized history.",
                equalTo(true))

        // Test browsingData.settings()
        response = sessionRule.waitForResult(
                browsingDataMessage(port, "get-settings"))

        val settings = response.getJSONObject("result")
        val dataToRemove = settings.getJSONObject("dataToRemove")
        val options = settings.getJSONObject("options")

        assertThat("Since should be correct",
                options.getInt("since"), equalTo(TEST_SINCE_VALUE))
        for (key in listOf("cache", "cookies", "history")) {
            assertThat("Data to remove should be correct",
                    dataToRemove.getBoolean(key), equalTo(true))
        }
        for (key in listOf("downloads", "localStorage")) {
            assertThat("Data to remove should be correct",
                    dataToRemove.getBoolean(key), equalTo(false))
        }

        val dataRemovalPermitted = settings.getJSONObject("dataRemovalPermitted")
        for (key in listOf("cache", "cookies", "downloads", "history", "localStorage")) {
            assertThat("Data removal permitted should be correct",
                    dataRemovalPermitted.getBoolean(key), equalTo(true))
        }

        // Test browsingData.settings() with no delegate
        sessionRule.delegateDuringNextWait(object : WebExtension.BrowsingDataDelegate {
            override fun onGetSettings(): GeckoResult<WebExtension.BrowsingDataDelegate.Settings>? {
                return null
            }
        })
        response = sessionRule.waitForResult(
                browsingDataMessage(port, "get-settings"))
        assertThat("browsingData.settings returns expected error.",
                response.getString("error"),
                equalTo("browsingData.settings is not supported"))

        sessionRule.waitForResult(controller.uninstall(extension))
    }

    @Test
    fun testBrowsingDataDelegate() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        val extension = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/browsing-data.xpi"))

        val accumulator = mutableListOf<String>()
        val result = GeckoResult<List<String>>()

        extension.browsingDataDelegate = object : WebExtension.BrowsingDataDelegate {
            fun register(type: String, timestamp: Long) {
                accumulator.add("$type $timestamp")
                if (accumulator.size >= 5) {
                    result.complete(accumulator)
                }
            }

            override fun onClearDownloads(sinceUnixTimestamp: Long): GeckoResult<Void> {
                register("downloads", sinceUnixTimestamp)
                return GeckoResult.fromValue(null);
            }

            override fun onClearFormData(sinceUnixTimestamp: Long): GeckoResult<Void> {
                register("formData", sinceUnixTimestamp)
                return GeckoResult.fromValue(null);
            }

            override fun onClearHistory(sinceUnixTimestamp: Long): GeckoResult<Void> {
                register("history", sinceUnixTimestamp)
                return GeckoResult.fromValue(null);
            }

            override fun onClearPasswords(sinceUnixTimestamp: Long): GeckoResult<Void> {
                register("passwords", sinceUnixTimestamp)
                return GeckoResult.fromValue(null);
            }
        }

        val actual = sessionRule.waitForResult(result)
        assertThat("Delegate methods get called in the right order",
            actual, equalTo(listOf(
                "downloads 10001",
                "formData 10002",
                "history 10003",
                "passwords 10004",
                "downloads 10005"
        )))

        sessionRule.waitForResult(controller.uninstall(extension))
    }

    // Same as testSetTabActive when the extension is not allowed in private browsing
    @Test
    fun testSetTabActiveNotAllowedInPrivateBrowsing() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))

        val onCloseRequestResult = GeckoResult<Void>()

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })
        val tabsExtension = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/tabs-activate-remove.xpi"))

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })
        var tabsExtensionPB = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/tabs-activate-remove-2.xpi"))

        tabsExtensionPB = sessionRule.waitForResult(
                controller.setAllowedInPrivateBrowsing(tabsExtensionPB, true))


        val newTabSession = sessionRule.createOpenSession(mainSession.settings)

        val newPrivateSession = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder().usePrivateMode(true).build())

        val privateBrowsingNewTabSession = GeckoResult<Void>()

        class TabDelegate(val result: GeckoResult<Void>, val extension: WebExtension,
                          val expectedSession: GeckoSession)
                : WebExtension.SessionTabDelegate {
            override fun onCloseTab(source: WebExtension?,
                                    session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(extension.id, source!!.id)
                assertEquals(expectedSession, session)
                result.complete(null)
                return GeckoResult.allow()
            }
        }

        newTabSession.webExtensionController.setTabDelegate(tabsExtensionPB,
                TabDelegate(privateBrowsingNewTabSession, tabsExtensionPB, newTabSession))

        newTabSession.webExtensionController.setTabDelegate(tabsExtension,
                TabDelegate(onCloseRequestResult, tabsExtension, newTabSession))

        val privateBrowsingPrivateSession = GeckoResult<Void>()

        newPrivateSession.webExtensionController.setTabDelegate(tabsExtensionPB,
                TabDelegate(privateBrowsingPrivateSession, tabsExtensionPB, newPrivateSession))

        // tabsExtension is not allowed in private browsing and shouldn't get this event
        newPrivateSession.webExtensionController.setTabDelegate(tabsExtension,
                object: WebExtension.SessionTabDelegate {
            override fun onCloseTab(source: WebExtension?,
                                    session: GeckoSession): GeckoResult<AllowOrDeny> {
                privateBrowsingPrivateSession.completeExceptionally(
                        RuntimeException("Should never happen"))
                return GeckoResult.allow()
            }
        })

        controller.setTabActive(mainSession, false)
        controller.setTabActive(newPrivateSession, true)

        sessionRule.waitForResult(privateBrowsingPrivateSession)

        controller.setTabActive(newPrivateSession, false)
        controller.setTabActive(newTabSession, true)

        sessionRule.waitForResult(onCloseRequestResult)
        sessionRule.waitForResult(privateBrowsingNewTabSession)

        sessionRule.waitForResult(
                sessionRule.runtime.webExtensionController.uninstall(tabsExtension))
        sessionRule.waitForResult(
                sessionRule.runtime.webExtensionController.uninstall(tabsExtensionPB))

        newTabSession.close()
        newPrivateSession.close()
    }

    // Verifies that the following messages are received from an extension page loaded in the session
    // - HELLO_FROM_PAGE_1 from nativeApp browser1
    // - HELLO_FROM_PAGE_2 from nativeApp browser2
    // - connection request from browser1
    // - HELLO_FROM_PORT from the port opened at the above step
    private fun testExtensionMessages(extension: WebExtension, session: GeckoSession) {
        val messageResult2 = GeckoResult<String>()
        session.webExtensionController.setMessageDelegate(
                extension, object : WebExtension.MessageDelegate {
            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                messageResult2.complete(message as String);
                return null
            }
        }, "browser2")

        val message2 = sessionRule.waitForResult(messageResult2)
        assertThat("Message is received correctly", message2,
                equalTo("HELLO_FROM_PAGE_2"))

        val messageResult1 = GeckoResult<String>()
        val portResult = GeckoResult<WebExtension.Port>()
        session.webExtensionController.setMessageDelegate(
                extension, object : WebExtension.MessageDelegate {
            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                messageResult1.complete(message as String);
                return null
            }

            override fun onConnect(port: WebExtension.Port) {
                portResult.complete(port)
            }
        }, "browser1")

        val message1 = sessionRule.waitForResult(messageResult1)
        assertThat("Message is received correctly", message1,
                equalTo("HELLO_FROM_PAGE_1"))

        val port = sessionRule.waitForResult(portResult)
        val portMessageResult = GeckoResult<String>()
        port.setDelegate(object : WebExtension.PortDelegate {
            override fun onPortMessage(message: Any, port: WebExtension.Port) {
                portMessageResult.complete(message as String)
            }
        })

        val portMessage = sessionRule.waitForResult(portMessageResult)
        assertThat("Message is received correctly", portMessage,
                equalTo("HELLO_FROM_PORT"))
    }

    // This test:
    // - loads an extension that tries to send some messages when loading tab.html
    // - verifies that the messages are received when loading the tab normally
    // - verifies that the messages are received when restoring the tab in a fresh session
    @Test
    fun testRestoringExtensionPagePreservesMessages() {
        // TODO: Bug 1648158
        assumeThat(sessionRule.env.isFission, equalTo(false))

        val extension = sessionRule.waitForResult(
                controller.installBuiltIn(EXTENSION_PAGE_RESTORE))

        mainSession.loadUri("${extension.metaData.baseUrl}tab.html")
        sessionRule.waitForPageStop()

        var savedState : GeckoSession.SessionState? = null
        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count=1)
            override fun onSessionStateChange(session: GeckoSession, state: GeckoSession.SessionState) {
                savedState = state
            }
        })

        // Test that messages are received in the main session
        testExtensionMessages(extension, mainSession)

        val newSession = sessionRule.createOpenSession()
        newSession.restoreState(savedState!!)
        newSession.waitForPageStop()

        // Test that messages are received in a restored state
        testExtensionMessages(extension, newSession)

        sessionRule.waitForResult(controller.uninstall(extension))
    }

    // This test
    // - Create and assign WebExtension TabDelegate to handle closing of tabs
    // - Create new GeckoSession for WebExtension to close
    // - Load url that will allow extension to identify the tab
    // - Registers a WebExtension
    // - Extension finds the tab by url and removes it
    // - TabDelegate handles closing of the tab
    // - Verify that request targets previously created GeckoSession
    @Test
    fun testBrowserTabsRemove() {
        val onCloseRequestResult = GeckoResult<Void>()
        val existingSession = sessionRule.createOpenSession()

        existingSession.loadTestPath("$HELLO_HTML_PATH?tabToClose")
        existingSession.waitForPageStop()

        val tabsExtension = sessionRule.waitForResult(
                controller.installBuiltIn(TABS_REMOVE_BACKGROUND))

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtension.SessionTabDelegate::class,
                { delegate -> existingSession.webExtensionController.setTabDelegate(tabsExtension, delegate) },
                { existingSession.webExtensionController.setTabDelegate(tabsExtension, null) },
                object : WebExtension.SessionTabDelegate {
            override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(existingSession, session)
                onCloseRequestResult.complete(null)
                return GeckoResult.allow()
            }
        })

        sessionRule.waitForResult(onCloseRequestResult)
        sessionRule.waitForResult(controller.uninstall(tabsExtension))
    }

    private fun installWebExtension(background: Boolean,
                                   messageDelegate: WebExtension.MessageDelegate): WebExtension {
        val webExtension: WebExtension

        if (background) {
            webExtension = sessionRule.waitForResult(
                    controller.installBuiltIn(MESSAGING_BACKGROUND))
            webExtension.setMessageDelegate(messageDelegate, "browser")
        } else {
            webExtension = sessionRule.waitForResult(
                    controller.installBuiltIn(MESSAGING_CONTENT))
            mainSession.webExtensionController
                    .setMessageDelegate(webExtension, messageDelegate, "browser")
        }

        return webExtension
    }

    @Test
    fun contentMessaging() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()
        testOnMessage(false)
    }

    @Test
    fun backgroundMessaging() {
        testOnMessage(true)
    }

    // This test
    // - installs a web extension
    // - waits for the web extension to connect to the browser
    // - on connect it will start listening on the port for a message
    // - When the message is received it sends a message in response and waits for another message
    // - When the second message is received it verifies it contains the expected value
    //
    // When `background == true` the test will be run using background messaging, otherwise the
    // test will use content script messaging.
    private fun testPortMessage(background: Boolean) {
        val result = GeckoResult<Void>()
        val prefix = if (background) "testBackground" else "testContent"

        val portDelegate = object: WebExtension.PortDelegate {
            var awaitingResponse = false

            override fun onPortMessage(message: Any, port: WebExtension.Port) {
                assertEquals(port.name, "browser")

                if (!awaitingResponse) {
                    assertThat("We should receive a message from the WebExtension",
                            message as String, equalTo("${prefix}PortMessage"))
                    port.postMessage(JSONObject("{\"message\": \"${prefix}PortMessageResponse\"}"))
                    awaitingResponse = true
                } else {
                    assertThat("The background script should receive our message and respond",
                            message as String, equalTo("response: ${prefix}PortMessageResponse"))
                    result.complete(null)
                }
            }

            override fun onDisconnect(port: WebExtension.Port) {
                // ignored
            }
        }

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onConnect(port: WebExtension.Port) {
                checkSender(port.name, port.sender, background)

                assertEquals(port.name, "browser")

                port.setDelegate(portDelegate)
            }

            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                // Ignored for this test
                return null
            }
        }

        val messaging = installWebExtension(background, messageDelegate)
        sessionRule.waitForResult(result)
        sessionRule.waitForResult(controller.uninstall(messaging))
    }

    @Test
    fun contentPortMessaging() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()
        testPortMessage(false)
    }

    @Test
    fun backgroundPortMessaging() {
        testPortMessage(true)
    }

    // This test
    // - Registers a web extension
    // - Awaits for the web extension to connect to the browser
    // - When connected, it triggers a disconnection from the other side and verifies that
    //   the browser is notified of the port being disconnected.
    //
    // When `background == true` the test will be run using background messaging, otherwise the
    // test will use content script messaging.
    //
    // When `refresh == true` the disconnection will be triggered by refreshing the page, otherwise
    // it will be triggered by sending a message to the web extension.
    private fun testPortDisconnect(background: Boolean, refresh: Boolean) {
        val result = GeckoResult<Void>()

        var messaging: WebExtension? = null
        var messagingPort: WebExtension.Port? = null

        val portDelegate = object: WebExtension.PortDelegate {
            override fun onPortMessage(message: Any,
                                       port: WebExtension.Port) {
                assertEquals(port, messagingPort)
            }

            override fun onDisconnect(port: WebExtension.Port) {
                assertEquals(messaging!!.id, port.sender.webExtension.id)
                assertEquals(port, messagingPort)
                // We successfully received a disconnection
                result.complete(null)
            }
        }

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onConnect(port: WebExtension.Port) {
                assertEquals(messaging!!.id, port.sender.webExtension.id)
                checkSender(port.name, port.sender, background)

                assertEquals(port.name, "browser")
                messagingPort = port
                port.setDelegate(portDelegate)

                if (refresh) {
                    // Refreshing the page should disconnect the port
                    mainSession.reload()
                } else {
                    // Let's ask the web extension to disconnect this port
                    val message = JSONObject()
                    message.put("action", "disconnect")

                    port.postMessage(message)
                }
            }

            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                assertEquals(messaging!!.id, sender.webExtension.id)

                // Ignored for this test
                return null
            }
        }

        messaging = installWebExtension(background, messageDelegate)
        sessionRule.waitForResult(result)
        sessionRule.waitForResult(controller.uninstall(messaging))
    }

    @Test
    fun contentPortDisconnect() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()
        testPortDisconnect(background=false, refresh=false)
    }

    @Test
    fun backgroundPortDisconnect() {
        testPortDisconnect(background=true, refresh=false)
    }

    @Test
    fun contentPortDisconnectAfterRefresh() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()
        testPortDisconnect(background=false, refresh=true)
    }

    fun checkSender(nativeApp: String, sender: WebExtension.MessageSender, background: Boolean) {
        assertEquals("nativeApp should always be 'browser'", nativeApp, "browser")

        if (background) {
            // For background scripts we only want messages from the extension, this should never
            // happen and it's a bug if we get here.
            assertEquals("Called from content script with background-only delegate.",
                    sender.environmentType, WebExtension.MessageSender.ENV_TYPE_EXTENSION)
            assertTrue("Unexpected sender url",
                    sender.url.endsWith("/_generated_background_page.html"))
        } else {
            assertEquals("Called from background script, expecting only content scripts",
                    sender.environmentType, WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT)
            assertTrue("Expecting only top level senders.", sender.isTopLevel)
            assertEquals("Unexpected sender url", sender.url, "https://example.com/")
        }
    }

    // This test
    // - Register a web extension and waits for connections
    // - When connected it disconnects the port from the app side
    // - Awaits for a message from the web extension confirming the web extension was notified of
    //   port being closed.
    //
    // When `background == true` the test will be run using background messaging, otherwise the
    // test will use content script messaging.
    private fun testPortDisconnectFromApp(background: Boolean) {
        val result = GeckoResult<Void>()

        var messaging: WebExtension? = null

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onConnect(port: WebExtension.Port) {
                assertEquals(messaging!!.id, port.sender.webExtension.id)
                checkSender(port.name, port.sender, background)

                port.disconnect()
            }

            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                assertEquals(messaging!!.id, sender.webExtension.id)
                checkSender(nativeApp, sender, background)

                if (message is JSONObject) {
                    if (message.getString("type") == "portDisconnected") {
                        result.complete(null)
                    }
                }

                return null
            }
        }

        messaging = installWebExtension(background, messageDelegate)
        sessionRule.waitForResult(result)
        sessionRule.waitForResult(controller.uninstall(messaging))
    }

    @Test
    fun contentPortDisconnectFromApp() {
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()
        testPortDisconnectFromApp(false)
    }

    @Test
    fun backgroundPortDisconnectFromApp() {
        testPortDisconnectFromApp(true)
    }

    // This test checks that scripts running in a iframe have the `isTopLevel` property set to false.
    private fun testIframeTopLevel() {
        val portTopLevel = GeckoResult<Void>()
        val portIframe = GeckoResult<Void>()
        val messageTopLevel = GeckoResult<Void>()
        val messageIframe = GeckoResult<Void>()

        var messaging: WebExtension? = null

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onConnect(port: WebExtension.Port) {
                assertEquals(messaging!!.id, port.sender.webExtension.id)
                assertEquals(WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT,
                        port.sender.environmentType)
                when (port.sender.url) {
                    "$TEST_ENDPOINT$HELLO_IFRAME_HTML_PATH" -> {
                        assertTrue(port.sender.isTopLevel)
                        portTopLevel.complete(null)
                    }
                    "$TEST_ENDPOINT$HELLO_HTML_PATH" -> {
                        assertFalse(port.sender.isTopLevel)
                        portIframe.complete(null)
                    }
                    else -> // We shouldn't get other messages
                        fail()
                }

                port.disconnect()
            }

            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                assertEquals(messaging!!.id, sender.webExtension.id)
                assertEquals(WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT,
                        sender.environmentType)
                when (sender.url) {
                    "$TEST_ENDPOINT$HELLO_IFRAME_HTML_PATH" -> {
                        assertTrue(sender.isTopLevel)
                        messageTopLevel.complete(null)
                    }
                    "$TEST_ENDPOINT$HELLO_HTML_PATH" -> {
                        assertFalse(sender.isTopLevel)
                        messageIframe.complete(null)
                    }
                    else -> // We shouldn't get other messages
                        fail()
                }

                return null
            }
        }

        messaging = sessionRule.waitForResult(controller.installBuiltIn(
                "resource://android/assets/web_extensions/messaging-iframe/"))
        mainSession.webExtensionController
                .setMessageDelegate(messaging, messageDelegate, "browser")
        sessionRule.waitForResult(portTopLevel)
        sessionRule.waitForResult(portIframe)
        sessionRule.waitForResult(messageTopLevel)
        sessionRule.waitForResult(messageIframe)
        sessionRule.waitForResult(controller.uninstall(messaging))
    }

    @Test
    fun iframeTopLevel() {
        mainSession.loadTestPath(HELLO_IFRAME_HTML_PATH)
        sessionRule.waitForPageStop()
        testIframeTopLevel()
    }

    @Test
    fun redirectToExtensionResource() {
        val result = GeckoResult<String>()
        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                assertEquals(message, "setupReadyStartTest")
                result.complete(null)
                return null
            }
        }

        val extension = sessionRule.waitForResult(controller.installBuiltIn(
                "resource://android/assets/web_extensions/redirect-to-android-resource/"))

        extension.setMessageDelegate(messageDelegate, "browser")
        sessionRule.waitForResult(result)

        // Extension has set up some webRequest listeners to redirect requests.
        // Open the test page and verify that the extension has redirected the
        // scripts as expected.
        mainSession.loadTestPath(TRACKERS_PATH)
        sessionRule.waitForPageStop()

        val textContent = mainSession.evaluateJS("document.body.textContent.replace(/\\s/g, '')")
        assertThat("The extension should have rewritten the script requests and the body",
                textContent as String, equalTo("start,extension-was-here,end"))

        sessionRule.waitForResult(controller.uninstall(extension))
    }

    @Test
    fun loadWebExtensionPage() {
        val result = GeckoResult<String>()
        var extension: WebExtension? = null

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                assertEquals(extension!!.id, sender.webExtension.id)
                assertEquals(WebExtension.MessageSender.ENV_TYPE_EXTENSION,
                        sender.environmentType)
                result.complete(message as String)

                return null
            }
        }

        extension = sessionRule.waitForResult(controller.ensureBuiltIn(
                "resource://android/assets/web_extensions/extension-page-update/",
                "extension-page-update@tests.mozilla.org"))

        val sessionController = mainSession.webExtensionController
        sessionController.setMessageDelegate(extension, messageDelegate, "browser")
        sessionController.setTabDelegate(extension, object: WebExtension.SessionTabDelegate {
            override fun onUpdateTab(extension: WebExtension,
                                     session: GeckoSession,
                                     details: WebExtension.UpdateTabDetails): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        mainSession.loadUri("https://example.com")

        mainSession.waitUntilCalled(object : NavigationDelegate, ProgressDelegate {
            @GeckoSessionTestRule.AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<PermissionDelegate.ContentPermission>) {
                assertThat("Url should load example.com first",
                        url, equalTo("https://example.com/"))
            }

            @GeckoSessionTestRule.AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully.",
                        success, equalTo(true))
            }
        })


        var page: String? = null
        val pageStop = GeckoResult<Boolean>()

        mainSession.delegateUntilTestEnd(object : NavigationDelegate, ProgressDelegate {
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<PermissionDelegate.ContentPermission>) {
                page = url
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                if (success && page != null && page!!.endsWith("/tab.html")) {
                    pageStop.complete(true)
                }
            }
        })

        // If ensureBuiltIn works correctly, this will not re-install the extension.
        // We can verify that it won't reinstall because that would cause the extension page to
        // close prematurely, making the test fail.
        val ensure = sessionRule.waitForResult(controller.ensureBuiltIn(
                "resource://android/assets/web_extensions/extension-page-update/",
                "extension-page-update@tests.mozilla.org"))

        assertThat("ID match", ensure.id, equalTo(extension.id))
        assertThat("version match", ensure.metaData.version, equalTo(extension.metaData.version))

        // Make sure the page loaded successfully
        sessionRule.waitForResult(pageStop)

        assertThat("Url should load WebExtension page", page, endsWith("/tab.html"))

        assertThat("WebExtension page should have access to privileged APIs",
            sessionRule.waitForResult(result), equalTo("HELLO_FROM_PAGE"))

        // Test that after uninstalling an extension, all its pages get closed
        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtension.SessionTabDelegate::class,
                { delegate -> mainSession.webExtensionController.setTabDelegate(extension, delegate) },
                { mainSession.webExtensionController.setTabDelegate(extension, null) },
                object : WebExtension.SessionTabDelegate {})

        val uninstall = controller.uninstall(extension)

        sessionRule.waitUntilCalled(object : WebExtension.SessionTabDelegate {
            @AssertCalled
            override fun onCloseTab(source: WebExtension?,
                                    session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(extension.id, source!!.id)
                assertEquals(mainSession, session)
                return GeckoResult.allow()
            }
        })

        sessionRule.waitForResult(uninstall)
    }

    @Test
    fun badUrl() {
        testInstallBuiltInError("invalid url", "Could not parse uri")
    }

    @Test
    fun badHost() {
        testInstallBuiltInError("resource://gre/", "Only resource://android")
    }

    @Test
    fun dontAllowRemoteUris() {
        testInstallBuiltInError("https://example.com/extension/", "Only resource://android")
    }

    @Test
    fun badFileType() {
        testInstallBuiltInError("resource://android/bad/location/error",
                "does not point to a folder")
    }

    @Test
    fun badLocationXpi() {
        testInstallBuiltInError("resource://android/bad/location/error.xpi",
                "does not point to a folder")
    }

    @Test
    fun testInstallBuiltInError() {
        testInstallBuiltInError("resource://android/bad/location/error/",
                "does not contain a valid manifest")
    }

    private fun testInstallBuiltInError(location: String, expectedError: String) {
        try {
            sessionRule.waitForResult(controller.installBuiltIn(location))
        } catch (ex: Exception) {
            // Let's make sure the error message contains the expected error message
            assertTrue(ex.message!!.contains(expectedError))

            return
        }

        fail("The above code should throw.")
    }

    // Test web extension permission.request.
    @WithDisplay(width = 100, height = 100)
    @Test
    fun permissionRequest() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))

        val extension = sessionRule.waitForResult(
                        controller.ensureBuiltIn("resource://android/assets/web_extensions/permission-request/",
                                                 "permissions@example.com"))

        mainSession.loadUri("${extension.metaData.baseUrl}clickToRequestPermission.html")
        sessionRule.waitForPageStop()

        // click triggers permissions.request
        mainSession.synthesizeTap(50, 50)

        sessionRule.delegateUntilTestEnd(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count = 2)
            override fun onOptionalPrompt(extension: WebExtension, permissions: Array<String>, origins: Array<String>): GeckoResult<AllowOrDeny> {
                val expected = arrayOf("geolocation")
                assertThat("Permissions should match the requested permissions", permissions, equalTo(expected))
                assertThat("Origins should match the requested origins", origins, equalTo(arrayOf("*://example.com/*")))
                return forEachCall(GeckoResult.deny(), GeckoResult.allow())
            }
        })

        var result = GeckoResult<String>()
        mainSession.webExtensionController.setMessageDelegate(
                extension, object : WebExtension.MessageDelegate {
            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                result.complete(message as String)
                return null
            }
        }, "browser")

        val message = sessionRule.waitForResult(result)
        assertThat("Permission request should first be denied.", message, equalTo("false"))

        mainSession.synthesizeTap(50, 50)
        result = GeckoResult<String>()
        val message2 = sessionRule.waitForResult(result)
        assertThat("Permission request should be accepted.", message2, equalTo("true"))

        mainSession.synthesizeTap(50, 50)
        result = GeckoResult<String>()
        val message3 = sessionRule.waitForResult(result)
        assertThat("Permission request should already be accepted.", message3, equalTo("true"))

        sessionRule.waitForResult(controller.uninstall(extension))
    }

    // Test the basic update extension flow with no new permissions.
    @Test
    fun update() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData.version, "1.0")

                return GeckoResult.allow()
            }
        })

        val update1 = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/update-1.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        val update2 = sessionRule.waitForResult(controller.update(update1));
        assertEquals(update2.metaData.version, "2.0")

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that updated extension changed the border color.
        assertBodyBorderEqualTo("blue")

        // Uninstall WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(update2))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being uninstalled
        assertBodyBorderEqualTo("")
    }

    // Test extension updating when the new extension has different permissions.
    @Test
    fun updateWithPerms() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData.version, "1.0")

                return GeckoResult.allow()
            }
        })

        val update1 = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/update-with-perms-1.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onUpdatePrompt(currentlyInstalled: WebExtension,
                                        updatedExtension: WebExtension,
                                        newPermissions: Array<String>,
                                        newOrigins: Array<String>): GeckoResult<AllowOrDeny> {
                assertEquals(currentlyInstalled.metaData.version, "1.0")
                assertEquals(updatedExtension.metaData.version, "2.0")
                assertEquals(newPermissions.size, 1)
                assertEquals(newPermissions[0], "tabs")
                return GeckoResult.allow()
            }
        })

        val update2 = sessionRule.waitForResult(controller.update(update1));
        assertEquals(update2.metaData.version, "2.0")

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that updated extension changed the border color.
        assertBodyBorderEqualTo("blue")

        // Uninstall WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(update2))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being uninstalled
        assertBodyBorderEqualTo("")
    }

    // Ensure update extension works as expected when there is no update available.
    @Test
    fun updateNotAvailable() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData.version, "2.0")

                return GeckoResult.allow()
            }
        })

        val update1 = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/update-2.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("blue")

        val update2 = sessionRule.waitForResult(controller.update(update1))
        assertNull(update2);

        // Uninstall WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(update1))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being uninstalled
        assertBodyBorderEqualTo("")
    }

    // Test denying an extension update.
    @Test
    fun updateDenyPerms() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData.version, "1.0")

                return GeckoResult.allow()
            }
        })

        val update1 = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/update-with-perms-1.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onUpdatePrompt(currentlyInstalled: WebExtension,
                                        updatedExtension: WebExtension,
                                        newPermissions: Array<String>,
                                        newOrigins: Array<String>): GeckoResult<AllowOrDeny> {
                assertEquals(currentlyInstalled.metaData.version, "1.0")
                assertEquals(updatedExtension.metaData.version, "2.0")
                return GeckoResult.deny()
            }
        })


        sessionRule.waitForResult(controller.update(update1).accept({
            // We should not be able to update the extension.
            assertTrue(false)
        }, { exception ->
            assertTrue(exception is WebExtension.InstallException)
            val installException = exception as WebExtension.InstallException
            assertEquals(installException.code, WebExtension.InstallException.ErrorCodes.ERROR_USER_CANCELED)
        }));

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that updated extension changed the border color.
        assertBodyBorderEqualTo("red")

        // Uninstall WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(update1))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being uninstalled
        assertBodyBorderEqualTo("")
    }

    @Test(expected = CancellationException::class)
    fun cancelInstall() {
        val install = controller.install("$TEST_ENDPOINT/stall/test.xpi")
        val cancel = sessionRule.waitForResult(install.cancel())
        assertTrue(cancel)

        sessionRule.waitForResult(install)
    }

    @Test
    fun cancelInstallFailsAfterInstalled() {
        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        var install = controller.install("resource://android/assets/web_extensions/borderify.xpi");
        val borderify = sessionRule.waitForResult(install)

        val cancel = sessionRule.waitForResult(install.cancel())
        assertFalse(cancel)

        sessionRule.waitForResult(controller.uninstall(borderify))
    }

    @Test
    fun updatePostpone() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false,
                "extensions.webextensions.warnings-as-errors" to false
        ))
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData.version, "1.0")
                return GeckoResult.allow()
            }
        })

        val update1 = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/update-postpone-1.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        sessionRule.waitForResult(controller.update(update1).accept({
            // We should not be able to update the extension.
            assertTrue(false)
        }, { exception ->
            assertTrue(exception is WebExtension.InstallException)
            val installException = exception as WebExtension.InstallException
            assertEquals(installException.code, WebExtension.InstallException.ErrorCodes.ERROR_POSTPONED)
        }));

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension is still the first extension.
        assertBodyBorderEqualTo("red")

        sessionRule.waitForResult(controller.uninstall(update1))
    }

    /*
     This function installs a web extension, disables it, updates it and uninstalls it

     @param source: Int - represents a logical type; can be EnableSource.APP or EnableSource.USER
     */
    private fun testUpdatingExtensionDisabledBy(source: Int) {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))
        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        val webExtension = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/update-1.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        val disabledWebExtension = sessionRule.waitForResult(controller.disable(webExtension, source))

        when (source) {
            EnableSource.APP -> checkDisabledState(disabledWebExtension, appDisabled=true)
            EnableSource.USER -> checkDisabledState(disabledWebExtension, userDisabled=true)
        }

        val updatedWebExtension = sessionRule.waitForResult(controller.update(disabledWebExtension))

        mainSession.reload()
        sessionRule.waitForPageStop()

        sessionRule.waitForResult(controller.uninstall(updatedWebExtension))
    }

    @Test
    fun updateDisabledByUser() {
        testUpdatingExtensionDisabledBy(EnableSource.USER)
    }

    @Test
    fun updateDisabledByApp() {
        testUpdatingExtensionDisabledBy(EnableSource.APP)
    }

    // This test
    // - Listen for a newTab request from a web extension
    // - Registers a web extension
    // - Waits for onNewTab request
    // - Verify that request came from right extension
    @Test
    fun testBrowserRuntimeOpenOptionsPageInNewTab() {
        val tabsCreateResult = GeckoResult<Void>()
        var optionsExtension: WebExtension? = null
        val tabDelegate = object : WebExtension.TabDelegate {
            @AssertCalled(count = 1)
            override fun onNewTab(
                    source: WebExtension,
                    details: WebExtension.CreateTabDetails)
                    : GeckoResult<GeckoSession> {
                assertThat(details.url, endsWith("options.html"))
                assertEquals(details.active, true)
                assertEquals(optionsExtension!!.id, source.id)
                tabsCreateResult.complete(null)
                return GeckoResult.fromValue(null)
            }
        }

        optionsExtension = sessionRule.waitForResult(
                controller.installBuiltIn(OPENOPTIONSPAGE_1_BACKGROUND))
        optionsExtension.setTabDelegate(tabDelegate)
        sessionRule.waitForResult(tabsCreateResult)

        sessionRule.waitForResult(controller.uninstall(optionsExtension))
    }

    // This test
    // - Listen for an openOptionsPage request from a web extension
    // - Registers a web extension
    // - Waits for onOpenOptionsPage request
    // - Verify that request came from right extension
    @Test
    fun testBrowserRuntimeOpenOptionsPageDelegate() {
        val openOptionsPageResult = GeckoResult<Void>()
        var optionsExtension: WebExtension? = null
        val tabDelegate = object : WebExtension.TabDelegate {
            @AssertCalled(count = 1)
            override fun onOpenOptionsPage(source: WebExtension) {
                assertThat(
                    source.metaData.optionsPageUrl,
                    endsWith("options.html"))
                assertEquals(optionsExtension!!.id, source.id)
                openOptionsPageResult.complete(null)
            }
        }

        optionsExtension = sessionRule.waitForResult(
                controller.installBuiltIn(OPENOPTIONSPAGE_2_BACKGROUND))
        optionsExtension.setTabDelegate(tabDelegate)
        sessionRule.waitForResult(openOptionsPageResult)

        sessionRule.waitForResult(controller.uninstall(optionsExtension))
    }

    // This test checks if the request from Web Extension is processed correctly in Java
    // the Boolean flags are true, other options have non-default values
    @Test
    fun testDownloadsFlagsTrue() {
        val uri = createTestUrl("/assets/www/images/test.gif")

        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))

        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        val webExtension = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/download-flags-true.xpi"))

        val assertOnDownloadCalled = GeckoResult<WebExtension.Download>()
        val downloadDelegate = object : DownloadDelegate {
            override fun onDownload(source: WebExtension, request: DownloadRequest): GeckoResult<DownloadInitData>? {
                assertEquals(webExtension!!.id, source.id)
                assertEquals(uri, request.request.uri)
                assertEquals("POST", request.request.method)

                request.request.body?.rewind()
                val result = Charset.forName("UTF-8").decode(request.request.body!!).toString()
                assertEquals("postbody", result)

                assertEquals("Mozilla Firefox", request.request.headers.get("User-Agent"))
                assertEquals("banana.gif", request.filename)
                assertTrue(request.allowHttpErrors)
                assertTrue(request.saveAs)
                assertEquals(GeckoWebExecutor.FETCH_FLAGS_PRIVATE, request.downloadFlags)
                assertEquals(DownloadRequest.CONFLICT_ACTION_OVERWRITE, request.conflictActionFlag)

                val download = controller.createDownload(1)
                assertOnDownloadCalled.complete(download)

                val downloadInfo = object: Download.Info {}

                val initialData = DownloadInitData(download, downloadInfo);
                return GeckoResult.fromValue(initialData)
            }
        }

        webExtension.setDownloadDelegate(downloadDelegate)

        mainSession.reload()
        sessionRule.waitForPageStop()

        try {
            sessionRule.waitForResult(assertOnDownloadCalled)
        } catch (exception: UiThreadUtils.TimeoutException) {
            controller.setAllowedInPrivateBrowsing(webExtension, true)
            val downloadCreated = sessionRule.waitForResult(assertOnDownloadCalled)
            assertNotNull(downloadCreated.id)

            sessionRule.waitForResult(controller.uninstall(webExtension))
        }
    }

    // This test checks if the request from Web Extension is processed correctly in Java
    // the Boolean flags are absent/false, other options have default values
    @Test
    fun testDownloadsFlagsFalse() {
        val uri = createTestUrl("/assets/www/images/test.gif")

        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))

        mainSession.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.allow()
            }
        })

        val webExtension = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/download-flags-false.xpi"))

        val assertOnDownloadCalled = GeckoResult<WebExtension.Download>()
        val downloadDelegate = object : DownloadDelegate {
            override fun onDownload(source: WebExtension, request: DownloadRequest): GeckoResult<DownloadInitData>? {
                assertEquals(webExtension!!.id, source.id)
                assertEquals(uri, request.request.uri)
                assertEquals("GET", request.request.method)
                assertNull(request.request.body)
                assertEquals(0, request.request.headers.size)
                assertNull(request.filename)
                assertFalse(request.allowHttpErrors)
                assertFalse(request.saveAs)
                assertEquals(GeckoWebExecutor.FETCH_FLAGS_NONE, request.downloadFlags)
                assertEquals(DownloadRequest.CONFLICT_ACTION_UNIQUIFY, request.conflictActionFlag)

                val download = controller.createDownload(2)
                assertOnDownloadCalled.complete(download)

                val downloadInfo = object: Download.Info {}

                val initialData = DownloadInitData(download, downloadInfo)
                return GeckoResult.fromValue(initialData)
            }
        }

        webExtension.setDownloadDelegate(downloadDelegate)

        mainSession.reload()
        sessionRule.waitForPageStop()

        val downloadCreated = sessionRule.waitForResult(assertOnDownloadCalled)
        assertNotNull(downloadCreated.id)
        sessionRule.waitForResult(controller.uninstall(webExtension))
    }

    @Test
    fun testOnChanged() {
        val uri = createTestUrl("/assets/www/images/test.gif")
        val downloadId = 4
        val unfinishedDownloadSize = 5L
        val finishedDownloadSize = 25L
        val expectedFilename = "test.gif"
        val expectedMime = "image/gif"
        val expectedEndTime = Date().time
        val expectedFilesize = 48L

        // first and second update
        val downloadData = object : Download.Info {
            var endTime : Long? = null
            val startTime = Date().time - 50000
            var fileExists = false
            var totalBytes: Long = -1
            var mime = ""
            var fileSize: Long = -1
            var filename = ""
            var state = Download.STATE_IN_PROGRESS

            override fun state(): Int {
                return state
            }

            override fun endTime(): Long? {
                return endTime
            }

            override fun startTime(): Long {
                return startTime
            }

            override fun fileExists(): Boolean {
                return fileExists;
            }

            override fun totalBytes(): Long {
                return totalBytes
            }

            override fun mime(): String {
                return mime
            }

            override fun fileSize(): Long {
                return fileSize
            }

            override fun filename(): String {
                return filename
            }
        }

        val webExtension = sessionRule.waitForResult(
                controller.installBuiltIn("resource://android/assets/web_extensions/download-onChanged/"))

        val assertOnDownloadCalled = GeckoResult<Download>()
        val downloadDelegate = object : DownloadDelegate {
            override fun onDownload(source: WebExtension, request: DownloadRequest): GeckoResult<WebExtension.DownloadInitData>? {
                assertEquals(webExtension!!.id, source.id)
                assertEquals(uri, request.request.uri)

                val download = controller.createDownload(downloadId)
                assertOnDownloadCalled.complete(download)
                return GeckoResult.fromValue(DownloadInitData(download, downloadData))
            }
        }

        val updates = mutableListOf<JSONObject>()

        val thirdUpdateReceived = GeckoResult<JSONObject>()
        val messageDelegate = object : MessageDelegate {
            override fun onMessage(nativeApp: String, message: Any, sender: MessageSender): GeckoResult<Any>? {
                val current = (message as JSONObject).getJSONObject("current")

                updates.add(message)

                // Once we get the size finished download, that means we got the last update
                if (current.getLong("totalBytes") == finishedDownloadSize) {
                    thirdUpdateReceived.complete(message)
                }

                return GeckoResult.fromValue(message)
            }
        }

        webExtension.setDownloadDelegate(downloadDelegate)
        webExtension.setMessageDelegate(messageDelegate, "browser")

        mainSession.reload()
        sessionRule.waitForPageStop()

        val downloadCreated = sessionRule.waitForResult(assertOnDownloadCalled)
        assertEquals(downloadId, downloadCreated.id)

        // first and second update (they are identical)
        downloadData.filename = expectedFilename
        downloadData.mime = expectedMime
        downloadData.totalBytes = unfinishedDownloadSize

        downloadCreated.update(downloadData)
        downloadCreated.update(downloadData)

        downloadData.fileSize = expectedFilesize
        downloadData.endTime = expectedEndTime
        downloadData.totalBytes = finishedDownloadSize
        downloadData.state = Download.STATE_COMPLETE
        downloadCreated.update(downloadData)

        sessionRule.waitForResult(thirdUpdateReceived)

        // The second update should not be there because the data was identical
        assertEquals(2, updates.size)

        val firstUpdateCurrent = updates[0].getJSONObject("current")
        val firstUpdatePrevious = updates[0].getJSONObject("previous")
        assertEquals(3, firstUpdateCurrent.length())
        assertEquals(3, firstUpdatePrevious.length())
        assertEquals(expectedMime, firstUpdateCurrent.getString("mime"))
        assertEquals("", firstUpdatePrevious.getString("mime"))
        assertEquals(expectedFilename, firstUpdateCurrent.getString("filename"))
        assertEquals("", firstUpdatePrevious.getString("filename"))
        assertEquals(unfinishedDownloadSize, firstUpdateCurrent.getLong("totalBytes"))
        assertEquals(-1, firstUpdatePrevious.getLong("totalBytes"))

        val secondUpdateCurrent = updates[1].getJSONObject("current")
        val secondUpdatePrevious = updates[1].getJSONObject("previous")
        assertEquals(4, secondUpdateCurrent.length())
        assertEquals(4, secondUpdatePrevious.length())
        assertEquals(finishedDownloadSize, secondUpdateCurrent.getLong("totalBytes"))
        assertEquals(firstUpdateCurrent.getLong("totalBytes"), secondUpdatePrevious.getLong("totalBytes"))
        assertEquals("complete", secondUpdateCurrent.get("state").toString())
        assertEquals("in_progress", secondUpdatePrevious.get("state").toString())
        assertEquals(expectedEndTime.toString(), secondUpdateCurrent.getString("endTime"))
        assertEquals("null", secondUpdatePrevious.getString("endTime"))
        assertEquals(expectedFilesize, secondUpdateCurrent.getLong("fileSize"))
        assertEquals(-1, secondUpdatePrevious.getLong("fileSize"))

        sessionRule.waitForResult(controller.uninstall(webExtension))
    }

    @Test
    fun testOnChangedWrongId() {
        val uri = createTestUrl("/assets/www/images/test.gif")
        val downloadId = 5

        val webExtension = sessionRule.waitForResult(
                controller.installBuiltIn("resource://android/assets/web_extensions/download-onChanged/"))

        val assertOnDownloadCalled = GeckoResult<WebExtension.Download>()
        val downloadDelegate = object : DownloadDelegate {
            override fun onDownload(source: WebExtension, request: DownloadRequest): GeckoResult<WebExtension.DownloadInitData>? {
                assertEquals(webExtension!!.id, source.id)
                assertEquals(uri, request.request.uri)

                val download = controller.createDownload(downloadId)
                assertOnDownloadCalled.complete(download)
                return GeckoResult.fromValue(DownloadInitData(download, object : Download.Info {}))
            }
        }

        val onMessageCalled = GeckoResult<String>()
        val messageDelegate = object : MessageDelegate {
            override fun onMessage(nativeApp: String, message: Any, sender: MessageSender): GeckoResult<Any>? {
                onMessageCalled.complete(message as String)
                return GeckoResult.fromValue(message)
            }
        }

        webExtension.setDownloadDelegate(downloadDelegate)
        webExtension.setMessageDelegate(messageDelegate, "browser")

        mainSession.reload()
        sessionRule.waitForPageStop()

        val updateData = object : WebExtension.Download.Info {
            override fun state(): Int {
                return WebExtension.Download.STATE_COMPLETE
            }
        }

        val randomDownload = controller.createDownload(25)

        val r = randomDownload!!.update(updateData)

        try {
            sessionRule.waitForResult(r!!)
        } catch (ex: Exception) {
            val a = ex.message!!
            assertEquals("Error: Trying to update unknown download", a)
            sessionRule.waitForResult(controller.uninstall(webExtension))
            return
        }
    }

}
