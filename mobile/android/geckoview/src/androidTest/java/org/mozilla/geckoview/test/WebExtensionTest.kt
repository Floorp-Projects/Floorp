/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.support.test.InstrumentationRegistry
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.core.StringEndsWith.endsWith
import org.hamcrest.core.IsEqual.equalTo
import org.json.JSONObject
import org.junit.Assert
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.test.util.HttpBin
import java.net.URI

import java.util.UUID

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
class WebExtensionTest : BaseSessionTest() {
    companion object {
        val TEST_ENDPOINT: String = "http://localhost:4243"
        val MESSAGING_BACKGROUND: String = "resource://android/assets/web_extensions/messaging/"
        val MESSAGING_CONTENT: String = "resource://android/assets/web_extensions/messaging-content/"
    }

    @Test
    @WithDevToolsAPI
    fun registerWebExtension() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        val colorBefore = sessionRule.evaluateJS(mainSession, "document.body.style.borderColor")
        assertThat("The border color should be empty when loading without extensions.",
                colorBefore as String, equalTo(""))

        val borderify = WebExtension("resource://android/assets/web_extensions/borderify/")

        // Load the WebExtension that will add a border to the body
        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(borderify))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        val color = sessionRule.evaluateJS(mainSession, "document.body.style.borderColor")
        assertThat("Content script should have been applied",
                color as String, equalTo("red"))

        // Unregister WebExtension and check again
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(borderify))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being unregistered
        val colorAfter = sessionRule.evaluateJS(mainSession, "document.body.style.borderColor")
        assertThat("Content script should have been applied",
                colorAfter as String, equalTo(""))
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

            override fun onMessage(message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                checkSender(sender, background)

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

    private fun createWebExtension(background: Boolean,
                                   messageDelegate: WebExtension.MessageDelegate): WebExtension {
        val webExtension: WebExtension
        val uuid = "{${UUID.randomUUID()}}"

        if (background) {
            webExtension = WebExtension(MESSAGING_BACKGROUND, uuid, WebExtension.Flags.NONE)
            webExtension.setMessageDelegate(messageDelegate, "browser")
        } else {
            webExtension = WebExtension(MESSAGING_CONTENT, uuid,
                    WebExtension.Flags.ALLOW_CONTENT_MESSAGING)
            sessionRule.session.setMessageDelegate(messageDelegate, "browser");
        }

        return webExtension
    }

    @Test
    @WithDevToolsAPI
    fun contentMessaging() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()
        testOnMessage(false)
    }

    @Test
    @WithDevToolsAPI
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
        var contentPort: WebExtension.Port? = null
        val result = GeckoResult<Void>()
        val prefix = if (background) "testBackground" else "testContent"

        val portDelegate = object: WebExtension.PortDelegate {
            var awaitingResponse = false

            override fun onPortMessage(message: Any, port: WebExtension.Port) {
                Assert.assertEquals(port.name, "browser")

                if (!awaitingResponse) {
                    assertThat("We should receive a message from the WebExtension",
                            message as String, equalTo("${prefix}PortMessage"))
                    contentPort!!.postMessage(JSONObject("{\"message\": \"${prefix}PortMessageResponse\"}"))
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
                checkSender(port.sender, background)

                Assert.assertEquals(port.name, "browser")

                port.setDelegate(portDelegate)
                contentPort = port
            }

            override fun onMessage(message: Any,
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
    @WithDevToolsAPI
    fun contentPortMessaging() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()
        testPortMessage(false)
    }

    @Test
    @WithDevToolsAPI
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
                Assert.assertEquals(port, messagingPort)
            }

            override fun onDisconnect(port: WebExtension.Port) {
                Assert.assertEquals(messaging, port.sender.webExtension)
                Assert.assertEquals(port, messagingPort)
                // We successfully received a disconnection
                result.complete(null)
            }
        }

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onConnect(port: WebExtension.Port) {
                Assert.assertEquals(messaging, port.sender.webExtension)
                checkSender(port.sender, background)

                Assert.assertEquals(port.name, "browser")
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

            override fun onMessage(message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                Assert.assertEquals(messaging, sender.webExtension)

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
    @WithDevToolsAPI
    fun contentPortDisconnect() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()
        testPortDisconnect(false, false)
    }

    @Test
    fun backgroundPortDisconnect() {
        testPortDisconnect(true, false)
    }

    @Test
    @WithDevToolsAPI
    fun contentPortDisconnectAfterRefresh() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()
        testPortDisconnect(false, true)
    }

    fun checkSender(sender: WebExtension.MessageSender, background: Boolean) {
        if (background) {
            // For background scripts we only want messages from the extension, this should never
            // happen and it's a bug if we get here.
            Assert.assertEquals("Called from content script with background-only delegate.",
                    sender.environmentType, WebExtension.MessageSender.ENV_TYPE_EXTENSION)
            Assert.assertTrue("Unexpected sender url",
                    sender.url.endsWith("/_generated_background_page.html"))
        } else {
            Assert.assertEquals("Called from background script, expecting only content scripts",
                    sender.environmentType, WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT)
            Assert.assertTrue("Expecting only top level senders.", sender.isTopLevel)
            Assert.assertEquals("Unexpected sender url", sender.url, "http://example.com/")
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
                Assert.assertEquals(messaging, port.sender.webExtension)
                checkSender(port.sender, background)

                port.disconnect()
            }

            override fun onMessage(message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                Assert.assertEquals(messaging, sender.webExtension)
                checkSender(sender, background)

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
    @WithDevToolsAPI
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
                Assert.assertEquals(messaging, port.sender.webExtension)
                Assert.assertEquals(WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT,
                        port.sender.environmentType)
                if (port.sender.url == "$TEST_ENDPOINT$HELLO_IFRAME_HTML_PATH") {
                    Assert.assertTrue(port.sender.isTopLevel)
                    portTopLevel.complete(null)
                } else if (port.sender.url == "$TEST_ENDPOINT$HELLO_HTML_PATH") {
                    Assert.assertFalse(port.sender.isTopLevel)
                    portIframe.complete(null)
                } else {
                    // We shouldn't get other messages
                    fail()
                }

                port.disconnect()
            }

            override fun onMessage(message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                Assert.assertEquals(messaging, sender.webExtension)
                Assert.assertEquals(WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT,
                        sender.environmentType)
                if (sender.url == "$TEST_ENDPOINT$HELLO_IFRAME_HTML_PATH") {
                    Assert.assertTrue(sender.isTopLevel)
                    messageTopLevel.complete(null)
                } else if (sender.url == "$TEST_ENDPOINT$HELLO_HTML_PATH") {
                    Assert.assertFalse(sender.isTopLevel)
                    messageIframe.complete(null)
                } else {
                    // We shouldn't get other messages
                    fail()
                }

                return null
            }
        }

        messaging = WebExtension("resource://android/assets/web_extensions/messaging-iframe/",
                "{${UUID.randomUUID()}}", WebExtension.Flags.ALLOW_CONTENT_MESSAGING)
        sessionRule.session.setMessageDelegate(messageDelegate, "browser");

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(messaging))
        sessionRule.waitForResult(portTopLevel)
        sessionRule.waitForResult(portIframe)
        sessionRule.waitForResult(messageTopLevel)
        sessionRule.waitForResult(messageIframe)
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(messaging))
    }

    @Test
    @WithDevToolsAPI
    fun iframeTopLevel() {
        val httpBin = HttpBin(InstrumentationRegistry.getTargetContext(), URI.create(TEST_ENDPOINT))

        try {
            httpBin.start()

            mainSession.loadUri("$TEST_ENDPOINT$HELLO_IFRAME_HTML_PATH")
            sessionRule.waitForPageStop()
            testIframeTopLevel()
        } finally {
            httpBin.stop()
        }
    }

    @Test
    fun loadWebExtensionPage() {
        val result = GeckoResult<String>()
        var extension: WebExtension? = null;

        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onMessage(message: Any,
                                   sender: WebExtension.MessageSender): GeckoResult<Any>? {
                Assert.assertEquals(extension, sender.webExtension)
                Assert.assertEquals(WebExtension.MessageSender.ENV_TYPE_EXTENSION,
                        sender.environmentType)
                result.complete(message as String)

                return null
            }
        }

        extension = WebExtension("resource://android/assets/web_extensions/extension-page-update/")

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(extension))
        mainSession.setMessageDelegate(messageDelegate, "browser")

        mainSession.loadUri("http://example.com");

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
        var pageStop = GeckoResult<Boolean>()

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

        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(extension))
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
                    WebExtension(location)
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
