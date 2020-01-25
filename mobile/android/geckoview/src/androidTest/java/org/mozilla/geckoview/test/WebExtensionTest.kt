/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.core.StringEndsWith.endsWith
import org.hamcrest.core.IsEqual.equalTo
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.WebExtension.DisabledFlags
import org.mozilla.geckoview.WebExtensionController.EnableSource

import java.util.UUID

@RunWith(AndroidJUnit4::class)
@MediumTest
class WebExtensionTest : BaseSessionTest() {
    companion object {
        private const val TABS_CREATE_BACKGROUND: String =
                "resource://android/assets/web_extensions/tabs-create/"
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
    }

    private val controller
            get() = sessionRule.runtime.webExtensionController

    @Before
    fun setup() {
        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtensionController.PromptDelegate::class,
                controller::setPromptDelegate,
                { controller.promptDelegate = null },
                object : WebExtensionController.PromptDelegate {}
        )
        sessionRule.setPrefsUntilTestEnd(mapOf("extensions.isembedded" to true))
        sessionRule.runtime.webExtensionController.setTabActive(mainSession, true)
    }

    @Test
    fun registerWebExtension() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        val borderify = WebExtension("resource://android/assets/web_extensions/borderify/",
                controller)

        // Load the WebExtension that will add a border to the body
        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(borderify))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        // Unregister WebExtension and check again
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(borderify))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being unregistered
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
                extension.metaData!!.enabled, equalTo(enabled))
        assertThat("userDisabled should match",
                extension.metaData!!.disabledFlags and DisabledFlags.USER > 0,
                equalTo(userDisabled))
        assertThat("appDisabled should match",
                extension.metaData!!.disabledFlags and DisabledFlags.APP > 0,
                equalTo(appDisabled))
        assertThat("blocklistDisabled should match",
                extension.metaData!!.disabledFlags and DisabledFlags.BLOCKLIST > 0,
                equalTo(blocklistDisabled))
    }

    @Test
    fun enableDisable() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
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
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData!!.description,
                        "Adds a red border to all webpages matching example.com.")
                assertEquals(extension.metaData!!.name, "Borderify")
                assertEquals(extension.metaData!!.version, "1.0")
                // TODO: Bug 1601067
                // assertEquals(extension.isBuiltIn, false)
                assertEquals(extension.metaData!!.enabled, false)
                assertEquals(extension.metaData!!.signedState,
                        WebExtension.SignedStateFlags.SIGNED)
                assertEquals(extension.metaData!!.blocklistState,
                        WebExtension.BlocklistStateFlags.NOT_BLOCKED)

                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }
        })

        val borderify = sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/borderify.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        var list = sessionRule.waitForResult(controller.list())
        assertEquals(list.size, 1)
        assertEquals(list[0].id, borderify.id)

        // Unregister WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(borderify))

        list = sessionRule.waitForResult(controller.list())
        assertEquals(list, emptyList<WebExtension>())

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being unregistered
        assertBodyBorderEqualTo("")
    }

    @Test
    fun installMultiple() {
        // dummy.xpi is not signed, but it could be
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false
        ))

        // First, make sure the list starts empty
        var list = sessionRule.waitForResult(controller.list())
        assertEquals(list, emptyList<WebExtension>())

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count=2)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
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
        list = sessionRule.waitForResult(controller.list())
        assertTrue(list.find { it.id == borderify.id } != null)
        assertTrue(list.find { it.id == dummy.id } != null)
        assertEquals(list.size, 2)

        // Uninstall borderify and verify that it's not in the list anymore
        sessionRule.waitForResult(controller.uninstall(borderify))

        list = sessionRule.waitForResult(controller.list())
        assertEquals(list.size, 1)
        assertEquals(list[0].id, dummy.id)

        // Uninstall dummy and make sure the list is now empty
        sessionRule.waitForResult(controller.uninstall(dummy))

        list = sessionRule.waitForResult(controller.list())
        assertEquals(list, emptyList<WebExtension>())
    }

    private fun testInstallError(name: String, expectedError: Int) {
        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count = 0)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
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

    @Test
    fun installUnsignedExtensionSignatureNotRequired() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false
        ))

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }
        })

        val borderify = sessionRule.waitForResult(controller.install(
                "resource://android/assets/web_extensions/borderify-unsigned.xpi")
                .then { extension ->
                    assertEquals(extension!!.metaData!!.signedState,
                            WebExtension.SignedStateFlags.MISSING)
                    assertEquals(extension.metaData!!.blocklistState,
                            WebExtension.BlocklistStateFlags.NOT_BLOCKED)
                    assertEquals(extension.metaData!!.name, "Borderify")
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

        val messaging = createWebExtension(background, messageDelegate)
        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(messaging))
        sessionRule.waitForResult(messageResult)

        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(messaging))
    }

    // This test
    // - Listen for a new tab request from a web extension
    // - Registers a web extension
    // - Waits for onNewTab request
    // - Verify that request came from right extension
    @Test
    fun testBrowserTabsCreate() {
        val tabsCreateResult = GeckoResult<Void>()
        var tabsExtension : WebExtension? = null

        val tabDelegate = object : WebExtensionController.TabDelegate {
            override fun onNewTab(source: WebExtension?, uri: String?): GeckoResult<GeckoSession> {
                assertEquals(uri, "https://www.mozilla.org/en-US/")
                assertEquals(tabsExtension, source)
                tabsCreateResult.complete(null)
                return GeckoResult.fromValue(GeckoSession(sessionRule.session.settings))
            }
        }
        controller.tabDelegate = tabDelegate
        tabsExtension = WebExtension(TABS_CREATE_BACKGROUND, controller)

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(tabsExtension))
        sessionRule.waitForResult(tabsCreateResult)

        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(tabsExtension))
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
        var tabsExtension : WebExtension? = null
        var extensionCreatedSession : GeckoSession? = null

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtensionController.TabDelegate::class,
                controller::setTabDelegate,
                { controller.tabDelegate = null },
                object : WebExtensionController.TabDelegate {
            override fun onNewTab(source: WebExtension?, uri: String?): GeckoResult<GeckoSession> {
                extensionCreatedSession = GeckoSession(sessionRule.session.settings)
                return GeckoResult.fromValue(extensionCreatedSession)
            }

            override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(tabsExtension, source)
                assertNotEquals(null, extensionCreatedSession)
                assertEquals(extensionCreatedSession, session)
                onCloseRequestResult.complete(null)
                return GeckoResult.ALLOW
            }
        })

        tabsExtension = WebExtension(TABS_CREATE_REMOVE_BACKGROUND, controller)

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(tabsExtension))
        sessionRule.waitForResult(onCloseRequestResult)

        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(tabsExtension))
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
    fun testBrowserTabsActivateBrowserTabsRemove() {
        val onCloseRequestResult = GeckoResult<Void>()
        var tabsExtension : WebExtension? = null
        val newTabSession = GeckoSession(sessionRule.session.settings)

        newTabSession.open(sessionRule.runtime)

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtensionController.TabDelegate::class,
                controller::setTabDelegate,
                { controller.tabDelegate = null },
                object : WebExtensionController.TabDelegate {

            override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(tabsExtension, source)
                assertEquals(newTabSession, session)
                onCloseRequestResult.complete(null)
                return GeckoResult.ALLOW
            }
        })

        tabsExtension = WebExtension(TABS_ACTIVATE_REMOVE_BACKGROUND, controller)

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(tabsExtension))

        controller.setTabActive(sessionRule.session, false)
        controller.setTabActive(newTabSession, true)

        sessionRule.waitForResult(onCloseRequestResult)
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(tabsExtension))
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

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtensionController.TabDelegate::class,
                controller::setTabDelegate,
                { controller.tabDelegate = null },
                object : WebExtensionController.TabDelegate {
            override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(existingSession, session)
                onCloseRequestResult.complete(null)
                return GeckoResult.ALLOW
            }
        })

        val tabsExtension = WebExtension(TABS_REMOVE_BACKGROUND, controller)

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(tabsExtension))
        sessionRule.waitForResult(onCloseRequestResult)

        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(tabsExtension))
    }

    private fun createWebExtension(background: Boolean,
                                   messageDelegate: WebExtension.MessageDelegate): WebExtension {
        val webExtension: WebExtension
        val uuid = "{${UUID.randomUUID()}}"

        if (background) {
            webExtension = WebExtension(MESSAGING_BACKGROUND, uuid, WebExtension.Flags.NONE,
                    controller)
            webExtension.setMessageDelegate(messageDelegate, "browser")
        } else {
            webExtension = WebExtension(MESSAGING_CONTENT, uuid,
                    WebExtension.Flags.ALLOW_CONTENT_MESSAGING, controller)
            sessionRule.session.setMessageDelegate(webExtension, messageDelegate, "browser")
        }

        return webExtension
    }

    @Test
    fun contentMessaging() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()
        testOnMessage(false)
    }

    @Test
    fun backgroundMessaging() {
        testOnMessage(true)
    }

    // This test
    // - registers a web extension
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

        val messaging = createWebExtension(background, messageDelegate)

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(messaging))
        sessionRule.waitForResult(result)
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(messaging))
    }

    @Test
    fun contentPortMessaging() {
        mainSession.loadUri("example.com")
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
                assertEquals(messaging, port.sender.webExtension)
                assertEquals(port, messagingPort)
                // We successfully received a disconnection
                result.complete(null)
            }
        }

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onConnect(port: WebExtension.Port) {
                assertEquals(messaging, port.sender.webExtension)
                checkSender(port.name, port.sender, background)

                assertEquals(port.name, "browser")
                messagingPort = port
                port.setDelegate(portDelegate)

                if (refresh) {
                    // Refreshing the page should disconnect the port
                    sessionRule.session.reload()
                } else {
                    // Let's ask the web extension to disconnect this port
                    val message = JSONObject()
                    message.put("action", "disconnect")

                    port.postMessage(message)
                }
            }

            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                assertEquals(messaging, sender.webExtension)

                // Ignored for this test
                return null
            }
        }

        messaging = createWebExtension(background, messageDelegate)

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(messaging))
        sessionRule.waitForResult(result)
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(messaging))
    }

    @Test
    fun contentPortDisconnect() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()
        testPortDisconnect(background=false, refresh=false)
    }

    @Test
    fun backgroundPortDisconnect() {
        testPortDisconnect(background=true, refresh=false)
    }

    @Test
    fun contentPortDisconnectAfterRefresh() {
        mainSession.loadUri("example.com")
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
            assertEquals("Unexpected sender url", sender.url, "http://example.com/")
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
                assertEquals(messaging, port.sender.webExtension)
                checkSender(port.name, port.sender, background)

                port.disconnect()
            }

            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                assertEquals(messaging, sender.webExtension)
                checkSender(nativeApp, sender, background)

                if (message is JSONObject) {
                    if (message.getString("type") == "portDisconnected") {
                        result.complete(null)
                    }
                }

                return null
            }
        }

        messaging = createWebExtension(background, messageDelegate)

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(messaging))
        sessionRule.waitForResult(result)
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(messaging))
    }

    @Test
    fun contentPortDisconnectFromApp() {
        mainSession.loadUri("example.com")
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
                assertEquals(messaging, port.sender.webExtension)
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
                assertEquals(messaging, sender.webExtension)
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

        messaging = WebExtension("resource://android/assets/web_extensions/messaging-iframe/",
                "{${UUID.randomUUID()}}", WebExtension.Flags.ALLOW_CONTENT_MESSAGING, controller)
        sessionRule.session.setMessageDelegate(messaging, messageDelegate, "browser")

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(messaging))
        sessionRule.waitForResult(portTopLevel)
        sessionRule.waitForResult(portIframe)
        sessionRule.waitForResult(messageTopLevel)
        sessionRule.waitForResult(messageIframe)
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(messaging))
    }

    @Test
    fun iframeTopLevel() {
        mainSession.loadTestPath(HELLO_IFRAME_HTML_PATH)
        sessionRule.waitForPageStop()
        testIframeTopLevel()
    }

    @Test
    fun loadWebExtensionPage() {
        val result = GeckoResult<String>()
        var extension: WebExtension? = null

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onMessage(nativeApp: String, message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                assertEquals(extension, sender.webExtension)
                assertEquals(WebExtension.MessageSender.ENV_TYPE_EXTENSION,
                        sender.environmentType)
                result.complete(message as String)

                return null
            }
        }

        extension = WebExtension("resource://android/assets/web_extensions/extension-page-update/",
                controller)

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(extension))
        mainSession.setMessageDelegate(extension, messageDelegate, "browser")

        mainSession.loadUri("http://example.com")

        mainSession.waitUntilCalled(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate {
            @GeckoSessionTestRule.AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?) {
                assertThat("Url should load example.com first",
                        url, equalTo("http://example.com/"))
            }

            @GeckoSessionTestRule.AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Page should load successfully.",
                        success, equalTo(true))
            }
        })


        var page: String? = null
        val pageStop = GeckoResult<Boolean>()

        mainSession.delegateUntilTestEnd(object : Callbacks.NavigationDelegate, Callbacks.ProgressDelegate {
            override fun onLocationChange(session: GeckoSession, url: String?) {
                page = url
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                if (success && page != null && page!!.endsWith("/tab.html")) {
                    pageStop.complete(true)
                }
            }
        })

        // Make sure the page loaded successfully
        sessionRule.waitForResult(pageStop)

        assertThat("Url should load WebExtension page", page, endsWith("/tab.html"))

        assertThat("WebExtension page should have access to privileged APIs",
            sessionRule.waitForResult(result), equalTo("HELLO_FROM_PAGE"))

        // Test that after unregistering an extension, all its pages get closed
        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtensionController.TabDelegate::class,
                controller::setTabDelegate,
                { controller.tabDelegate = null },
                object : WebExtensionController.TabDelegate {})

        val unregister = sessionRule.runtime.unregisterWebExtension(extension)

        sessionRule.waitUntilCalled(object : WebExtensionController.TabDelegate {
            @AssertCalled
            override fun onCloseTab(source: WebExtension?,
                                    session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(null, source)
                assertEquals(mainSession, session)
                return GeckoResult.ALLOW
            }
        })

        sessionRule.waitForResult(unregister)
    }

    @Test
    fun badFileType() {
        testRegisterError("resource://android/bad/location/error",
                "does not point to a folder or an .xpi")
    }

    @Test
    fun badLocationXpi() {
        testRegisterError("resource://android/bad/location/error.xpi",
                "NS_ERROR_FILE_NOT_FOUND")
    }

    @Test
    fun badLocationFolder() {
        testRegisterError("resource://android/bad/location/error/",
                "NS_ERROR_FILE_NOT_FOUND")
    }

    private fun testRegisterError(location: String, expectedError: String) {
        try {
            sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(
                    WebExtension(location, controller)
            ))
        } catch (ex: Exception) {
            // Let's make sure the error message contains the WebExtension URL
            assertTrue(ex.message!!.contains(location))

            // and it contains the expected error message
            assertTrue(ex.message!!.contains(expectedError))

            return
        }

        fail("The above code should throw.")
    }
}
