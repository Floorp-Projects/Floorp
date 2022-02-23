// META: global=window,dedicatedworker
// META: script=/common/media.js
// META: script=/webcodecs/utils.js

const defaultConfig = {
  codec: 'vp8',
  width: 640,
  height: 480
};

let bitmap_blob = null;

async function generateBitmap(width, height) {
  if (!bitmap_blob) {
    let response = await fetch("pattern.png");
    bitmap_blob = await response.blob();
  }

  var size = {
    resizeWidth: width,
    resizeHeight: height
  };

  return createImageBitmap(bitmap_blob, size);
}

async function createVideoFrame(width, height, timestamp) {
  let bitmap = await generateBitmap(width, height);
  return new VideoFrame(bitmap, { timestamp: timestamp });
}

promise_test(t => {
  // VideoEncoderInit lacks required fields.
  assert_throws_js(TypeError, () => { new VideoEncoder({}); });

  // VideoEncoderInit has required fields.
  let encoder = new VideoEncoder(getDefaultCodecInit(t));

  assert_equals(encoder.state, "unconfigured");

  encoder.close();

  return endAfterEventLoopTurn();
}, 'Test VideoEncoder construction');

promise_test(t => {
  let encoder = new VideoEncoder(getDefaultCodecInit(t));

  let badCodecsList = [
    '',                         // Empty codec
    'bogus',                    // Non exsitent codec
    'vorbis',                   // Audio codec
    'vp9',                      // Ambiguous codec
    'video/webm; codecs="vp9"'  // Codec with mime type
  ]

  testConfigurations(encoder, defaultConfig, badCodecsList);

  return endAfterEventLoopTurn();
}, 'Test VideoEncoder.configure()');

promise_test(async t => {
  let output_chunks = [];
  let codecInit = getDefaultCodecInit(t);
  let decoderConfig = null;
  let encoderConfig = {
    codec: 'vp8',
    width: 640,
    height: 480,
    displayWidth: 800,
    displayHeight: 600,
  };

  codecInit.output = (chunk, metadata) => {
    assert_not_equals(metadata, null);
    if (metadata.decoderConfig)
      decoderConfig = metadata.decoderConfig;
    output_chunks.push(chunk);
  }

  let encoder = new VideoEncoder(codecInit);
  encoder.configure(encoderConfig);

  let frame1 = await createVideoFrame(640, 480, 0);
  let frame2 = await createVideoFrame(640, 480, 33333);

  encoder.encode(frame1);
  encoder.encode(frame2);

  await encoder.flush();

  // Decoder config should be given with the first chunk
  assert_not_equals(decoderConfig, null);
  assert_equals(decoderConfig.codec, encoderConfig.codec);
  assert_greater_than_equal(decoderConfig.codedHeight, encoderConfig.height);
  assert_greater_than_equal(decoderConfig.codedWidth, encoderConfig.width);
  assert_equals(decoderConfig.displayAspectHeight, encoderConfig.displayHeight);
  assert_equals(decoderConfig.displayAspectWidth, encoderConfig.displayWidth);
  assert_not_equals(decoderConfig.colorSpace.primaries, null);
  assert_not_equals(decoderConfig.colorSpace.transfer, null);
  assert_not_equals(decoderConfig.colorSpace.matrix, null);
  assert_not_equals(decoderConfig.colorSpace.fullRange, null);

  assert_equals(output_chunks.length, 2);
  assert_equals(output_chunks[0].timestamp, frame1.timestamp);
  assert_equals(output_chunks[1].timestamp, frame2.timestamp);
}, 'Test successful configure(), encode(), and flush()');

promise_test(async t => {
  let codecInit = getDefaultCodecInit(t);
  let encoderConfig = {
    codec: 'vp8',
    width: 320,
    height: 200
  };

  codecInit.output = (chunk, metadata) => {}

  let encoder = new VideoEncoder(codecInit);

  // No encodes yet.
  assert_equals(encoder.encodeQueueSize, 0);

  encoder.configure(encoderConfig);

  // Still no encodes.
  assert_equals(encoder.encodeQueueSize, 0);

  const frames_count = 100;
  let frames = [];
  for (let i = 0; i < frames_count; i++) {
    let frame = await createVideoFrame(320, 200, i * 16000);
    frames.push(frame);
  }

  for (let frame of frames)
    encoder.encode(frame);

  // Some encodes should have already started being processed, but not all
  // 100 of them.
  assert_greater_than(encoder.encodeQueueSize, 0);
  assert_less_than(encoder.encodeQueueSize, frames_count);

  await encoder.flush();
  // We can guarantee that all encodes are processed after a flush.
  assert_equals(encoder.encodeQueueSize, 0);

  for (let frame of frames) {
    encoder.encode(frame);
    frame.close();
  }

  assert_greater_than(encoder.encodeQueueSize, 0);
  encoder.reset();
  assert_equals(encoder.encodeQueueSize, 0);
}, 'encodeQueueSize test');


