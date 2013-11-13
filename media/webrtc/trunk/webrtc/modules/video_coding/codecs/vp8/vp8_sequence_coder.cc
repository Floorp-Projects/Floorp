 /*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/common_video/interface/video_image.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"
#include "webrtc/tools/simple_command_line_parser.h"

class Vp8SequenceCoderEncodeCallback : public webrtc::EncodedImageCallback {
 public:
  explicit Vp8SequenceCoderEncodeCallback(FILE* encoded_file)
      : encoded_file_(encoded_file),
        encoded_bytes_(0) {}
  ~Vp8SequenceCoderEncodeCallback();
  int Encoded(webrtc::EncodedImage& encoded_image,
              const webrtc::CodecSpecificInfo* codecSpecificInfo,
              const webrtc::RTPFragmentationHeader*);
  // Returns the encoded image.
  webrtc::EncodedImage encoded_image() { return encoded_image_; }
  int encoded_bytes() { return encoded_bytes_; }
 private:
  webrtc::EncodedImage encoded_image_;
  FILE* encoded_file_;
  int encoded_bytes_;
};

Vp8SequenceCoderEncodeCallback::~Vp8SequenceCoderEncodeCallback() {
  delete [] encoded_image_._buffer;
  encoded_image_._buffer = NULL;
}
int Vp8SequenceCoderEncodeCallback::Encoded(
    webrtc::EncodedImage& encoded_image,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    const webrtc::RTPFragmentationHeader* fragmentation) {
  if (encoded_image_._size < encoded_image._size) {
    delete [] encoded_image_._buffer;
    encoded_image_._buffer = NULL;
    encoded_image_._buffer = new uint8_t[encoded_image._size];
    encoded_image_._size = encoded_image._size;
  }
  memcpy(encoded_image_._buffer, encoded_image._buffer, encoded_image._size);
  encoded_image_._length = encoded_image._length;
  if (encoded_file_ != NULL) {
    if (fwrite(encoded_image._buffer, 1, encoded_image._length,
               encoded_file_) != encoded_image._length) {
      return -1;
    }
  }
  encoded_bytes_ += encoded_image_._length;
  return 0;
}

// TODO(mikhal): Add support for varying the frame size.
class Vp8SequenceCoderDecodeCallback : public webrtc::DecodedImageCallback {
 public:
  explicit Vp8SequenceCoderDecodeCallback(FILE* decoded_file)
      : decoded_file_(decoded_file) {}
  int Decoded(webrtc::I420VideoFrame& frame);
  bool DecodeComplete();

 private:
  FILE* decoded_file_;
};

int Vp8SequenceCoderDecodeCallback::Decoded(webrtc::I420VideoFrame& image) {
  EXPECT_EQ(0, webrtc::PrintI420VideoFrame(image, decoded_file_));
  return 0;
}

int SequenceCoder(webrtc::test::CommandLineParser& parser) {
  int width = strtol((parser.GetFlag("w")).c_str(), NULL, 10);
  int height = strtol((parser.GetFlag("h")).c_str(), NULL, 10);
  int framerate = strtol((parser.GetFlag("f")).c_str(), NULL, 10);

  if (width <= 0 || height <= 0 || framerate <= 0) {
    fprintf(stderr, "Error: Resolution cannot be <= 0!\n");
    return -1;
  }
  int target_bitrate = strtol((parser.GetFlag("b")).c_str(), NULL, 10);
  if (target_bitrate <= 0) {
    fprintf(stderr, "Error: Bit-rate cannot be <= 0!\n");
    return -1;
  }

  // SetUp
  // Open input file.
  std::string encoded_file_name = parser.GetFlag("encoded_file");
  FILE* encoded_file = fopen(encoded_file_name.c_str(), "wb");
  if (encoded_file == NULL) {
    fprintf(stderr, "Error: Cannot open encoded file\n");
    return -1;
  }
  std::string input_file_name = parser.GetFlag("input_file");
  FILE* input_file = fopen(input_file_name.c_str(), "rb");
  if (input_file == NULL) {
    fprintf(stderr, "Error: Cannot open input file\n");
    return -1;
  }
  // Open output file.
  std::string output_file_name = parser.GetFlag("output_file");
  FILE* output_file = fopen(output_file_name.c_str(), "wb");
  if (output_file == NULL) {
    fprintf(stderr, "Error: Cannot open output file\n");
    return -1;
  }

  // Get range of frames: will encode num_frames following start_frame).
  int start_frame = strtol((parser.GetFlag("start_frame")).c_str(), NULL, 10);
  int num_frames = strtol((parser.GetFlag("num_frames")).c_str(), NULL, 10);

  // Codec SetUp.
  webrtc::VideoCodec inst;
  memset(&inst, 0, sizeof(inst));
  webrtc::VP8Encoder* encoder = webrtc::VP8Encoder::Create();
  webrtc::VP8Decoder* decoder = webrtc::VP8Decoder::Create();
  inst.codecType = webrtc::kVideoCodecVP8;
  inst.codecSpecific.VP8.feedbackModeOn = false;
  inst.codecSpecific.VP8.denoisingOn = true;
  inst.maxFramerate = framerate;
  inst.startBitrate = target_bitrate;
  inst.maxBitrate = 8000;
  inst.width = width;
  inst.height = height;

  if (encoder->InitEncode(&inst, 1, 1440) < 0) {
    fprintf(stderr, "Error: Cannot initialize vp8 encoder\n");
    return -1;
  }
  EXPECT_EQ(0, decoder->InitDecode(&inst, 1));
  webrtc::I420VideoFrame input_frame;
  unsigned int length = webrtc::CalcBufferSize(webrtc::kI420, width, height);
  webrtc::scoped_array<uint8_t> frame_buffer(new uint8_t[length]);

  int half_width = (width + 1) / 2;
  // Set and register callbacks.
  Vp8SequenceCoderEncodeCallback encoder_callback(encoded_file);
  encoder->RegisterEncodeCompleteCallback(&encoder_callback);
  Vp8SequenceCoderDecodeCallback decoder_callback(output_file);
  decoder->RegisterDecodeCompleteCallback(&decoder_callback);
  // Read->Encode->Decode sequence.
  // num_frames = -1 implies unlimited encoding (entire sequence).
  int64_t starttime = webrtc::TickTime::MillisecondTimestamp();
  int frame_cnt = 1;
  int frames_processed = 0;
  input_frame.CreateEmptyFrame(width, height, width, half_width, half_width);
  while (!feof(input_file) &&
      (num_frames == -1 || frames_processed < num_frames)) {
     if (fread(frame_buffer.get(), 1, length, input_file) != length)
      continue;
    if (frame_cnt >= start_frame) {
      webrtc::ConvertToI420(webrtc::kI420, frame_buffer.get(), 0, 0,
                            width, height, 0, webrtc::kRotateNone,
                            &input_frame);
      encoder->Encode(input_frame, NULL, NULL);
      decoder->Decode(encoder_callback.encoded_image(), false, NULL);
      ++frames_processed;
    }
    ++frame_cnt;
  }
  printf("\nProcessed %d frames\n", frames_processed);
  int64_t endtime = webrtc::TickTime::MillisecondTimestamp();
  int64_t totalExecutionTime = endtime - starttime;
  printf("Total execution time: %.2lf ms\n",
         static_cast<double>(totalExecutionTime));
  int sum_enc_bytes = encoder_callback.encoded_bytes();
  double actual_bit_rate =  8.0 * sum_enc_bytes /
      (frame_cnt / inst.maxFramerate);
  printf("Actual bitrate: %f kbps\n", actual_bit_rate / 1000);
  webrtc::test::QualityMetricsResult psnr_result, ssim_result;
  EXPECT_EQ(0, webrtc::test::I420MetricsFromFiles(
      input_file_name.c_str(), output_file_name.c_str(),
      inst.width, inst.height,
      &psnr_result, &ssim_result));
  printf("PSNR avg: %f[dB], min: %f[dB]\nSSIM avg: %f, min: %f\n",
          psnr_result.average, psnr_result.min,
          ssim_result.average, ssim_result.min);
  return frame_cnt;
}

int main(int argc, char** argv) {
  std::string program_name = argv[0];
  std::string usage = "Encode and decodes a video sequence, and writes"
  "results to a file.\n"
  "Example usage:\n" + program_name + " functionality"
  " --w=352 --h=288 --input_file=input.yuv --output_file=output.yuv "
  " Command line flags:\n"
  "  - width(int): The width of the input file. Default: 352\n"
  "  - height(int): The height of the input file. Default: 288\n"
  "  - input_file(string): The YUV file to encode."
  "      Default: foreman.yuv\n"
  "  - encoded_file(string): The vp8 encoded file (encoder output)."
  "      Default: vp8_encoded.vp8\n"
  "  - output_file(string): The yuv decoded file (decoder output)."
  "      Default: vp8_decoded.yuv\n."
  "  - start_frame - frame number in which encoding will begin. Default: 0"
  "  - num_frames - Number of frames to be processed. "
  "      Default: -1 (entire sequence).";

  webrtc::test::CommandLineParser parser;

  // Init the parser and set the usage message.
  parser.Init(argc, argv);

  // Reset flags.
  parser.SetFlag("w", "352");
  parser.SetFlag("h", "288");
  parser.SetFlag("f", "30");
  parser.SetFlag("b", "500");
  parser.SetFlag("start_frame", "0");
  parser.SetFlag("num_frames", "-1");
  parser.SetFlag("output_file", webrtc::test::OutputPath() + "vp8_decoded.yuv");
  parser.SetFlag("encoded_file",
                 webrtc::test::OutputPath() + "vp8_encoded.vp8");
  parser.SetFlag("input_file", webrtc::test::ResourcePath("foreman_cif",
                                                          "yuv"));

  parser.ProcessFlags();
  if (parser.GetFlag("help") == "true") {
    parser.PrintUsageMessage();
  }
  parser.PrintEnteredFlags();

  return SequenceCoder(parser);
}
