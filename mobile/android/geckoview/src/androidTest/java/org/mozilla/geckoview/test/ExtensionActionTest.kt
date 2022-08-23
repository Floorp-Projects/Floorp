package org.mozilla.geckoview.test

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.equalTo
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeThat
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.WebExtension
import org.mozilla.geckoview.Image.ImageProcessingException
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

@MediumTest
@RunWith(Parameterized::class)
class ExtensionActionTest : BaseSessionTest() {
    private var extension: WebExtension? = null
    private var otherExtension: WebExtension? = null
    private var default: WebExtension.Action? = null
    private var backgroundPort: WebExtension.Port? = null
    private var windowPort: WebExtension.Port? = null

    companion object {
        @get:Parameterized.Parameters(name = "{0}")
        @JvmStatic
        val parameters = listOf(
                arrayOf("#pageAction"),
                arrayOf("#browserAction"))
    }

    @field:Parameterized.Parameter(0) @JvmField var id: String = ""

    private val controller
        get() = sessionRule.runtime.webExtensionController

    @Before
    fun setup() {
        controller.setTabActive(mainSession, true)

        // This method installs the extension, opens up ports with the background script and the
        // content script and captures the default action definition from the manifest
        val browserActionDefaultResult = GeckoResult<WebExtension.Action>()
        val pageActionDefaultResult = GeckoResult<WebExtension.Action>()

        val windowPortResult = GeckoResult<WebExtension.Port>()
        val backgroundPortResult = GeckoResult<WebExtension.Port>()

        extension = sessionRule.waitForResult(
                controller.installBuiltIn("resource://android/assets/web_extensions/actions/"));
        // Another dummy extension, only used to check restrictions related to setting
        // another extension url as a popup url, and so there is no delegate needed for it.
        otherExtension = sessionRule.waitForResult(
                controller.installBuiltIn("resource://android/assets/web_extensions/dummy/"));

        mainSession.webExtensionController.setMessageDelegate(
                extension!!,
                object : WebExtension.MessageDelegate {
                    override fun onConnect(port: WebExtension.Port) {
                        windowPortResult.complete(port)
                    }
                }, "browser")
        extension!!.setMessageDelegate(object : WebExtension.MessageDelegate {
            override fun onConnect(port: WebExtension.Port) {
                backgroundPortResult.complete(port)
            }
        }, "browser")

        sessionRule.addExternalDelegateDuringNextWait(
                WebExtension.ActionDelegate::class,
                extension!!::setActionDelegate,
                { extension!!.setActionDelegate(null) },
        object : WebExtension.ActionDelegate {
            override fun onBrowserAction(extension: WebExtension, session: GeckoSession?, action: WebExtension.Action) {
                assertEquals(action.title, "Test action default")
                browserActionDefaultResult.complete(action)
            }
            override fun onPageAction(extension: WebExtension, session: GeckoSession?, action: WebExtension.Action) {
                assertEquals(action.title, "Test action default")
                pageActionDefaultResult.complete(action)
            }
        })

        mainSession.loadUri("http://example.com")
        sessionRule.waitForPageStop()

        val pageAction = sessionRule.waitForResult(pageActionDefaultResult)
        val browserAction = sessionRule.waitForResult(browserActionDefaultResult)

        default = when (id) {
            "#pageAction" -> pageAction
            "#browserAction" -> browserAction
            else -> throw IllegalArgumentException()
        }

        windowPort = sessionRule.waitForResult(windowPortResult)
        backgroundPort = sessionRule.waitForResult(backgroundPortResult)

        if (id == "#pageAction") {
            // Make sure that the pageAction starts enabled for this tab
            testActionApi("""{"action": "enable"}""") { action ->
                assertEquals(action.enabled, true)
            }
        }
    }

    private val type: String
        get() = when(id) {
            "#pageAction" -> "pageAction"
            "#browserAction" -> "browserAction"
            else -> throw IllegalArgumentException()
        }

