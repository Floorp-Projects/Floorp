/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.util.concurrent.CancellationException;
import org.hamcrest.core.StringEndsWith.endsWith
import org.hamcrest.core.IsEqual.equalTo
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.WebExtension.DisabledFlags
import org.mozilla.geckoview.WebExtensionController.EnableSource
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.Setting

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
    @Setting.List(Setting(key = Setting.Key.USE_PRIVATE_MODE, value = "true"))
    fun runInPrivateBrowsing() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // Make sure border is empty before running the extension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count=1)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }
        })

        var borderify = sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/borderify.xpi"))

        // Make sure private mode is enabled
        assertTrue(mainSession.settings.usePrivateMode)
        assertFalse(borderify.metaData!!.allowedInPrivateBrowsing)
        // Check that the WebExtension was not applied to a private mode page
        assertBodyBorderEqualTo("")

        borderify = sessionRule.waitForResult(
                controller.setAllowedInPrivateBrowsing(borderify, true))

        assertTrue(borderify.metaData!!.allowedInPrivateBrowsing)
        // Check that the WebExtension was applied to a private mode page now that the extension
        // is enabled in private mode
        mainSession.reload();
        sessionRule.waitForPageStop()
        assertBodyBorderEqualTo("red")

        borderify = sessionRule.waitForResult(
                controller.setAllowedInPrivateBrowsing(borderify, false))

        assertFalse(borderify.metaData!!.allowedInPrivateBrowsing)
        // Check that the WebExtension was not applied to a private mode page after being
        // not allowed to run in private mode
        mainSession.reload();
        sessionRule.waitForPageStop()
        assertBodyBorderEqualTo("")

        // Unregister WebExtension and check again
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
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }
        })

        val dummy = sessionRule.waitForResult(
                controller.install("resource://android/assets/web_extensions/dummy.xpi"))

        val metadata = dummy.metaData!!
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

    @Test
    fun installDeny() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // Ensure border is empty to start.
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled(count = 1)
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.DENY)
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
        val tabsExtension = WebExtension(TABS_CREATE_BACKGROUND, controller)

        val tabDelegate = object : WebExtension.TabDelegate {
            override fun onNewTab(source: WebExtension, details: WebExtension.CreateTabDetails): GeckoResult<GeckoSession> {
                assertEquals(details.url, "https://www.mozilla.org/en-US/")
                assertEquals(details.active, true)
                assertEquals(tabsExtension, source)
                tabsCreateResult.complete(null)
                return GeckoResult.fromValue(null)
            }
        }
        tabsExtension.setTabDelegate(tabDelegate)

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
        val tabsExtension = WebExtension(TABS_CREATE_REMOVE_BACKGROUND, controller)

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtension.TabDelegate::class,
                tabsExtension::setTabDelegate,
                { tabsExtension.setTabDelegate(null) },
                object : WebExtension.TabDelegate {
            override fun onNewTab(source: WebExtension, details: WebExtension.CreateTabDetails): GeckoResult<GeckoSession> {
                val extensionCreatedSession = sessionRule.createClosedSession(sessionRule.session.settings)

                extensionCreatedSession.webExtensionController.setTabDelegate(tabsExtension, object : WebExtension.SessionTabDelegate {
                    override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                        assertEquals(tabsExtension, source)
                        assertEquals(details.active, true)
                        assertNotEquals(null, extensionCreatedSession)
                        assertEquals(extensionCreatedSession, session)
                        onCloseRequestResult.complete(null)
                        return GeckoResult.ALLOW
                    }
                })

                return GeckoResult.fromValue(extensionCreatedSession)
            }
        })


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
    fun testSetTabActive() {
        val onCloseRequestResult = GeckoResult<Void>()
        val tabsExtension = WebExtension(TABS_ACTIVATE_REMOVE_BACKGROUND, controller)
        val newTabSession = sessionRule.createOpenSession(sessionRule.session.settings)

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtension.SessionTabDelegate::class,
                { delegate -> newTabSession.webExtensionController.setTabDelegate(tabsExtension, delegate) },
                { newTabSession.webExtensionController.setTabDelegate(tabsExtension, null) },
                object : WebExtension.SessionTabDelegate {

            override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(tabsExtension, source)
                assertEquals(newTabSession, session)
                onCloseRequestResult.complete(null)
                return GeckoResult.ALLOW
            }
        })

        sessionRule.waitForResult(sessionRule.runtime.registerWebExtension(tabsExtension))

        controller.setTabActive(sessionRule.session, false)
        controller.setTabActive(newTabSession, true)

        sessionRule.waitForResult(onCloseRequestResult)
        sessionRule.waitForResult(sessionRule.runtime.unregisterWebExtension(tabsExtension))
    }

    // Same as testSetTabActive when the extension is not allowed in private browsing
    @Test
    fun testSetTabActiveNotAllowedInPrivateBrowsing() {
        // android.os.Debug.waitForDebugger()
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))

        val onCloseRequestResult = GeckoResult<Void>()

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }
        })
        val tabsExtension = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/tabs-activate-remove.xpi"))

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }
        })
        var tabsExtensionPB = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/tabs-activate-remove-2.xpi"))

        tabsExtensionPB = sessionRule.waitForResult(
                controller.setAllowedInPrivateBrowsing(tabsExtensionPB, true))


        val newTabSession = sessionRule.createOpenSession(sessionRule.session.settings)

        val newPrivateSession = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder().usePrivateMode(true).build())

        val privateBrowsingNewTabSession = GeckoResult<Void>()

        class TabDelegate(val result: GeckoResult<Void>, val extension: WebExtension,
                          val expectedSession: GeckoSession)
                : WebExtension.SessionTabDelegate {
            override fun onCloseTab(source: WebExtension?,
                                    session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(extension, source)
                assertEquals(expectedSession, session)
                result.complete(null)
                return GeckoResult.ALLOW
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
                return GeckoResult.ALLOW
            }
        })

        controller.setTabActive(sessionRule.session, false)
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

        val tabsExtension = WebExtension(TABS_REMOVE_BACKGROUND, controller)

        sessionRule.addExternalDelegateUntilTestEnd(
                WebExtension.SessionTabDelegate::class,
                { delegate -> existingSession.webExtensionController.setTabDelegate(tabsExtension, delegate) },
                { existingSession.webExtensionController.setTabDelegate(tabsExtension, null) },
                object : WebExtension.SessionTabDelegate {
            override fun onCloseTab(source: WebExtension?, session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(existingSession, session)
                onCloseRequestResult.complete(null)
                return GeckoResult.ALLOW
            }
        })

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
            sessionRule.session.webExtensionController
                    .setMessageDelegate(webExtension, messageDelegate, "browser")
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
        sessionRule.session.webExtensionController
                .setMessageDelegate(messaging, messageDelegate, "browser")

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

    @Ignore // Broken with browser.tabs.documentchannel == true
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
        val sessionController = mainSession.webExtensionController
        sessionController.setMessageDelegate(extension, messageDelegate, "browser")
        sessionController.setTabDelegate(extension, object: WebExtension.SessionTabDelegate {
            override fun onUpdateTab(extension: WebExtension,
                                     session: GeckoSession,
                                     details: WebExtension.UpdateTabDetails): GeckoResult<AllowOrDeny> {
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }
        })

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
                WebExtension.SessionTabDelegate::class,
                { delegate -> mainSession.webExtensionController.setTabDelegate(extension, delegate) },
                { mainSession.webExtensionController.setTabDelegate(extension, null) },
                object : WebExtension.SessionTabDelegate {})

        val unregister = sessionRule.runtime.unregisterWebExtension(extension)

        sessionRule.waitUntilCalled(object : WebExtension.SessionTabDelegate {
            @AssertCalled
            override fun onCloseTab(source: WebExtension?,
                                    session: GeckoSession): GeckoResult<AllowOrDeny> {
                assertEquals(extension, source)
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

    // Test the basic update extension flow with no new permissions.
    @Test
    fun update() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "xpinstall.signatures.required" to false,
                "extensions.install.requireBuiltInCerts" to false,
                "extensions.update.requireBuiltInCerts" to false
        ))
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData!!.version, "1.0")

                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
            }
        })

        val update1 = sessionRule.waitForResult(
                controller.install("https://example.org/tests/junit/update-1.xpi"))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was applied by checking the border color
        assertBodyBorderEqualTo("red")

        val update2 = sessionRule.waitForResult(controller.update(update1));
        assertEquals(update2.metaData!!.version, "2.0")

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that updated extension changed the border color.
        assertBodyBorderEqualTo("blue")

        // Unregister WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(update2))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being unregistered
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
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData!!.version, "1.0")

                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
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
                assertEquals(currentlyInstalled.metaData!!.version, "1.0")
                assertEquals(updatedExtension.metaData!!.version, "2.0")
                assertEquals(newPermissions.size, 1)
                assertEquals(newPermissions[0], "tabs")
                return GeckoResult.fromValue(AllowOrDeny.ALLOW);
            }
        })

        val update2 = sessionRule.waitForResult(controller.update(update1));
        assertEquals(update2.metaData!!.version, "2.0")

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that updated extension changed the border color.
        assertBodyBorderEqualTo("blue")

        // Unregister WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(update2))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being unregistered
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
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData!!.version, "2.0")

                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
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

        // Unregister WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(update1))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being unregistered
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
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData!!.version, "1.0")

                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
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
                assertEquals(currentlyInstalled.metaData!!.version, "1.0")
                assertEquals(updatedExtension.metaData!!.version, "2.0")
                return GeckoResult.fromValue(AllowOrDeny.DENY);
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

        // Unregister WebExtension and check again
        sessionRule.waitForResult(controller.uninstall(update1))

        mainSession.reload()
        sessionRule.waitForPageStop()

        // Check that the WebExtension was not applied after being unregistered
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
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
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
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        // First let's check that the color of the border is empty before loading
        // the WebExtension
        assertBodyBorderEqualTo("")

        sessionRule.delegateDuringNextWait(object : WebExtensionController.PromptDelegate {
            @AssertCalled
            override fun onInstallPrompt(extension: WebExtension): GeckoResult<AllowOrDeny> {
                assertEquals(extension.metaData!!.version, "1.0")
                return GeckoResult.fromValue(AllowOrDeny.ALLOW)
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
}
