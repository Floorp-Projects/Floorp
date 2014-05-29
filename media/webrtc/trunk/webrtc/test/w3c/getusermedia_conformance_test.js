// Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

setup({timeout:10000});

// Helper functions to minimize code duplication.
function failedCallback(test) {
  return test.step_func(function (error) {
    assert_unreached('Should not get an error callback');
  });
}
function invokeGetUserMedia(test, okCallback) {
  getUserMedia({ video: true, audio: true }, okCallback,
      failedCallback(test));
}

// 4.2 MediaStream.
var mediaStreamTest = async_test('4.2 MediaStream');

function verifyMediaStream(stream) {
  // TODO(kjellander): Add checks for default values where applicable.
  test(function () {
    assert_own_property(stream, 'id');
    assert_true(typeof stream.id === 'string');
    assert_readonly(stream, 'id');
  }, '[MediaStream] id attribute');

  test(function () {
    assert_inherits(stream, 'getAudioTracks');
    assert_true(typeof stream.getAudioTracks === 'function');
  }, '[MediaStream] getAudioTracks function');

  test(function () {
    assert_inherits(stream, 'getVideoTracks');
    assert_true(typeof stream.getVideoTracks === 'function');
  }, '[MediaStream] getVideoTracks function');

  test(function () {
    assert_inherits(stream, 'getTrackById');
    assert_true(typeof stream.getTrackById === 'function');
  }, '[MediaStream] getTrackById function');

  test(function () {
    assert_inherits(stream, 'addTrack');
    assert_true(typeof stream.addTrack === 'function');
  }, '[MediaStream] addTrack function');

  test(function () {
    assert_inherits(stream, 'removeTrack');
    assert_true(typeof stream.removeTrack === 'function');
  }, '[MediaStream] removeTrack function');

  test(function () {
    // Missing in Chrome.
    assert_inherits(stream, 'clone');
    assert_true(typeof stream.clone === 'function');
  }, '[MediaStream] clone function');

  test(function () {
    assert_own_property(stream, 'ended');
    assert_true(typeof stream.ended === 'boolean');
    assert_readonly(stream, 'ended');
  }, '[MediaStream] ended attribute');

  test(function () {
    assert_own_property(stream, 'onended');
    assert_true(stream.onended === null);
  }, '[MediaStream] onended EventHandler');

  test(function () {
    assert_own_property(stream, 'onaddtrack');
    assert_true(stream.onaddtrack === null);
  }, '[MediaStream] onaddtrack EventHandler');

  test(function () {
    assert_own_property(stream, 'onremovetrack');
    assert_true(stream.onremovetrack === null);
  }, '[MediaStream] onremovetrack EventHandler');
}

mediaStreamTest.step(function() {
  var okCallback = mediaStreamTest.step_func(function (stream) {
    verifyMediaStream(stream);

    var videoTracks = stream.getVideoTracks();
    assert_true(videoTracks.length > 0);

    // Verify event handlers are working.
    stream.onaddtrack = onAddTrackCallback
    stream.onremovetrack = onRemoveTrackCallback
    stream.removeTrack(videoTracks[0]);
    stream.addTrack(videoTracks[0]);
    mediaStreamTest.done();
  });
  var onAddTrackCallback = mediaStreamTest.step_func(function () {
    // TODO(kjellander): verify number of tracks.
    mediaStreamTest.done();
  });
  var onRemoveTrackCallback = mediaStreamTest.step_func(function () {
    // TODO(kjellander): verify number of tracks.
    mediaStreamTest.done();
  });
  invokeGetUserMedia(mediaStreamTest, okCallback);;
});

// 4.3 MediaStreamTrack.
var mediaStreamTrackTest = async_test('4.3 MediaStreamTrack');