    @After
    fun tearDown() {
        if (extension != null) {
            extension!!.setMessageDelegate(null, "browser")
            extension!!.setActionDelegate(null)
            sessionRule.waitForResult(controller.uninstall(extension!!))
        }

        if (otherExtension != null) {
            sessionRule.waitForResult(controller.uninstall(otherExtension!!))
        }
    }

    private fun testBackgroundActionApi(message: String, tester: (WebExtension.Action) -> Unit) {
        val result = GeckoResult<Void>()

        val json = JSONObject(message)
        json.put("type", type)

        backgroundPort!!.postMessage(json)

        sessionRule.addExternalDelegateDuringNextWait(
                WebExtension.ActionDelegate::class,
                extension!!::setActionDelegate,
                { extension!!.setActionDelegate(null) },
                object : WebExtension.ActionDelegate {
            override fun onBrowserAction(extension: WebExtension, session: GeckoSession?, action: WebExtension.Action) {
                if (sessionRule.currentCall.counter == 1) {
                    // When attaching the delegate, we will receive a default message, ignore it
                    return
                }
                assertEquals(id, "#browserAction")
                default = action
                tester(action)
                result.complete(null)
            }
            override fun onPageAction(extension: WebExtension, session: GeckoSession?, action: WebExtension.Action) {
                if (sessionRule.currentCall.counter == 1) {
                    // When attaching the delegate, we will receive a default message, ignore it
                    return
                }
                assertEquals(id, "#pageAction")
                default = action
                tester(action)
                result.complete(null)
            }
        })

        sessionRule.waitForResult(result)
    }

    private fun testSetPopup(popupUrl: String, isUrlAllowed: Boolean) {
        val setPopupResult = GeckoResult<Void>()

        backgroundPort!!.setDelegate(object : WebExtension.PortDelegate {
            override fun onPortMessage(message: Any, port: WebExtension.Port) {
                val json = message as JSONObject
                if (json.getString("resultFor") == "setPopup" &&
                    json.getString("type") == type) {
                    if (isUrlAllowed != json.getBoolean("success")) {
                      val expectedResString = when(isUrlAllowed) {
                        true -> "allowed"
                        else -> "disallowed"
                      };
                      setPopupResult.completeExceptionally(IllegalArgumentException(
                              "Expected \"${popupUrl}\" to be ${ expectedResString }"))
                    } else {
                      setPopupResult.complete(null)
                    }
                } else {
                    // We should NOT receive the expected message result.
                    setPopupResult.completeExceptionally(IllegalArgumentException(
                            "Received unexpected result for: ${json.getString("type")} ${json.getString("resultFor")}"))
                }
            }
        })

        var json = JSONObject("""{
           "action": "setPopupCheckRestrictions",
           "popup": "$popupUrl" 
        }""");

        json.put("type", type)
        windowPort!!.postMessage(json)

        sessionRule.waitForResult(setPopupResult)
    }

    private fun testActionApi(message: String, tester: (WebExtension.Action) -> Unit) {
        val result = GeckoResult<Void>()

        val json = JSONObject(message)
        json.put("type", type)

        windowPort!!.postMessage(json)

        sessionRule.addExternalDelegateDuringNextWait(
                WebExtension.ActionDelegate::class,
                { delegate ->
                    mainSession.webExtensionController.setActionDelegate(extension!!, delegate) },
                { mainSession.webExtensionController.setActionDelegate(extension!!, null) },
        object : WebExtension.ActionDelegate {
            override fun onBrowserAction(extension: WebExtension, session: GeckoSession?, action: WebExtension.Action) {
                assertEquals(id, "#browserAction")
                val resolved = action.withDefault(default!!)
                tester(resolved)
                result.complete(null)
            }
            override fun onPageAction(extension: WebExtension, session: GeckoSession?, action: WebExtension.Action) {
                assertEquals(id, "#pageAction")
                val resolved = action.withDefault(default!!)
                tester(resolved)
                result.complete(null)
            }
        })

        sessionRule.waitForResult(result)
    }

