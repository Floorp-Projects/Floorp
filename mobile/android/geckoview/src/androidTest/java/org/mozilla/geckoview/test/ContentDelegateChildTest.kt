/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.os.SystemClock
import android.view.*
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement

import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Assert.assertNull
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.*

@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentDelegateChildTest : BaseSessionTest() {

    private fun sendLongPress(x: Float, y: Float){
        val downTime = SystemClock.uptimeMillis()
        var eventTime = SystemClock.uptimeMillis()
        var event = MotionEvent.obtain(
                                    downTime,
                                    eventTime,
                                    MotionEvent.ACTION_DOWN,
                                    x,
                                    y, 0);
        mainSession.panZoomController.onTouchEvent(event)
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun requestContextMenuOnAudio() {
        mainSession.loadTestPath(CONTEXT_MENU_AUDIO_HTML_PATH)
        mainSession.waitForPageStop()
        sendLongPress(0f, 0f)

        mainSession.waitUntilCalled(object: ContentDelegate {

            @AssertCalled(count = 1)
            override fun onContextMenu(session: GeckoSession, 
                                       screenX: Int, 
                                       screenY: Int, 
                                       element: ContextElement) {
                assertThat(
                            "Type should be audio.",
                            element.type,
                            equalTo(ContextElement.TYPE_AUDIO))
                assertThat(
                            "The element source should be the mp3 file.",
                            element.srcUri,
                            endsWith("owl.mp3"))
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun requestContextMenuOnBlobBuffered() {
        mainSession.loadTestPath(CONTEXT_MENU_BLOB_BUFFERED_HTML_PATH)
        mainSession.waitForPageStop()
        mainSession.waitForRoundTrip()
        sendLongPress(50f, 50f)

        mainSession.waitUntilCalled(object: ContentDelegate {

            @AssertCalled(count = 1)
            override fun onContextMenu(session: GeckoSession, 
                                       screenX: Int, 
                                       screenY: Int, 
                                       element: ContextElement) {
                assertThat(
                            "Type should be video.",
                            element.type,
                            equalTo(ContextElement.TYPE_VIDEO))
                assertNull(
                            "Buffered blob should not have a srcUri.",
                            element.srcUri)
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun requestContextMenuOnBlobFull() {
        mainSession.loadTestPath(CONTEXT_MENU_BLOB_FULL_HTML_PATH)
        mainSession.waitForPageStop()
        mainSession.waitForRoundTrip()
        sendLongPress(50f, 50f)

        mainSession.waitUntilCalled(object: ContentDelegate {

            @AssertCalled(count = 1)
            override fun onContextMenu(session: GeckoSession, 
                                       screenX: Int, 
                                       screenY: Int, 
                                       element: ContextElement) {

                assertThat(
                            "Type should be image.",
                            element.type,
                            equalTo(ContextElement.TYPE_IMAGE))
                assertThat(
                            "Alternate text should match.",
                            element.altText, 
                            equalTo("An orange circle."))
                assertThat(
                            "The element source should begin with blob.",
                            element.srcUri,
                            startsWith("blob:"))
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun requestContextMenuOnImageNested() {
        mainSession.loadTestPath(CONTEXT_MENU_IMAGE_NESTED_HTML_PATH)
        mainSession.waitForPageStop()
        sendLongPress(50f, 50f)

        mainSession.waitUntilCalled(object: ContentDelegate {

            @AssertCalled(count = 1)
            override fun onContextMenu(session: GeckoSession, 
                                       screenX: Int, 
                                       screenY: Int, 
                                       element: ContextElement) {

                assertThat(
                            "Type should be image.",
                            element.type,
                            equalTo(ContextElement.TYPE_IMAGE))
                assertThat(
                            "Alternate text should match.",
                            element.altText, 
                            equalTo("Test Image"))
                assertThat(
                            "The element source should be the image file.",
                            element.srcUri, 
                            endsWith("test.gif"))
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun requestContextMenuOnImage() {
        mainSession.loadTestPath(CONTEXT_MENU_IMAGE_HTML_PATH)
        mainSession.waitForPageStop()
        sendLongPress(50f, 50f)

        mainSession.waitUntilCalled(object: ContentDelegate {

            @AssertCalled(count = 1)
            override fun onContextMenu(session: GeckoSession, 
                                       screenX: Int, 
                                       screenY: Int, 
                                       element: ContextElement) {
                assertThat(
                            "Type should be image.",
                            element.type,
                            equalTo(ContextElement.TYPE_IMAGE))
                assertThat(
                            "Alternate text should match.",
                            element.altText ,
                            equalTo("Test Image"))
                assertThat(
                            "The element source should be the image file.",
                            element.srcUri ,
                            endsWith("test.gif"))
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun requestContextMenuOnLink() {
        mainSession.loadTestPath(CONTEXT_MENU_LINK_HTML_PATH)
        mainSession.waitForPageStop()
        sendLongPress(50f, 50f)

        mainSession.waitUntilCalled(object: ContentDelegate {

            @AssertCalled(count = 1)
            override fun onContextMenu(session: GeckoSession, 
                                       screenX: Int, 
                                       screenY: Int, 
                                       element: ContextElement) {
                assertThat(
                            "Type should be none.",
                            element.type,
                            equalTo(ContextElement.TYPE_NONE))          
                assertThat(
                            "The element link title should be the title of the anchor.",
                            element.title,
                            equalTo("Hello Link Title"))
                assertThat(
                            "The element link URI should be the href of the anchor.",
                            element.linkUri,
                            endsWith("hello.html"))
            }
        })
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun requestContextMenuOnVideo() {
        // Bug 1700243
        assumeThat(sessionRule.env.isIsolatedProcess, equalTo(false))
        mainSession.loadTestPath(CONTEXT_MENU_VIDEO_HTML_PATH)
        mainSession.waitForPageStop()
        sendLongPress(50f, 50f)

        mainSession.waitUntilCalled(object: ContentDelegate {

            @AssertCalled(count = 1)
            override fun onContextMenu(session: GeckoSession, 
                                       screenX: Int, 
                                       screenY: Int, 
                                       element: ContextElement) {
                assertThat(
                            "Type should be video.",
                            element.type,
                            equalTo(ContextElement.TYPE_VIDEO))
                assertThat(
                            "The element source should be the video file.",
                            element.srcUri, 
                            endsWith("short.mp4"))
            }
        })
    }
}