function verifyTrack(type, track) {
  test(function () {
    assert_own_property(track, 'kind');
    assert_readonly(track, 'kind');
    assert_true(typeof track.kind === 'string',
        'kind is an object (DOMString)');
  }, '[MediaStreamTrack (' + type + ')] kind attribute');

  test(function () {
    assert_own_property(track, 'id');
    assert_readonly(track, 'id');
    assert_true(typeof track.id === 'string',
        'id is an object (DOMString)');
  }, '[MediaStreamTrack (' + type + ')] id attribute');

  test(function () {
    assert_own_property(track, 'label');
    assert_readonly(track, 'label');
    assert_true(typeof track.label === 'string',
        'label is an object (DOMString)');
  }, '[MediaStreamTrack (' + type + ')] label attribute');

  test(function () {
    assert_own_property(track, 'enabled');
    assert_true(typeof track.enabled === 'boolean');
    assert_true(track.enabled, 'enabled property must be true initially');
  }, '[MediaStreamTrack (' + type + ')] enabled attribute');

  test(function () {
    // Missing in Chrome.
    assert_own_property(track, 'muted');
    assert_readonly(track, 'muted');
    assert_true(typeof track.muted === 'boolean');
    assert_false(track.muted, 'muted property must be false initially');
  }, '[MediaStreamTrack (' + type + ')] muted attribute');

  test(function () {
    assert_own_property(track, 'onmute');
    assert_true(track.onmute === null);
  }, '[MediaStreamTrack (' + type + ')] onmute EventHandler');

  test(function () {
    assert_own_property(track, 'onunmute');
    assert_true(track.onunmute === null);
  }, '[MediaStreamTrack (' + type + ')] onunmute EventHandler');

  test(function () {
    // Missing in Chrome.
    assert_own_property(track, '_readonly');
    assert_readonly(track, '_readonly');
    assert_true(typeof track._readonly === 'boolean');
  }, '[MediaStreamTrack (' + type + ')] _readonly attribute');

  test(function () {
    // Missing in Chrome.
    assert_own_property(track, 'remote');
    assert_readonly(track, 'remote');
    assert_true(typeof track.remote === 'boolean');
  }, '[MediaStreamTrack (' + type + ')] remote attribute');

  test(function () {
    assert_own_property(track, 'readyState');
    assert_readonly(track, 'readyState');
    assert_true(typeof track.readyState === 'string');
    // TODO(kjellander): verify the initial state.
  }, '[MediaStreamTrack (' + type + ')] readyState attribute');

  test(function () {
    // Missing in Chrome.
    assert_own_property(track, 'onstarted');
    assert_true(track.onstarted === null);
  }, '[MediaStreamTrack (' + type + ')] onstarted EventHandler');

  test(function () {
    assert_own_property(track, 'onended');
    assert_true(track.onended === null);
  }, '[MediaStreamTrack (' + type + ')] onended EventHandler');

  test(function () {
    // Missing in Chrome.
    assert_inherits(track, 'getSourceInfos');
    assert_true(typeof track.getSourceInfos === 'function');
  }, '[MediaStreamTrack (' + type + ')]: getSourceInfos function');

  test(function () {
    // Missing in Chrome.
    assert_inherits(track, 'constraints');
    assert_true(typeof track.constraints === 'function');
  }, '[MediaStreamTrack (' + type + ')]: constraints function');

  test(function () {
    // Missing in Chrome.
    assert_inherits(track, 'states');
    assert_true(typeof track.states === 'function');
  }, '[MediaStreamTrack (' + type + ')]: states function');

  test(function () {
    // Missing in Chrome.
    assert_inherits(track, 'capabilities');
    assert_true(typeof track.capabilities === 'function');
  }, '[MediaStreamTrack (' + type + ')]: capabilities function');

  test(function () {
    // Missing in Chrome.
    assert_inherits(track, 'applyConstraints');
    assert_true(typeof track.applyConstraints === 'function');
  }, '[MediaStreamTrack (' + type + ')]: applyConstraints function');

  test(function () {
    // Missing in Chrome.
    assert_own_property(track, 'onoverconstrained');
    assert_true(track.onoverconstrained === null);
  }, '[MediaStreamTrack (' + type + ')] onoverconstrained EventHandler');

  test(function () {
    // Missing in Chrome.
    assert_inherits(track, 'clone');
    assert_true(typeof track.clone === 'function');
  }, '[MediaStreamTrack (' + type + ')] clone function');

  test(function () {
    // Missing in Chrome.
    assert_inherits(track, 'stop');
    assert_true(typeof track.stop === 'function');
  }, '[MediaStreamTrack (' + type + ')] stop function');
};
mediaStreamTrackTest.step(function() {
  var okCallback = mediaStreamTrackTest.step_func(function (stream) {
    verifyTrack('audio', stream.getAudioTracks()[0]);
    verifyTrack('video', stream.getVideoTracks()[0]);
    mediaStreamTrackTest.done();
  });
  invokeGetUserMedia(mediaStreamTrackTest, okCallback);
});

