/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.content.ClipData
import android.os.Build
import android.os.SystemClock
import android.view.DragEvent
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import androidx.test.filters.SdkSuppress
import org.hamcrest.Matchers.equalTo
import org.json.JSONObject
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay

@RunWith(AndroidJUnit4::class)
@SdkSuppress(minSdkVersion = Build.VERSION_CODES.N)
@MediumTest
class DragAndDropTest : BaseSessionTest() {
    // DragEvent has no constructor, so we create it via Java reflection.
    fun createDragEvent(action: Int, x: Float = 0.0F, y: Float = 0.0F): DragEvent {
        val method = DragEvent::class.java.getDeclaredMethod("obtain")
        method.setAccessible(true)
        val dragEvent = method.invoke(null) as DragEvent

        val fieldAction = DragEvent::class.java.getDeclaredField("mAction")
        fieldAction.setAccessible(true)
        fieldAction.set(dragEvent, action)

        if (listOf(DragEvent.ACTION_DRAG_STARTED, DragEvent.ACTION_DRAG_LOCATION, DragEvent.ACTION_DROP).contains(action)) {
            val fieldX = DragEvent::class.java.getDeclaredField("mX")
            fieldX.setAccessible(true)
            fieldX.set(dragEvent, x)

            val fieldY = DragEvent::class.java.getDeclaredField("mY")
            fieldY.setAccessible(true)
            fieldY.set(dragEvent, y)
        }

        val clipData = ClipData.newPlainText("label", "foo")
        if (action == DragEvent.ACTION_DROP) {
            val fieldClipData = DragEvent::class.java.getDeclaredField("mClipData")
            fieldClipData.setAccessible(true)
            fieldClipData.set(dragEvent, clipData)
        }

        if (action != DragEvent.ACTION_DRAG_ENDED) {
            var clipDescription = clipData.getDescription()
            val fieldClipDescription = DragEvent::class.java.getDeclaredField("mClipDescription")
            fieldClipDescription.setAccessible(true)
            fieldClipDescription.set(dragEvent, clipDescription)
        }

        return dragEvent
    }

    fun sendDragEvent(startX: Float, startY: Float, endY: Float) {
        // Android doesn't fire MotionEvent during drag and drop.
        val dragStartEvent = createDragEvent(DragEvent.ACTION_DRAG_STARTED)
        mainSession.panZoomController.onDragEvent(dragStartEvent)
        val dragEnteredEvent = createDragEvent(DragEvent.ACTION_DRAG_ENTERED)
        mainSession.panZoomController.onDragEvent(dragEnteredEvent)
        listOf(startY, endY).forEach {
            val dragLocationEvent = createDragEvent(DragEvent.ACTION_DRAG_LOCATION, startX, it)
            mainSession.panZoomController.onDragEvent(dragLocationEvent)
        }
        val dropEvent = createDragEvent(DragEvent.ACTION_DROP, startX, endY)
        mainSession.panZoomController.onDragEvent(dropEvent)
        val dragEndedEvent = createDragEvent(DragEvent.ACTION_DRAG_ENDED)
        mainSession.panZoomController.onDragEvent(dragEndedEvent)
    }

    @WithDisplay(width = 300, height = 300)
    @Test
    fun dragStartTest() {
        mainSession.loadTestPath(DND_HTML_PATH)
        sessionRule.waitForPageStop()

        val promise = mainSession.evaluatePromiseJS(
            """
            new Promise(r => document.querySelector('#drag').addEventListener('dragstart', r, { once: true }))
            """.trimIndent(),
        )
        val downTime = SystemClock.uptimeMillis()
        mainSession.synthesizeMouse(downTime, MotionEvent.ACTION_DOWN, 50, 20, MotionEvent.BUTTON_PRIMARY)
        for (y in 30..50) {
            mainSession.synthesizeMouse(downTime, MotionEvent.ACTION_MOVE, 50, y, MotionEvent.BUTTON_PRIMARY)
        }
        mainSession.synthesizeMouse(downTime, MotionEvent.ACTION_UP, 50, 50, 0)
        promise.value

        assertThat("drag event is started correctly", true, equalTo(true))
    }

    @WithDisplay(width = 300, height = 300)
    @Test
    fun dropFromExternalTest() {
        mainSession.loadTestPath(DND_HTML_PATH)
        sessionRule.waitForPageStop()

        val promise = mainSession.evaluatePromiseJS(
            """
          new Promise(
              r => document.querySelector('#drop').addEventListener(
                       'drop',
                       e => r(e.dataTransfer.getData('text/plain')),
                       { once: true }))
            """.trimIndent(),
        )

        sendDragEvent(100.0F, 150.0F, 250.0F)

        assertThat("drop event is fired correctly", promise.value as String, equalTo("foo"))
    }

    @WithDisplay(width = 300, height = 500)
    @Test
    fun dropFromExternalToTextControlTest() {
        mainSession.loadTestPath(DND_HTML_PATH)
        sessionRule.waitForPageStop()

        val promiseDragOver = mainSession.evaluatePromiseJS(
            """
          new Promise(
              r => document.querySelector('textarea').addEventListener(
                       'dragover',
                       e => r({ types: e.dataTransfer.types, data: e.dataTransfer.getData('text/plain') }),
                       { once: true }))
            """.trimIndent(),
        )

        val promiseSetValue = mainSession.evaluatePromiseJS(
            """
          new Promise(
              r => document.querySelector('textarea').addEventListener(
                       'input',
                       e => r(document.querySelector('textarea').value),
                       { once: true }))
            """.trimIndent(),
        )

        sendDragEvent(100.0F, 250.0F, 450.0F)

        var value = promiseDragOver.value as JSONObject
        assertThat("dataTransfer type is text/plain", value.getJSONArray("types").getString(0), equalTo("text/plain"))
        assertThat("dataTransfer set empty string during dragover event", value.getString("data"), equalTo(""))
        assertThat("input event is fired correctly", promiseSetValue.value as String, equalTo("foo"))
    }
}
