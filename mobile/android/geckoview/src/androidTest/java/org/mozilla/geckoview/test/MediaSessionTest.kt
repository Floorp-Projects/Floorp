/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import android.util.Log

import org.hamcrest.Matchers.*
import org.json.JSONObject
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assume.assumeThat
import org.junit.Assume.assumeTrue

import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.util.Callbacks

import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MediaSession

class Metadata(
    title: String?,
    artist: String?,
    album: String?)
        : MediaSession.Metadata(title, artist, album) {}

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

        val DEFAULT_META = arrayOf(
                Metadata(
                        DEFAULT_TEST_TITLE1,
                        // TODO: enforce null for empty strings?
                        "",
                        ""))
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

    @Ignore // TODO: Enable with disabled controller deactivation settings.
    @Test
    fun domMetadataPlayback() {
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
                onPlayCalled[1])

        // 5. Wait for track 1 end.
        //    a. Ensure onPause (1) is called.
        val completedStep5 = GeckoResult.allOf(
                onPauseCalled[1])

        // 6. Play next track (2).
        //    a. Ensure onMetadata (2) is called.
        //    b. Ensure onPlay (2) is called.
        val completedStep6 = GeckoResult.allOf(
                onMetadataCalled[1],
                onPlayCalled[2])

        // 7. Play next track (3).
        //    a. Ensure onPause (2) is called.
        //    b. Ensure onMetadata (3) is called.
        //    c. Ensure onPlay (3) is called.
        val completedStep7 = GeckoResult.allOf(
                onPauseCalled[2],
                onMetadataCalled[2],
                onPlayCalled[3])

        // 8. Play previous track (2).
        //    a. Ensure onPause (3) is called.
        //    b. Ensure onMetadata (2) is called.
        //    c. Ensure onPlay (2) is called.
        val completedStep8 = GeckoResult.allOf(
                onPauseCalled[3],
                onMetadataCalled[3],
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

        session1.delegateUntilTestEnd(object : Callbacks.MediaSessionDelegate {
            @AssertCalled(count = 1)
            override fun onActivated(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onActivatedCalled[0].complete(null)
                mediaSession1 = mediaSession
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

            @AssertCalled(count = 5)
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

            @AssertCalled(count = 5)
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
        mediaSession1!!.nextTrack()

        sessionRule.waitForResult(completedStep6)
        mediaSession1!!.nextTrack()

        sessionRule.waitForResult(completedStep7)
        mediaSession1!!.previousTrack()

        sessionRule.waitForResult(completedStep8)
        sessionRule.waitForResult(completedStep9)
    }

    @Ignore // TODO: Enable with disabled controller deactivation settings.
    @Test
    fun defaultMetadataPlayback() {
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
        // 1. Load Media Session page which contains 1 audio track.
        // 2. Track 1 is played on page load.
        //    a. Ensure onActivated is called.
        //    a. Ensure onMetadata (1) is called.
        //    b. Ensure onPlay (1) is called.
        val completedStep2 = GeckoResult.allOf(
                onActivatedCalled[0],
                onMetadataCalled[0],
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

        session1.delegateUntilTestEnd(object : Callbacks.MediaSessionDelegate {
            @AssertCalled(count = 1)
            override fun onActivated(
                    session: GeckoSession,
                    mediaSession: MediaSession) {
                onActivatedCalled[0].complete(null)
                mediaSession1 = mediaSession
            }

            /*
            TODO: currently not called for non-media-session content.
            @AssertCalled
            override fun onFeatures(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    features: Long) {

                val play = (features and MediaSession.Feature.PLAY) != 0L
                val pause = (features and MediaSession.Feature.PAUSE) != 0L
                val stop = (features and MediaSession.Feature.PAUSE) != 0L

                assertThat(
                        "Core playback constrols should be supported",
                        play && pause && stop,
                        equalTo(true))
            }
            */

            @AssertCalled(count = 1)
            override fun onMetadata(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    meta: MediaSession.Metadata) {
                assertThat(
                        "Title should match",
                        meta.title,
                        equalTo(DEFAULT_META[0].title))
                assertThat(
                        "Artist should match",
                        meta.artist,
                        equalTo(DEFAULT_META[0].artist))
                assertThat(
                        "Album should match",
                        meta.album,
                        equalTo(DEFAULT_META[0].album))

                onMetadataCalled[sessionRule.currentCall.counter - 1]
                        .complete(null)
            }

            @AssertCalled(count = 2)
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

    @Ignore // We don't need to control active state anymore.
    @Test
    fun domMultiSessions() {
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
        //    e. Session1: Verify !isActive.
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

        session1.delegateUntilTestEnd(object : Callbacks.MediaSessionDelegate {
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

            @AssertCalled(count = 2)
            override fun onMetadata(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    meta: MediaSession.Metadata) {
                onMetadataCalled[0][sessionRule.currentCall.counter - 1]
                        .complete(null)

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

        session2.delegateUntilTestEnd(object : Callbacks.MediaSessionDelegate {
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

            @AssertCalled(count = 1)
            override fun onMetadata(
                    session: GeckoSession,
                    mediaSession: MediaSession,
                    meta: MediaSession.Metadata) {
                onMetadataCalled[1][sessionRule.currentCall.counter - 1]
                        .complete(null)

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

        mediaSession1!!.nextTrack()
        sessionRule.waitForResult(completedStep7)
        sessionRule.waitForResult(completedStep8)
    }
}