mediaStreamTrackTest.step(function() {
  var okCallback = mediaStreamTrackTest.step_func(function (stream) {
    // Verify event handlers are working.
    var track = stream.getVideoTracks()[0];
    track.onended = onendedCallback
    track.stop();
    mediaStreamTrackTest.done();
  });
  var onendedCallback = mediaStreamTrackTest.step_func(function () {
    assert_true(track.ended);
    mediaStreamTrackTest.done();
  });
  invokeGetUserMedia(mediaStreamTrackTest, okCallback);
});

// 4.4 MediaStreamTrackEvent tests.
var mediaStreamTrackEventTest = async_test('4.4 MediaStreamTrackEvent');
mediaStreamTrackEventTest.step(function() {
  var okCallback = mediaStreamTrackEventTest.step_func(function (stream) {
    // TODO(kjellander): verify attributes
    mediaStreamTrackEventTest.done();
  });
  invokeGetUserMedia(mediaStreamTrackEventTest, okCallback);
});

// 4.5 Video and Audio Tracks tests.
var avTracksTest = async_test('4.5 Video and Audio Tracks');
avTracksTest.step(function() {
  var okCallback = avTracksTest.step_func(function (stream) {
    // TODO(kjellander): verify attributes
    avTracksTest.done();
  });
  invokeGetUserMedia(avTracksTest, okCallback);
});

// 5. The model: sources, sinks, constraints, and states

// 6. Source states
// 6.1 Dictionary MediaSourceStates Members

// 7. Source capabilities
// 7.1 Dictionary CapabilityRange Members
// 7.2 CapabilityList array
// 7.3 Dictionary AllVideoCapabilities Members
// 7.4 Dictionary AllAudioCapabilities Members

// 8. URL tests.
var createObjectURLTest = async_test('8.1 URL createObjectURL method');
createObjectURLTest.step(function() {
  var okCallback = createObjectURLTest.step_func(function (stream) {
    var url = webkitURL.createObjectURL(stream);
    assert_true(typeof url === 'string');
    createObjectURLTest.done();
  });
  invokeGetUserMedia(createObjectURLTest, okCallback);
});

// 9. MediaStreams as Media Elements.
var mediaElementsTest = async_test('9. MediaStreams as Media Elements');

function verifyVideoTagWithStream(videoTag) {
  test(function () {
    assert_equals(videoTag.buffered.length, 0);
  }, '[Video tag] buffered attribute');

  test(function () {
    // Attempts to alter currentTime shall be ignored.
    assert_true(videoTag.currentTime >= 0);
    assert_throws('InvalidStateError',
                  function () { videoTag.currentTime = 1234; },
                  'Attempts to modify currentTime shall throw ' +
                      'InvalidStateError');
  }, '[Video tag] currentTime attribute');

  test(function () {
    assert_equals(videoTag.duration, Infinity, 'videoTag.duration');
  }, '[Video tag] duration attribute');

  test(function () {
    assert_false(videoTag.seeking, 'videoTag.seeking');
  }, '[Video tag] seeking attribute');

  test(function () {
    assert_equals(videoTag.defaultPlaybackRate, 1.0);
    assert_throws('DOMException',
                  function () { videoTag.defaultPlaybackRate = 2.0; },
                  'Attempts to alter videoTag.defaultPlaybackRate MUST fail');
  }, '[Video tag] defaultPlaybackRate attribute');

  test(function () {
    assert_equals(videoTag.playbackRate, 1.0);
    assert_throws('DOMException',
      function () { videoTag.playbackRate = 2.0; },
      'Attempts to alter videoTag.playbackRate MUST fail');
  }, '[Video tag] playbackRate attribute');

  test(function () {
    assert_equals(videoTag.played.length, 1, 'videoTag.played.length');
    assert_equals(videoTag.played.start(0), 0);
    assert_true(videoTag.played.end(0) >= videoTag.currentTime);
  }, '[Video tag] played attribute');

  test(function () {
    assert_equals(videoTag.seekable.length, 0);
    assert_equals(videoTag.seekable.start(), videoTag.currentTime);
    assert_equals(videoTag.seekable.end(), videoTag.currentTime);
    assert_equals(videoTag.startDate, NaN, 'videoTag.startDate');
  }, '[Video tag] seekable attribute');

  test(function () {
    assert_false(videoTag.loop);
  }, '[Video tag] loop attribute');
};

