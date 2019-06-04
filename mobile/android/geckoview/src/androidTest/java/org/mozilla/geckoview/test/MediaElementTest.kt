/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MediaElement
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.TimeoutMillis
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Assume.assumeTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule

@RunWith(AndroidJUnit4::class)
@TimeoutMillis(45000)
@MediumTest
class MediaElementTest : BaseSessionTest() {

    interface MediaElementDelegate : MediaElement.Delegate {
        override fun onPlaybackStateChange(mediaElement: MediaElement, mediaState: Int) {}
        override fun onReadyStateChange(mediaElement: MediaElement, readyState: Int) {}
        override fun onMetadataChange(mediaElement: MediaElement, metaData: MediaElement.Metadata) {}
        override fun onLoadProgress(mediaElement: MediaElement, progressInfo: MediaElement.LoadProgressInfo) {}
        override fun onVolumeChange(mediaElement: MediaElement, volume: Double, muted: Boolean) {}
        override fun onTimeChange(mediaElement: MediaElement, time: Double) {}
        override fun onPlaybackRateChange(mediaElement: MediaElement, rate: Double) {}
        override fun onFullscreenChange(mediaElement: MediaElement, fullscreen: Boolean) {}
        override fun onError(mediaElement: MediaElement, errorCode: Int) {}
    }

    private fun setupPrefs() {

        sessionRule.setPrefsUntilTestEnd(mapOf(
                "media.autoplay.default" to 0,
                "full-screen-api.allow-trusted-requests-only" to false))

    }

    private fun setupDelegate(path: String) {
        sessionRule.session.loadTestPath(path)
        sessionRule.waitUntilCalled(object : Callbacks.MediaDelegate {
            @AssertCalled
            override fun onMediaAdd(session: GeckoSession, element: MediaElement) {
                sessionRule.addExternalDelegateUntilTestEnd(
                        MediaElementDelegate::class,
                        element::setDelegate,
                        { element.delegate = null },
                        object : MediaElementDelegate {})
            }
        })
    }

    private fun setupPrefsAndDelegates(path: String) {
        setupPrefs()
        setupDelegate(path)
    }

    private fun waitUntilState(waitState: Int = MediaElement.MEDIA_READY_STATE_HAVE_ENOUGH_DATA): MediaElement {
        var ready = false
        var result: MediaElement? = null
        while (!ready) {
            sessionRule.waitUntilCalled(object : MediaElementDelegate {
                @AssertCalled
                override fun onReadyStateChange(mediaElement: MediaElement, readyState: Int) {
                    if (readyState == waitState) {
                        ready = true
                        result = mediaElement
                    }
                }
            })
        }
        if (result == null) {
            throw IllegalStateException("No MediaElement Found")
        }
        return result!!
    }

    private fun waitUntilVideoReady(path: String, waitState: Int = MediaElement.MEDIA_READY_STATE_HAVE_ENOUGH_DATA): MediaElement {
        setupPrefsAndDelegates(path)
        return waitUntilState(waitState)
    }

    private fun waitUntilVideoReadyNoPrefs(path: String, waitState: Int = MediaElement.MEDIA_READY_STATE_HAVE_ENOUGH_DATA): MediaElement {
        setupDelegate(path)
        return waitUntilState(waitState)
    }

    private fun waitForPlaybackStateChange(waitState: Int, lambda: (element: MediaElement, state: Int) -> Unit = { _: MediaElement, _: Int -> }) {
        var waiting = true
        while (waiting) {
            sessionRule.waitUntilCalled(object : MediaElementDelegate {
                @AssertCalled
                override fun onPlaybackStateChange(mediaElement: MediaElement, mediaState: Int) {
                    if (mediaState == waitState) {
                        waiting = false
                        lambda(mediaElement, mediaState)
                    }
                }
            })
        }
    }