    @Test
    fun disableTest() {
        testActionApi("""{"action": "disable"}""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, false)
        }
    }

    @Test
    fun attachingDelegateTriggersDefaultUpdate() {
        val result = GeckoResult<Void>()

        // We should always get a default update after we attach the delegate
        when (id) {
            "#browserAction" -> {
                extension!!.setActionDelegate(object : WebExtension.ActionDelegate {
                    override fun onBrowserAction(extension: WebExtension, session: GeckoSession?,
                                                 action: WebExtension.Action) {
                        assertEquals(action.title, "Test action default")
                        result.complete(null)
                    }
                })
            }
            "#pageAction" -> {
                extension!!.setActionDelegate(object : WebExtension.ActionDelegate {
                    override fun onPageAction(extension: WebExtension, session: GeckoSession?,
                                              action: WebExtension.Action) {
                        assertEquals(action.title, "Test action default")
                        result.complete(null)
                    }
                })
            }
            else -> throw IllegalArgumentException()
        }

        sessionRule.waitForResult(result)
    }

    @Test
    fun enableTest() {
        // First, make sure the action is disabled
        testActionApi("""{"action": "disable"}""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, false)
        }

        testActionApi("""{"action": "enable"}""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, true)
        }
    }

    @Test
    fun setOverridenTitle() {
        testActionApi("""{
               "action": "setTitle",
               "title": "overridden title"
            }""") { action ->
            assertEquals(action.title, "overridden title")
            assertEquals(action.enabled, true)
        }
    }

    @Test
    fun setBadgeText() {
        assumeThat("Only browserAction supports this API.", id, equalTo("#browserAction"))

        testActionApi("""{
           "action": "setBadgeText",
           "text": "12"
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.badgeText, "12")
            assertEquals(action.enabled, true)
        }
    }

    @Test
    fun setBadgeBackgroundColor() {
        assumeThat("Only browserAction supports this API.", id, equalTo("#browserAction"))

        colorTest("setBadgeBackgroundColor", "#ABCDEF", "#FFABCDEF")
        colorTest("setBadgeBackgroundColor", "#F0A", "#FFFF00AA")
        colorTest("setBadgeBackgroundColor", "red", "#FFFF0000")
        colorTest("setBadgeBackgroundColor", "rgb(0, 0, 255)", "#FF0000FF")
        colorTest("setBadgeBackgroundColor", "rgba(0, 255, 0, 0.5)", "#8000FF00")
        colorRawTest("setBadgeBackgroundColor", "[0, 0, 255, 128]", "#800000FF")
    }

    private fun colorTest(actionName: String, color: String, expectedHex: String) {
        colorRawTest(actionName, "\"$color\"", expectedHex)
    }

    private fun colorRawTest(actionName: String, color: String, expectedHex: String) {
        testActionApi("""{
           "action": "$actionName",
           "color": $color
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.badgeText, "")
            assertEquals(action.enabled, true)

            val result = when (actionName) {
                "setBadgeTextColor" -> action.badgeTextColor!!
                "setBadgeBackgroundColor" -> action.badgeBackgroundColor!!
                else -> throw IllegalArgumentException()
            }

            val hexColor = String.format("#%08X", result)
            assertEquals(hexColor, expectedHex)
        }
    }

    @Test
    fun setBadgeTextColor() {
        assumeThat("Only browserAction supports this API.", id, equalTo("#browserAction"))

        colorTest("setBadgeTextColor", "#ABCDEF", "#FFABCDEF")
        colorTest("setBadgeTextColor", "#F0A", "#FFFF00AA")
        colorTest("setBadgeTextColor", "red", "#FFFF0000")
        colorTest("setBadgeTextColor", "rgb(0, 0, 255)", "#FF0000FF")
        colorTest("setBadgeTextColor", "rgba(0, 255, 0, 0.5)", "#8000FF00")
        colorRawTest("setBadgeTextColor", "[0, 0, 255, 128]", "#800000FF")
    }

    @Test
    fun setDefaultTitle() {
        assumeThat("Only browserAction supports default properties.", id, equalTo("#browserAction"))

        // Setting a default value will trigger the default handler on the extension object
        testBackgroundActionApi("""{
            "action": "setTitle",
            "title": "new default title"
        }""") { action ->
            assertEquals(action.title, "new default title")
            assertEquals(action.badgeText, "")
            assertEquals(action.enabled, true)
        }

        // When an overridden title is set, the default has no effect
        testActionApi("""{
           "action": "setTitle",
           "title": "test override"
        }""") { action ->
            assertEquals(action.title, "test override")
            assertEquals(action.badgeText, "")
            assertEquals(action.enabled, true)
        }

        // When the override is null, the new default takes effect
        testActionApi("""{
           "action": "setTitle",
           "title": null
        }""") { action ->
            assertEquals(action.title, "new default title")
            assertEquals(action.badgeText, "")
            assertEquals(action.enabled, true)
        }

        // When the default value is null, the manifest value is used
        testBackgroundActionApi("""{
           "action": "setTitle",
           "title": null
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.badgeText, "")
            assertEquals(action.enabled, true)
        }
    }

    private fun compareBitmap(expectedLocation: String, actual: Bitmap) {
        val stream = InstrumentationRegistry.getInstrumentation().targetContext.assets
                .open(expectedLocation)

        val expected = BitmapFactory.decodeStream(stream)
        for (x in 0 until actual.height) {
            for (y in 0 until actual.width) {
                assertEquals(expected.getPixel(x, y), actual.getPixel(x, y))
            }
        }
    }

    @Test
    fun setIconSvg() {
        val svg = GeckoResult<Void>()

        testActionApi("""{
           "action": "setIcon",
           "path": "button/icon.svg"
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, true)

            action.icon!!.getBitmap(100).accept { actual ->
                compareBitmap("web_extensions/actions/button/expected.png", actual!!)
                svg.complete(null)
            }
        }

        sessionRule.waitForResult(svg)
    }

    @Test
    fun themeIcons() {
        assumeThat("Only browserAction supports this API.", id, equalTo("#browserAction"))

        val png32 = GeckoResult<Void>()

        default!!.icon!!.getBitmap(32).accept ({ actual ->
            compareBitmap("web_extensions/actions/button/beasts-32.png", actual!!)
            png32.complete(null)
        }, { error ->
            png32.completeExceptionally(error!!)
        })

        sessionRule.waitForResult(png32)
    }

    @Test
    fun setIconPng() {
        val png100 = GeckoResult<Void>()
        val png38 = GeckoResult<Void>()
        val png19 = GeckoResult<Void>()
        val png10 = GeckoResult<Void>()

        testActionApi("""{
           "action": "setIcon",
           "path": {
             "19": "button/geo-19.png",
             "38": "button/geo-38.png"
           }
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, true)

            action.icon!!.getBitmap(100).accept { actual ->
                compareBitmap("web_extensions/actions/button/geo-38.png", actual!!)
                png100.complete(null)
            }

            action.icon!!.getBitmap(38).accept { actual ->
                compareBitmap("web_extensions/actions/button/geo-38.png", actual!!)
                png38.complete(null)
            }

            action.icon!!.getBitmap(19).accept { actual ->
                compareBitmap("web_extensions/actions/button/geo-19.png", actual!!)
                png19.complete(null)
            }

            action.icon!!.getBitmap(10).accept { actual ->
                compareBitmap("web_extensions/actions/button/geo-19.png", actual!!)
                png10.complete(null)
            }
        }

        sessionRule.waitForResult(png100)
        sessionRule.waitForResult(png38)
        sessionRule.waitForResult(png19)
        sessionRule.waitForResult(png10)
    }

    @Test
    fun setIconError() {
        val error = GeckoResult<Void>()

        testActionApi("""{
            "action": "setIcon",
            "path": "invalid/path/image.png"
        }""") { action ->
            action.icon!!.getBitmap(38).accept({
                error.completeExceptionally(RuntimeException("Should not succeed."))
            }, { exception ->
                if (!(exception is ImageProcessingException)) {
                    throw exception!!;
                }
                error.complete(null)
            })
        }

        sessionRule.waitForResult(error)
    }

    @Test
    fun testSetPopupRestrictions() {
      testSetPopup("https://example.com", false)
      testSetPopup("${otherExtension!!.metaData.baseUrl}other-extension.html", false)
      testSetPopup("${extension!!.metaData.baseUrl}same-extension.html", true)
      testSetPopup("relative-url-01.html", true);
      testSetPopup("/relative-url-02.html", true);
    }

    @Test
    @GeckoSessionTestRule.WithDisplay(width=100, height=100)
    fun testOpenPopup() {
        // First, let's make sure we have a popup set
        val actionResult = GeckoResult<Void>()
        testActionApi("""{
           "action": "setPopup",
           "popup": "test-popup.html"
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, true)

            actionResult.complete(null)
        }
        sessionRule.waitForResult(actionResult)

        val url = when(id) {
            "#browserAction" -> "test-open-popup-browser-action.html"
            "#pageAction" -> "test-open-popup-page-action.html"
            else -> throw IllegalArgumentException()
        }

        var location = extension!!.metaData.baseUrl;
        mainSession.loadUri("$location$url")
        sessionRule.waitForPageStop()

        val openPopup = GeckoResult<Void>()
        mainSession.webExtensionController.setActionDelegate(extension!!,
                object : WebExtension.ActionDelegate {
            override fun onOpenPopup(extension: WebExtension,
                                     popupAction: WebExtension.Action): GeckoResult<GeckoSession>? {
                assertEquals(extension, this@ExtensionActionTest.extension)
                openPopup.complete(null)
                return null
            }
        })

        // openPopup needs user activation
        mainSession.synthesizeTap(50, 50)

        sessionRule.waitForResult(openPopup)
    }

    @Test
    fun testClickWhenPopupIsNotDefined() {
        val pong = GeckoResult<Void>()

        backgroundPort!!.setDelegate(object : WebExtension.PortDelegate {
            override fun onPortMessage(message: Any, port: WebExtension.Port) {
                val json = message as JSONObject
                if (json.getString("method") == "pong") {
                    pong.complete(null)
                } else {
                    // We should NOT receive onClicked here
                    pong.completeExceptionally(IllegalArgumentException(
                            "Received unexpected: ${json.getString("method")}"))
                }
            }
        })

        val actionResult = GeckoResult<WebExtension.Action>()

        testActionApi("""{
           "action": "setPopup",
           "popup": "test-popup.html"
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, true)

            actionResult.complete(action)
        }

        val togglePopup = GeckoResult<Void>()
        val action = sessionRule.waitForResult(actionResult)

        extension!!.setActionDelegate(object : WebExtension.ActionDelegate {
            override fun onTogglePopup(extension: WebExtension,
                                     popupAction: WebExtension.Action): GeckoResult<GeckoSession>? {
                assertEquals(extension, this@ExtensionActionTest.extension)
                assertEquals(popupAction, action)
                togglePopup.complete(null)
                return null
            }
        })

        // This click() will not cause an onClicked callback because popup is set
        action.click()

        // but it will cause togglePopup to be called
        sessionRule.waitForResult(togglePopup)

        // If the response to ping reaches us before the onClicked we know onClicked wasn't called
        backgroundPort!!.postMessage(JSONObject("""{
            "type": "ping"
        }"""))

        sessionRule.waitForResult(pong)
    }

    @Test
    fun testClickWhenPopupIsDefined() {
        val onClicked = GeckoResult<Void>()
        backgroundPort!!.setDelegate(object : WebExtension.PortDelegate {
            override fun onPortMessage(message: Any, port: WebExtension.Port) {
                val json = message as JSONObject
                assertEquals(json.getString("method"), "onClicked")
                assertEquals(json.getString("type"), type)
                onClicked.complete(null)
            }
        })

        testActionApi("""{
           "action": "setPopup",
           "popup": null
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, true)

            // This click() WILL cause an onClicked callback
            action.click()
        }

        sessionRule.waitForResult(onClicked)
    }

    @Test
    fun testPopupMessaging() {
        val popupSession = sessionRule.createOpenSession()

        val actionResult = GeckoResult<WebExtension.Action>()
        testActionApi("""{
           "action": "setPopup",
           "popup": "test-popup-messaging.html"
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, true)
            actionResult.complete(action)
        }

        val messages = mutableListOf<String>()
        val messageResult = GeckoResult<List<String>>()
        val portResult = GeckoResult<WebExtension.Port>()
        val messageDelegate = object : WebExtension.MessageDelegate {
            override fun onMessage(
                nativeApp: String,
                message: Any,
                sender: WebExtension.MessageSender
            ): GeckoResult<Any>? {
                assertEquals(extension!!.id, sender.webExtension.id)
                assertEquals(WebExtension.MessageSender.ENV_TYPE_EXTENSION,
                    sender.environmentType)
                assertEquals(sender.isTopLevel, true)
                assertEquals("${extension!!.metaData.baseUrl}test-popup-messaging.html",
                    sender.url)
                assertEquals(sender.session, popupSession)
                messages.add(message as String)
                if (messages.size == 2) {
                    messageResult.complete(messages)
                    return null
                } else {
                    return GeckoResult.fromValue("TEST_RESPONSE")
                }
            }

            override fun onConnect(port: WebExtension.Port) {
                assertEquals(extension!!.id, port.sender.webExtension.id)
                assertEquals(WebExtension.MessageSender.ENV_TYPE_EXTENSION,
                    port.sender.environmentType)
                assertEquals(true, port.sender.isTopLevel)
                assertEquals("${extension!!.metaData.baseUrl}test-popup-messaging.html",
                    port.sender.url)
                assertEquals(port.sender.session, popupSession)
                portResult.complete(port)
            }
        }

        popupSession.webExtensionController.setMessageDelegate(
            extension!!, messageDelegate, "browser")

        val action = sessionRule.waitForResult(actionResult)
        extension!!.setActionDelegate(object : WebExtension.ActionDelegate {
            override fun onTogglePopup(extension: WebExtension,
                                       popupAction: WebExtension.Action): GeckoResult<GeckoSession>? {
                assertEquals(extension, this@ExtensionActionTest.extension)
                assertEquals(popupAction, action)
                return GeckoResult.fromValue(popupSession)
            }
        })

        action.click()

        val message = sessionRule.waitForResult(messageResult)
        assertThat("Message should match", message, equalTo(listOf(
            "testPopupMessage", "response: TEST_RESPONSE")))

        val port = sessionRule.waitForResult(portResult)
        val portMessageResult = GeckoResult<String>()

        port.setDelegate(object : WebExtension.PortDelegate {
            override fun onPortMessage(message: Any, p: WebExtension.Port) {
                assertEquals(port, p)
                portMessageResult.complete(message as String)
            }
        })

        val portMessage = sessionRule.waitForResult(portMessageResult)
        assertThat("Message should match", portMessage,
            equalTo("testPopupPortMessage"))
    }

    @Test
    fun testPopupsCanCloseThemselves() {
        val onCloseRequestResult = GeckoResult<Void>()
        val popupSession = sessionRule.createOpenSession()
        popupSession.delegateUntilTestEnd(object : GeckoSession.ContentDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onCloseRequest(session: GeckoSession) {
                onCloseRequestResult.complete(null)
            }
        })

        val actionResult = GeckoResult<WebExtension.Action>()
        testActionApi("""{
           "action": "setPopup",
           "popup": "test-popup.html"
        }""") { action ->
            assertEquals(action.title, "Test action default")
            assertEquals(action.enabled, true)
            actionResult.complete(action)
        }

        val togglePopup = GeckoResult<Void>()
        val action = sessionRule.waitForResult(actionResult)
        extension!!.setActionDelegate(object : WebExtension.ActionDelegate {
            override fun onTogglePopup(extension: WebExtension,
                                     popupAction: WebExtension.Action): GeckoResult<GeckoSession>? {
                assertEquals(extension, this@ExtensionActionTest.extension)
                assertEquals(popupAction, action)
                togglePopup.complete(null)
                return GeckoResult.fromValue(popupSession)
            }
        })
        action.click()
        sessionRule.waitForResult(togglePopup)

        sessionRule.waitForResult(onCloseRequestResult)
    }
}

