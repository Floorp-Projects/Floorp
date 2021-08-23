/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assume.assumeThat

import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MediaSession

class Metadata(
    title: String?,
    artist: String?,
    album: String?)
        : MediaSession.Metadata(title, artist, album, null) {}

@RunWith(AndroidJUnit4::class)
@MediumTest
class MediaSessionTest : BaseSessionTest() {
    companion object {
        // See MEDIA_SESSION_DOM1_PATH file for details.
        const val DOM_TEST_TITLE1 = "hoot"
        const val DOM_TEST_TITLE2 = "hoot2"
        const val DOM_TEST_TITLE3 = "hoot3"
        const val DOM_TEST_ARTIST1 = "owl"
        const val DOM_TEST_ARTIST2 = "stillowl"
        const val DOM_TEST_ARTIST3 = "immaowl"
        const val DOM_TEST_ALBUM1 = "hoots"
        const val DOM_TEST_ALBUM2 = "dahoots"
        const val DOM_TEST_ALBUM3 = "mahoots"
        const val DEFAULT_TEST_TITLE1 = "MediaSessionDefaultTest1"
        const val TEST_DURATION1 = 3.37
        const val WEBM_TEST_DURATION = 5.59
        const val WEBM_TEST_WIDTH = 560L
        const val WEBM_TEST_HEIGHT = 320L

        val DOM_META = arrayOf(
                Metadata(
                        DOM_TEST_TITLE1,
                        DOM_TEST_ARTIST1,
                        DOM_TEST_ALBUM1),
                Metadata(
                        DOM_TEST_TITLE2,
                        DOM_TEST_ARTIST2,
                        DOM_TEST_ALBUM2),
                Metadata(
                        DOM_TEST_TITLE3,
                        DOM_TEST_ARTIST3,
                        DOM_TEST_ALBUM3))
    }