promise_test(async t => {
  let timestamp = 0;
  let callbacks_before_reset = 0;
  let callbacks_after_reset = 0;
  const timestamp_step = 40000;
  const expected_callbacks_before_reset = 3;
  let codecInit = getDefaultCodecInit(t);
  let bitmap = await generateBitmap(320, 200);
  let encoder = null;
  let reset_completed = false;
  codecInit.output = (chunk, metadata) => {
    if (chunk.timestamp % 2 == 0) {
      // pre-reset frames have even timestamp
      callbacks_before_reset++;
      if (callbacks_before_reset == expected_callbacks_before_reset) {
        encoder.reset();
        reset_completed = true;
      }
    } else {
      // after-reset frames have odd timestamp
      callbacks_after_reset++;
    }
  }

  encoder = new VideoEncoder(codecInit);
  encoder.configure(defaultConfig);
  await encoder.flush();

  // Send 10x frames to the encoder, call reset() on it after x outputs,
  // and make sure no more chunks are emitted afterwards.
  let encodes_before_reset = expected_callbacks_before_reset * 10;
  for (let i = 0; i < encodes_before_reset; i++) {
    let frame = new VideoFrame(bitmap, { timestamp: timestamp });
    timestamp += timestamp_step;
    encoder.encode(frame);
    frame.close();
  }

  await t.step_wait(() => reset_completed,
    "Reset() should be called by output callback", 10000, 1);

  assert_equals(callbacks_before_reset, expected_callbacks_before_reset);
  assert_true(reset_completed);
  assert_equals(encoder.encodeQueueSize, 0);

  let newConfig = { ...defaultConfig };
  newConfig.width = 800;
  newConfig.height = 600;
  encoder.configure(newConfig);

  const frames_after_reset = 5;
  for (let i = 0; i < frames_after_reset; i++) {
    let frame = await createVideoFrame(800, 600, timestamp + 1);
    timestamp += timestamp_step;
    encoder.encode(frame);
    frame.close();
  }
  await encoder.flush();

  assert_equals(callbacks_after_reset, frames_after_reset,
    "not all after-reset() outputs have been emitted");
  assert_equals(callbacks_before_reset, expected_callbacks_before_reset,
    "pre-reset() outputs were emitter after reset() and flush()");
  assert_equals(encoder.encodeQueueSize, 0);
}, 'Test successful reset() and re-confiugre()');

promise_test(async t => {
  let output_chunks = [];
  let codecInit = getDefaultCodecInit(t);
  codecInit.output = chunk => output_chunks.push(chunk);

  let encoder = new VideoEncoder(codecInit);

  // No encodes yet.
  assert_equals(encoder.encodeQueueSize, 0);

  let config = defaultConfig;

  encoder.configure(config);

  let frame1 = await createVideoFrame(640, 480, 0);
  let frame2 = await createVideoFrame(640, 480, 33333);

  encoder.encode(frame1);
  encoder.configure(config);

  encoder.encode(frame2);

  await encoder.flush();

  // We can guarantee that all encodes are processed after a flush.
  assert_equals(encoder.encodeQueueSize, 0);

  assert_true(output_chunks.length == 2);
  assert_equals(output_chunks[0].timestamp, frame1.timestamp);
  assert_equals(output_chunks[1].timestamp, frame2.timestamp);

  output_chunks = [];

  let frame3 = await createVideoFrame(640, 480, 66666);
  let frame4 = await createVideoFrame(640, 480, 100000);

  encoder.encode(frame3);

  // Verify that a failed call to configure does not change the encoder's state.
  let badConfig = { ...defaultConfig };
  badConfig.codec = 'bogus';
  assert_throws_js(TypeError, () => encoder.configure(badConfig));

  encoder.encode(frame4);

  await encoder.flush();

  assert_equals(output_chunks[0].timestamp, frame3.timestamp);
  assert_equals(output_chunks[1].timestamp, frame4.timestamp);
}, 'Test successful encode() after re-configure().');

promise_test(async t => {
  let encoder = new VideoEncoder(getDefaultCodecInit(t));

  let frame = await createVideoFrame(640, 480, 0);

  return testClosedCodec(t, encoder, defaultConfig, frame);
}, 'Verify closed VideoEncoder operations');

promise_test(async t => {
  let encoder = new VideoEncoder(getDefaultCodecInit(t));

  let frame = await createVideoFrame(640, 480, 0);

  return testUnconfiguredCodec(t, encoder, frame);
}, 'Verify unconfigured VideoEncoder operations');

promise_test(async t => {
  let encoder = new VideoEncoder(getDefaultCodecInit(t));

  let frame = await createVideoFrame(640, 480, 0);
  frame.close();

  encoder.configure(defaultConfig);

  assert_throws_dom("OperationError", () => {
    encoder.encode(frame);
  });
}, 'Verify encoding closed frames throws.');

promise_test(async t => {
  let output_chunks = [];
  let codecInit = getDefaultCodecInit(t);
  codecInit.output = chunk => output_chunks.push(chunk);

  let encoder = new VideoEncoder(codecInit);
  let config = defaultConfig;
  encoder.configure(config);

  let frame = await createVideoFrame(640, 480, -10000);
  encoder.encode(frame);
  frame.close();
  await encoder.flush();
  encoder.close();
  assert_equals(output_chunks.length, 1);
  assert_equals(output_chunks[0].timestamp, -10000, "first chunk timestamp");
  assert_greater_than(output_chunks[0].byteLength, 0);
}, 'Encode video with negative timestamp');