    private fun waitForMetadata(path: String): MediaElement.Metadata? {
        setupPrefsAndDelegates(path)
        var meta: MediaElement.Metadata? = null
        while (meta == null) {
            sessionRule.waitUntilCalled(object : MediaElementDelegate {
                @AssertCalled
                override fun onMetadataChange(mediaElement: MediaElement, metaData: MediaElement.Metadata) {
                    meta = metaData
                }
            })
        }
        return meta
    }

    private fun playMedia(path: String) {
        val mediaElement = waitUntilVideoReady(path)
        mediaElement.play()
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAY)
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAYING)
    }

    private fun playMediaFromScript(path: String) {
        waitUntilVideoReady(path)
        sessionRule.evaluateJS(mainSession, "$('video').play()")
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAY)
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAYING)
    }

    private fun pauseMedia(path: String) {
        val mediaElement = waitUntilVideoReady(path)
        mediaElement.play()
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAYING) { element: MediaElement, _: Int ->
            element.pause()
        }
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PAUSE)
    }

    private fun timeMedia(path: String, limit: Double) {
        val mediaElement = waitUntilVideoReady(path)
        mediaElement.play()
        var waiting = true
        while (waiting) {
            sessionRule.waitUntilCalled(object : MediaElementDelegate {
                @AssertCalled
                override fun onTimeChange(mediaElement: MediaElement, time: Double) {
                    if (time > limit) {
                        waiting = false
                    }
                }
            })
        }
    }

    private fun seekMedia(path: String, seek: Double) {
        val media = waitUntilVideoReady(path)
        media.seek(seek)
        var waiting = true
        // Sometimes we get a MediaElement.MEDIA_STATE_SUSPEND state change. So just wait until
        // the test receives the SEEKING state change or time out.
        while (waiting) {
            sessionRule.waitUntilCalled(object : MediaElementDelegate {
                @AssertCalled
                override fun onPlaybackStateChange(mediaElement: MediaElement, mediaState: Int) {
                    if (mediaState == MediaElement.MEDIA_STATE_SEEKING) {
                        waiting = false
                    }
                }
            })
        }
        waiting = true
        while (waiting) {
            sessionRule.waitUntilCalled(object : MediaElementDelegate {
                @AssertCalled
                override fun onTimeChange(mediaElement: MediaElement, time: Double) {
                    if (time >= seek) {
                        waiting = false
                    }
                }
            })
        }
        sessionRule.waitUntilCalled(object : MediaElementDelegate {
            @AssertCalled
            override fun onPlaybackStateChange(mediaElement: MediaElement, mediaState: Int) {
                assertThat("Done seeking", mediaState, equalTo(MediaElement.MEDIA_STATE_SEEKED))
            }
        })
    }

    private fun fullscreenMedia(path: String) {
        waitUntilVideoReady(path)
        sessionRule.evaluateJS(mainSession, "$('video').requestFullscreen()")
        var waiting = true
        while (waiting) {
            sessionRule.waitUntilCalled(object : MediaElementDelegate {
                @AssertCalled
                override fun onFullscreenChange(mediaElement: MediaElement, fullscreen: Boolean) {
                    if (fullscreen) {
                        waiting = false
                    }
                }
            })
        }
    }

    @WithDevToolsAPI
    @Test
    fun oggPlayMedia() {
        playMedia(VIDEO_OGG_PATH)
    }

    @WithDevToolsAPI
    @Ignore //disable test for frequent failures Bug 1554117
    @Test
    fun oggPlayMediaFromScript() {
        playMediaFromScript(VIDEO_OGG_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun oggPauseMedia() {
        pauseMedia(VIDEO_OGG_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun oggTimeMedia() {
        timeMedia(VIDEO_OGG_PATH, 2.0)
    }

    @WithDevToolsAPI
    @Test
    fun oggMetadataMedia() {
        val meta = waitForMetadata(VIDEO_OGG_PATH)
        assertThat("Current source is set", meta?.currentSource, equalTo("resource://android/assets/www/videos/video.ogg"))
        assertThat("Width is set", meta?.width, equalTo(320L))
        assertThat("Height is set", meta?.height, equalTo(240L))
        assertThat("Video is seekable", meta?.isSeekable, equalTo(true))
        // Disabled duration test for Bug 1510393
        // assertThat("Duration is set", meta?.duration, closeTo(4.0, 0.1))
        assertThat("Contains one video track", meta?.videoTrackCount, equalTo(1))
        assertThat("Contains one audio track", meta?.audioTrackCount, equalTo(0))
    }

    @WithDevToolsAPI
    @Test
    fun oggSeekMedia() {
        seekMedia(VIDEO_OGG_PATH, 2.0)
    }

    @WithDevToolsAPI
    @Test
    fun oggFullscreenMedia() {
        fullscreenMedia(VIDEO_OGG_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun webmPlayMedia() {
        playMedia(VIDEO_WEBM_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun webmPlayMediaFromScript() {
        // disable test on pgo and debug for frequently failing Bug 1532404
        assumeTrue(false)
        playMediaFromScript(VIDEO_WEBM_PATH)        
    }

    @WithDevToolsAPI
    @Test
    fun webmPauseMedia() {
        pauseMedia(VIDEO_WEBM_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun webmTimeMedia() {
        timeMedia(VIDEO_WEBM_PATH, 0.2)
    }

    @WithDevToolsAPI
    @Test
    fun webmMetadataMedia() {
        val meta = waitForMetadata(VIDEO_WEBM_PATH)
        assertThat("Current source is set", meta?.currentSource, equalTo("resource://android/assets/www/videos/gizmo.webm"))
        assertThat("Width is set", meta?.width, equalTo(560L))
        assertThat("Height is set", meta?.height, equalTo(320L))
        assertThat("Video is seekable", meta?.isSeekable, equalTo(true))
        assertThat("Duration is set", meta?.duration, closeTo(5.6, 0.1))
        assertThat("Contains one video track", meta?.videoTrackCount, equalTo(1))
        assertThat("Contains one audio track", meta?.audioTrackCount, equalTo(1))
    }

    @WithDevToolsAPI
    @Test
    fun webmSeekMedia() {
        seekMedia(VIDEO_WEBM_PATH, 0.2)
    }

    @WithDevToolsAPI
    @Test
    fun webmFullscreenMedia() {
        fullscreenMedia(VIDEO_WEBM_PATH)
    }

    private fun waitForVolumeChange(volumeLevel: Double, isMuted: Boolean) {
        sessionRule.waitUntilCalled(object : MediaElementDelegate {
            @AssertCalled
            override fun onVolumeChange(mediaElement: MediaElement, volume: Double, muted: Boolean) {
                assertThat("Volume was set", volume, closeTo(volumeLevel, 0.0001))
                assertThat("Not muted", muted, equalTo(isMuted))
            }
        })
    }

    @WithDevToolsAPI
    @Test
    fun webmVolumeMedia() {
        val media = waitUntilVideoReady(VIDEO_WEBM_PATH)
        val volumeLevel = 0.5
        val volumeLevel2 = 0.75
        media.setVolume(volumeLevel)
        waitForVolumeChange(volumeLevel, false)
        media.setMuted(true)
        waitForVolumeChange(volumeLevel, true)
        media.setVolume(volumeLevel2)
        waitForVolumeChange(volumeLevel2, true)
        media.setMuted(false)
        waitForVolumeChange(volumeLevel2, false)
    }

    // NOTE: All MP4 tests are disabled on automation by Bug 1503952
    @WithDevToolsAPI
    @Test
    fun mp4PlayMedia() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        playMedia(VIDEO_MP4_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun mp4PlayMediaFromScript() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        playMediaFromScript(VIDEO_MP4_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun mp4PauseMedia() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        pauseMedia(VIDEO_MP4_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun mp4TimeMedia() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        timeMedia(VIDEO_MP4_PATH, 0.2)
    }

    @WithDevToolsAPI
    @Test
    fun mp4MetadataMedia() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        val meta = waitForMetadata(VIDEO_MP4_PATH)
        assertThat("Current source is set", meta?.currentSource, equalTo("resource://android/assets/www/videos/short.mp4"))
        assertThat("Width is set", meta?.width, equalTo(320L))
        assertThat("Height is set", meta?.height, equalTo(240L))
        assertThat("Video is seekable", meta?.isSeekable, equalTo(true))
        assertThat("Duration is set", meta?.duration, closeTo(0.5, 0.1))
        assertThat("Contains one video track", meta?.videoTrackCount, equalTo(1))
        assertThat("Contains one audio track", meta?.audioTrackCount, equalTo(1))
    }

    @WithDevToolsAPI
    @Test
    fun mp4SeekMedia() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        seekMedia(VIDEO_MP4_PATH, 0.2)
    }

    @WithDevToolsAPI
    @Test
    fun mp4FullscreenMedia() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        fullscreenMedia(VIDEO_MP4_PATH)
    }

    @WithDevToolsAPI
    @Test
    fun mp4VolumeMedia() {
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        val media = waitUntilVideoReady(VIDEO_MP4_PATH)
        val volumeLevel = 0.5
        val volumeLevel2 = 0.75
        media.setVolume(volumeLevel)
        waitForVolumeChange(volumeLevel, false)
        media.setMuted(true)
        waitForVolumeChange(volumeLevel, true)
        media.setVolume(volumeLevel2)
        waitForVolumeChange(volumeLevel2, true)
        media.setMuted(false)
        waitForVolumeChange(volumeLevel2, false)
    }

    @Ignore
    @WithDevToolsAPI
    @Test
    fun badMediaPath() {
        // Disabled on automation by Bug 1503957
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        setupPrefsAndDelegates(VIDEO_BAD_PATH)
        sessionRule.waitForPageStop()
        sessionRule.waitUntilCalled(object : MediaElementDelegate {
            @AssertCalled
            override fun onError(mediaElement: MediaElement, errorCode: Int) {
                assertThat("Got media error", errorCode, equalTo(MediaElement.MEDIA_ERROR_NETWORK_NO_SOURCE))
            }
        })
    }

    @WithDevToolsAPI
    @Test fun autoplayBlocked() {
        sessionRule.runtime.settings.autoplayDefault = GeckoRuntimeSettings.AUTOPLAY_DEFAULT_BLOCKED

        val media = waitUntilVideoReadyNoPrefs(AUTOPLAY_PATH)
        val promise = sessionRule.evaluateJS(mainSession, "$('video').play()").asJSPromise()
        var exceptionCaught = false
        try {
            val result = promise.value as Boolean
            assertThat("Promise should not resolve", result, equalTo(false))
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            exceptionCaught = true;
        }

        assertThat("video.play() failed with exception", exceptionCaught, equalTo(true))
        media.play()
        /*
        // Fails due to bug 1524092
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAY)
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAYING)
        */
    }

    @WithDevToolsAPI
    @Test fun autoplayAllowed() {
        sessionRule.runtime.settings.autoplayDefault = GeckoRuntimeSettings.AUTOPLAY_DEFAULT_ALLOWED

        val media = waitUntilVideoReadyNoPrefs(VIDEO_WEBM_PATH)
        val promise = sessionRule.evaluateJS(mainSession, "$('video').play()").asJSPromise()
        var exceptionCaught = false
        try {
            promise.value
        } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
            exceptionCaught = true;
        }

        assertThat("video.play() did not fail", exceptionCaught, equalTo(false))
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAY)
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAYING)
        media.pause()

        media.play()
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAY)
        waitForPlaybackStateChange(MediaElement.MEDIA_STATE_PLAYING)

        // Restore default runtime settings
        sessionRule.runtime.settings.autoplayDefault = GeckoRuntimeSettings.AUTOPLAY_DEFAULT_BLOCKED
    }

}