mediaElementsTest.step(function() {
  var okCallback = mediaElementsTest.step_func(function (stream) {
    var videoTag = document.getElementById('local-view');
    // Call the polyfill wrapper to attach the media stream to this element.
    attachMediaStream(videoTag, stream);
    verifyVideoTagWithStream(videoTag);
    mediaElementsTest.done();
  });
  invokeGetUserMedia(mediaElementsTest, okCallback);
});

// 11. Obtaining local multimedia content.

// 11.1 NavigatorUserMedia.
var getUserMediaTest = async_test('11.1 NavigatorUserMedia');
getUserMediaTest.step(function() {
  var okCallback = getUserMediaTest.step_func(function (stream) {
    assert_true(stream !== null);
    getUserMediaTest.done();
  });

  // boolean parameters, without failure callback:
  getUserMedia({ video: true, audio: true }, okCallback);
  getUserMedia({ video: true, audio: false }, okCallback);
  getUserMedia({ video: false, audio: true }, okCallback);

  // boolean parameters, with failure callback:
  getUserMedia({ video: true, audio: true }, okCallback,
      failedCallback(getUserMediaTest));
  getUserMedia({ video: true, audio: false }, okCallback,
      failedCallback(getUserMediaTest));
  getUserMedia({ video: false, audio: true }, okCallback,
      failedCallback(getUserMediaTest));
});

// 11.2 MediaStreamConstraints.
var constraintsTest = async_test('11.2 MediaStreamConstraints');
constraintsTest.step(function() {
  var okCallback = constraintsTest.step_func(function (stream) {
    assert_true(stream !== null);
    constraintsTest.done();
  });

  // Constraints on video.
  // See http://webrtc.googlecode.com/svn/trunk/samples/js/demos/html/constraints-and-stats.html
  // for more examples of constraints.
  var constraints = {};
  constraints.audio = true;
  constraints.video = { mandatory: {}, optional: [] };
  constraints.video.mandatory.minWidth = 640;
  constraints.video.mandatory.minHeight = 480;
  constraints.video.mandatory.minFrameRate = 15;

  getUserMedia(constraints, okCallback, failedCallback(constraintsTest));
});

// 11.3 NavigatorUserMediaSuccessCallback.
var successCallbackTest =
  async_test('11.3 NavigatorUserMediaSuccessCallback');
successCallbackTest.step(function() {
  var okCallback = successCallbackTest.step_func(function (stream) {
    assert_true(stream !== null);
    successCallbackTest.done();
  });
  invokeGetUserMedia(successCallbackTest, okCallback);
});

// 11.4 NavigatorUserMediaError and NavigatorUserMediaErrorCallback.
var errorCallbackTest = async_test('11.4 NavigatorUserMediaError and ' +
                                   'NavigatorUserMediaErrorCallback');
errorCallbackTest.step(function() {
  var okCallback = errorCallbackTest.step_func(function (stream) {
    assert_unreached('Should not get a success callback');
  });
  var errorCallback = errorCallbackTest.step_func(function (error) {
    assert_own_property(error, 'name');
    assert_readonly(error.name);
    assert_true(typeof error.name === 'string');
    assert_equals(error.name, 'ConstraintNotSatisfiedError', 'error.name');
    errorCallbackTest.done();
  });
  // Setting both audio and video to false triggers an error callback.
  // TODO(kjellander): Figure out if there's a way in the spec to trigger an
  // error callback.

  // TODO(kjellander): Investigate why the error callback is not called when
  // false/false is provided in Chrome.
  getUserMedia({ video: false, audio: false }, okCallback, errorCallback);
});