    @Before
    fun setup() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
            "media.mediacontrol.stopcontrol.aftermediaends" to false,
            "dom.media.mediasession.enabled" to true))
    }

    @After
    fun teardown() {
    }

    @Test
    fun domMetadataPlayback() {
        // TODO: needs bug 1700243
        assumeThat(sessionRule.env.isIsolatedProcess, equalTo(false))

        val onActivatedCalled = arrayOf(GeckoResult<Void>())
        val onMetadataCalled = arrayOf(
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>())
        val onPlayCalled = arrayOf(GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>())
        val onPauseCalled = arrayOf(GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>())

        // Test:
        // 1. Load DOM Media Session page which contains 3 audio tracks.
        // 2. Track 1 is played on page load.
        //    a. Ensure onActivated is called.
        //    b. Ensure onMetadata (1) is called.
        //    c. Ensure onPlay (1) is called.
        val completedStep2 = GeckoResult.allOf(
                onActivatedCalled[0],
                onMetadataCalled[0],
                onPlayCalled[0])

        // 3. Pause playback of track 1.
        //    a. Ensure onPause (1) is called.
        val completedStep3 = GeckoResult.allOf(
                onPauseCalled[0])

        // 4. Resume playback (1).
        //    a. Ensure onMetadata (1) is called.
        //    b. Ensure onPlay (1) is called.
        val completedStep4 = GeckoResult.allOf(
                onPlayCalled[1],
                onMetadataCalled[1])

        // 5. Wait for track 1 end.
        //    a. Ensure onPause (1) is called.
        val completedStep5 = GeckoResult.allOf(
                onPauseCalled[1])

        // 6. Play next track (2).
        //    a. Ensure onMetadata (2) is called.
        //    b. Ensure onPlay (2) is called.
        val completedStep6 = GeckoResult.allOf(
                onMetadataCalled[2],
                onPlayCalled[2])

        // 7. Play next track (3).
        //    a. Ensure onPause (2) is called.
        //    b. Ensure onMetadata (3) is called.
        //    c. Ensure onPlay (3) is called.
        val completedStep7 = GeckoResult.allOf(
                onPauseCalled[2],
                onMetadataCalled[3],
                onPlayCalled[3])

        // 8. Play previous track (2).
        //    a. Ensure onPause (3) is called.
        //    b. Ensure onMetadata (2) is called.
        //    c. Ensure onPlay (2) is called.
        val completedStep8a = GeckoResult.allOf(
                onPauseCalled[3])
        // Without the split, this seems to race and we don't get the pause event.
        val completedStep8b = GeckoResult.allOf(
                onMetadataCalled[4],
                onPlayCalled[4])

        // 9. Wait for track 2 end.
        //    a. Ensure onPause (2) is called.
        val completedStep9 = GeckoResult.allOf(
                onPauseCalled[4])

        val path = MEDIA_SESSION_DOM1_PATH
        val session1 = sessionRule.createOpenSession()

        var mediaSession1 : MediaSession? = null
        // 1.
        session1.loadTestPath(path)

        session1.delegateUntilTestEnd(object : MediaSession.Delegate {
            @AssertCalled(count = 1, order = [1])
            override fun onActivated(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onActivatedCalled[0].complete(null)
                mediaSession1 = mediaSession
            }

            @AssertCalled(false)
            override fun onDeactivated(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
            }

            @AssertCalled
            override fun onFeatures(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    features: Long) {

                val play = (features and MediaSession.Feature.PLAY) != 0L
                val pause = (features and MediaSession.Feature.PAUSE) != 0L
                val stop = (features and MediaSession.Feature.PAUSE) != 0L
                val next = (features and MediaSession.Feature.PAUSE) != 0L
                val prev = (features and MediaSession.Feature.PAUSE) != 0L

                assertThat(
                        "Playback constrols should be supported",
                        play && pause && stop && next && prev,
                        equalTo(true))
            }

            @AssertCalled(count = 5, order = [2])
            override fun onMetadata(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    meta: MediaSession.Metadata) {

                assertThat(
                        "Title should match",
                        meta.title,
                        equalTo(forEachCall(
                                DOM_META[0].title,
                                DOM_META[0].title,
                                DOM_META[1].title,
                                DOM_META[2].title,
                                DOM_META[1].title)))
                assertThat(
                        "Artist should match",
                        meta.artist,
                        equalTo(forEachCall(
                                DOM_META[0].artist,
                                DOM_META[0].artist,
                                DOM_META[1].artist,
                                DOM_META[2].artist,
                                DOM_META[1].artist)))
                assertThat(
                        "Album should match",
                        meta.album,
                        equalTo(forEachCall(
                                DOM_META[0].album,
                                DOM_META[0].album,
                                DOM_META[1].album,
                                DOM_META[2].album,
                                DOM_META[1].album)))
                assertThat(
                        "Artwork image should be non-null",
                        meta.artwork!!.getBitmap(200),
                        notNullValue())

                onMetadataCalled[sessionRule.currentCall.counter - 1]
                        .complete(null)
            }

            @AssertCalled
            override fun onPositionState(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    state: MediaSession.PositionState) {
                assertThat(
                      "Duration should match",
                      state.duration,
                      closeTo(TEST_DURATION1, 0.01))

                assertThat(
                      "Playback rate should match",
                      state.playbackRate,
                      closeTo(1.0, 0.01))

                assertThat(
                      "Position should be >= 0",
                      state.position,
                      greaterThanOrEqualTo(0.0))
            }

            @AssertCalled(count = 5, order = [2])
            override fun onPlay(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPlayCalled[sessionRule.currentCall.counter - 1]
                        .complete(null)
            }

            @AssertCalled(count = 5)
            override fun onPause(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPauseCalled[sessionRule.currentCall.counter - 1]
                        .complete(null)
            }
        })

        sessionRule.waitForResult(completedStep2)
        mediaSession1!!.pause()

        sessionRule.waitForResult(completedStep3)
        mediaSession1!!.play()

        sessionRule.waitForResult(completedStep4)
        sessionRule.waitForResult(completedStep5)
        mediaSession1!!.pause()
        mediaSession1!!.nextTrack()
        mediaSession1!!.play()

        sessionRule.waitForResult(completedStep6)
        mediaSession1!!.pause()
        mediaSession1!!.nextTrack()
        mediaSession1!!.play()

        sessionRule.waitForResult(completedStep7)
        mediaSession1!!.pause()

        sessionRule.waitForResult(completedStep8a)
        mediaSession1!!.previousTrack()
        mediaSession1!!.play()

        sessionRule.waitForResult(completedStep8b)
        sessionRule.waitForResult(completedStep9)
    }

    @Test
    fun defaultMetadataPlayback() {
        // TODO: needs bug 1700243
        assumeThat(sessionRule.env.isIsolatedProcess, equalTo(false))

        val onActivatedCalled = arrayOf(GeckoResult<Void>())
        val onPlayCalled = arrayOf(GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>())
        val onPauseCalled = arrayOf(GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>(),
                GeckoResult<Void>())

        // Test:
        // 1. Load Media Session page which contains 1 audio track.
        // 2. Track 1 is played on page load.
        //    a. Ensure onActivated is called.
        //    b. Ensure onPlay (1) is called.
        val completedStep2 = GeckoResult.allOf(
                onActivatedCalled[0],
                onPlayCalled[0])

        // 3. Pause playback of track 1.
        //    a. Ensure onPause (1) is called.
        val completedStep3 = GeckoResult.allOf(
                onPauseCalled[0])

        // 4. Resume playback (1).
        //    b. Ensure onPlay (1) is called.
        val completedStep4 = GeckoResult.allOf(
                onPlayCalled[1])

        // 5. Wait for track 1 end.
        //    a. Ensure onPause (1) is called.
        val completedStep5 = GeckoResult.allOf(
                onPauseCalled[1])

        val path = MEDIA_SESSION_DEFAULT1_PATH
        val session1 = sessionRule.createOpenSession()

        var mediaSession1 : MediaSession? = null
        // 1.
        session1.loadTestPath(path)

        session1.delegateUntilTestEnd(object : MediaSession.Delegate {
            @AssertCalled(count = 1, order = [1])
            override fun onActivated(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onActivatedCalled[0].complete(null)
                mediaSession1 = mediaSession
            }

            @AssertCalled(count = 2, order = [2])
            override fun onPlay(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPlayCalled[sessionRule.currentCall.counter - 1]
                        .complete(null)
            }

            @AssertCalled(count = 2)
            override fun onPause(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPauseCalled[sessionRule.currentCall.counter - 1]
                        .complete(null)
            }
        })

        sessionRule.waitForResult(completedStep2)
        mediaSession1!!.pause()

        sessionRule.waitForResult(completedStep3)
        mediaSession1!!.play()

        sessionRule.waitForResult(completedStep4)
        sessionRule.waitForResult(completedStep5)
    }

    @Test
    fun domMultiSessions() {
        // TODO: needs bug 1700243
        assumeThat(sessionRule.env.isIsolatedProcess, equalTo(false))

        val onActivatedCalled = arrayOf(
                arrayOf(GeckoResult<Void>()),
                arrayOf(GeckoResult<Void>()))
        val onMetadataCalled = arrayOf(
                arrayOf(
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>()),
                arrayOf(
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>()))
        val onPlayCalled = arrayOf(
                arrayOf(
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>()),
                arrayOf(
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>()))
        val onPauseCalled = arrayOf(
                arrayOf(
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>()),
                arrayOf(
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>(),
                    GeckoResult<Void>()))

        // Test:
        // 1. Session1: Load DOM Media Session page with 3 audio tracks.
        // 2. Session1: Track 1 is played on page load.
        //    a. Session1: Ensure onActivated is called.
        //    b. Session1: Ensure onMetadata (1) is called.
        //    c. Session1: Ensure onPlay (1) is called.
        //    d. Session1: Verify isActive.
        val completedStep2 = GeckoResult.allOf(
                onActivatedCalled[0][0],
                onMetadataCalled[0][0],
                onPlayCalled[0][0])

        // 3. Session1: Pause playback of track 1.
        //    a. Session1: Ensure onPause (1) is called.
        val completedStep3 = GeckoResult.allOf(
                onPauseCalled[0][0])

        // 4. Session2: Load DOM Media Session page with 3 audio tracks.
        // 5. Session2: Track 1 is played on page load.
        //    a. Session2: Ensure onActivated is called.
        //    b. Session2: Ensure onMetadata (1) is called.
        //    c. Session2: Ensure onPlay (1) is called.
        //    d. Session2: Verify isActive.
        val completedStep5 = GeckoResult.allOf(
                onActivatedCalled[1][0],
                onMetadataCalled[1][0],
                onPlayCalled[1][0])

        // 6. Session2: Pause playback of track 1.
        //    a. Session2: Ensure onPause (1) is called.
        val completedStep6 = GeckoResult.allOf(
                onPauseCalled[1][0])

        // 7. Session1: Play next track (2).
        //    a. Session1: Ensure onMetadata (2) is called.
        //    b. Session1: Ensure onPlay (2) is called.
        val completedStep7 = GeckoResult.allOf(
                onMetadataCalled[0][1],
                onPlayCalled[0][1])

        // 8. Session1: wait for track 1 end.
        //    a. Ensure onPause (1) is called.
        val completedStep8 = GeckoResult.allOf(
                onPauseCalled[0][1])

        val path = MEDIA_SESSION_DOM1_PATH
        val session1 = sessionRule.createOpenSession()
        val session2 = sessionRule.createOpenSession()
        var mediaSession1 : MediaSession? = null
        var mediaSession2 : MediaSession? = null

        session1.delegateUntilTestEnd(object : MediaSession.Delegate {
            @AssertCalled(count = 1)
            override fun onActivated(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onActivatedCalled[0][sessionRule.currentCall.counter - 1]
                        .complete(null)
                mediaSession1 = mediaSession

                assertThat(
                        "Should be active",
                        mediaSession1?.isActive,
                        equalTo(true))
            }

            @AssertCalled
            override fun onPositionState(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    state: MediaSession.PositionState) {
                assertThat(
                      "Duration should match",
                      state.duration,
                      closeTo(TEST_DURATION1, 0.01))

                assertThat(
                      "Playback rate should match",
                      state.playbackRate,
                      closeTo(1.0, 0.01))

                assertThat(
                      "Position should be >= 0",
                      state.position,
                      greaterThanOrEqualTo(0.0))
            }

            @AssertCalled
            override fun onFeatures(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    features: Long) {

                val play = (features and MediaSession.Feature.PLAY) != 0L
                val pause = (features and MediaSession.Feature.PAUSE) != 0L
                val stop = (features and MediaSession.Feature.PAUSE) != 0L
                val next = (features and MediaSession.Feature.PAUSE) != 0L
                val prev = (features and MediaSession.Feature.PAUSE) != 0L

                assertThat(
                        "Playback constrols should be supported",
                        play && pause && stop && next && prev,
                        equalTo(true))
            }

            @AssertCalled
            override fun onMetadata(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    meta: MediaSession.Metadata) {
                val count = sessionRule.currentCall.counter
                if (count < 3) {
                        // Ignore redundant calls.
                        onMetadataCalled[0][count - 1].complete(null)
                }

                assertThat(
                        "Title should match",
                        meta.title,
                        equalTo(forEachCall(
                                DOM_META[0].title,
                                DOM_META[1].title)))
                assertThat(
                        "Artist should match",
                        meta.artist,
                        equalTo(forEachCall(
                                DOM_META[0].artist,
                                DOM_META[1].artist)))
                assertThat(
                        "Album should match",
                        meta.album,
                        equalTo(forEachCall(
                                DOM_META[0].album,
                                DOM_META[1].album)))
                assertThat(
                        "Artwork image should be non-null",
                        meta.artwork!!.getBitmap(200),
                        notNullValue())
            }

            @AssertCalled(count = 2)
            override fun onPlay(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPlayCalled[0][sessionRule.currentCall.counter - 1]
                        .complete(null)
            }

            @AssertCalled(count = 2)
            override fun onPause(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPauseCalled[0][sessionRule.currentCall.counter - 1]
                        .complete(null)
            }
        })

        session2.delegateUntilTestEnd(object : MediaSession.Delegate {
            @AssertCalled(count = 1)
            override fun onActivated(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onActivatedCalled[1][sessionRule.currentCall.counter - 1]
                        .complete(null)
                mediaSession2 = mediaSession;

                assertThat(
                        "Should be active",
                        mediaSession1!!.isActive,
                        equalTo(true))
                assertThat(
                        "Should be active",
                        mediaSession2!!.isActive,
                        equalTo(true))
            }

            @AssertCalled
            override fun onMetadata(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    meta: MediaSession.Metadata) {
                val count = sessionRule.currentCall.counter
                if (count < 2) {
                        // Ignore redundant calls.
                        onMetadataCalled[1][0].complete(null)
                }

                assertThat(
                        "Title should match",
                        meta.title,
                        equalTo(forEachCall(
                                DOM_META[0].title)))
                assertThat(
                        "Artist should match",
                        meta.artist,
                        equalTo(forEachCall(
                                DOM_META[0].artist)))
                assertThat(
                        "Album should match",
                        meta.album,
                        equalTo(forEachCall(
                                DOM_META[0].album)))
            }

            @AssertCalled(count = 1)
            override fun onPlay(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPlayCalled[1][sessionRule.currentCall.counter - 1]
                        .complete(null)
            }

            @AssertCalled(count = 1)
            override fun onPause(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPauseCalled[1][sessionRule.currentCall.counter - 1]
                        .complete(null)
            }
        })

        session1.loadTestPath(path)
        sessionRule.waitForResult(completedStep2)

        mediaSession1!!.pause()
        sessionRule.waitForResult(completedStep3)

        session2.loadTestPath(path)
        sessionRule.waitForResult(completedStep5)

        mediaSession2!!.pause()
        sessionRule.waitForResult(completedStep6)

        mediaSession1!!.pause()
        mediaSession1!!.nextTrack()
        mediaSession1!!.play()
        sessionRule.waitForResult(completedStep7)
        sessionRule.waitForResult(completedStep8)
    }

    @Test
    fun fullscreenVideoElementMetadata() {
        // TODO: bug 1706656
        assumeThat(sessionRule.env.isIsolatedProcess, equalTo(false))

        sessionRule.setPrefsUntilTestEnd(mapOf(
                "media.autoplay.default" to 0,
                "full-screen-api.allow-trusted-requests-only" to false))

        val onActivatedCalled = GeckoResult<Void>()
        val onPlayCalled = GeckoResult<Void>()
        val onPauseCalled = GeckoResult<Void>()
        val onFullscreenCalled = arrayOf(
                GeckoResult<Void>(),
                GeckoResult<Void>())

        // Test:
        // 1. Load video test page which contains 1 video element.
        //    a. Ensure page has loaded.
        // 2. Play video element.
        //    a. Ensure onActivated is called.
        //    b. Ensure onPlay is called.
        val completedStep2 = GeckoResult.allOf(
                onActivatedCalled,
                onPlayCalled)

        // 3. Enter fullscreen of the video.
        //    a. Ensure onFullscreen is called.
        val completedStep3 = GeckoResult.allOf(
                onFullscreenCalled[0])

        // 4. Exit fullscreen of the video.
        //    a. Ensure onFullscreen is called.
        val completedStep4 = GeckoResult.allOf(
                onFullscreenCalled[1])

        // 5. Pause the video.
        //    a. Ensure onPause is called.
        val completedStep5 = GeckoResult.allOf(
                onPauseCalled)

        var mediaSession1 : MediaSession? = null

        val path = VIDEO_WEBM_PATH
        val session1 = sessionRule.createOpenSession()

        session1.delegateUntilTestEnd(object : MediaSession.Delegate {
            @AssertCalled(count = 1, order = [1])
            override fun onActivated(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                mediaSession1 = mediaSession

                onActivatedCalled.complete(null)

                assertThat(
                        "Should be active",
                        mediaSession.isActive,
                        equalTo(true))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onPlay(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPlayCalled.complete(null)
            }

            @AssertCalled(count = 1)
            override fun onPause(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onPauseCalled.complete(null)
            }

            @AssertCalled(count = 2)
            override fun onFullscreen(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    enabled: Boolean,
                    meta: MediaSession.ElementMetadata?) {
                if (sessionRule.currentCall.counter == 1) {
                    assertThat(
                        "Fullscreen should be enabled",
                        enabled,
                        equalTo(true))
                    assertThat(
                        "Element metadata should exist",
                        meta,
                        notNullValue())
                    assertThat(
                        "Duration should match",
                        meta!!.duration,
                        closeTo(WEBM_TEST_DURATION, 0.01))
                    assertThat(
                        "Width should match",
                        meta.width,
                        equalTo(WEBM_TEST_WIDTH))
                    assertThat(
                        "Height should match",
                        meta.height,
                        equalTo(WEBM_TEST_HEIGHT))
                    assertThat(
                        "Audio track count should match",
                        meta.audioTrackCount,
                        equalTo(1))
                    assertThat(
                        "Video track count should match",
                        meta.videoTrackCount,
                        equalTo(1))

                } else {
                    assertThat(
                        "Fullscreen should be disabled",
                        enabled,
                        equalTo(false))
                }

                onFullscreenCalled[sessionRule.currentCall.counter - 1]
                        .complete(null)
            }
        })

        // 1.
        session1.loadTestPath(path)
        sessionRule.waitForPageStop()

        // 2.
        session1.evaluateJS("document.querySelector('video').play()")
        sessionRule.waitForResult(completedStep2)

        // 3.
        session1.evaluateJS(
                "document.querySelector('video').requestFullscreen()")
        sessionRule.waitForResult(completedStep3)

        // 4.
        session1.evaluateJS("document.exitFullscreen()")
        sessionRule.waitForResult(completedStep4)

        // 5.
        mediaSession1!!.pause()
        sessionRule.waitForResult(completedStep5)
    }
}
