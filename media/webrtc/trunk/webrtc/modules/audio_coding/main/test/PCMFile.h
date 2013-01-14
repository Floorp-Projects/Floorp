/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_PCMFILE_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_PCMFILE_H_

#include <cstdio>
#include <cstdlib>
#include <string>

#include "module_common_types.h"
#include "typedefs.h"

namespace webrtc {

class PCMFile {
 public:
  PCMFile();
  PCMFile(WebRtc_UWord32 timestamp);
  ~PCMFile() {
    if (pcm_file_ != NULL) {
      fclose(pcm_file_);
    }
  }

  void Open(const std::string& filename, WebRtc_UWord16 frequency,
                  const char* mode, bool auto_rewind = false);

  WebRtc_Word32 Read10MsData(AudioFrame& audio_frame);

  void Write10MsData(WebRtc_Word16 *playout_buffer,
                     WebRtc_UWord16 length_smpls);
  void Write10MsData(AudioFrame& audio_frame);

  WebRtc_UWord16 PayloadLength10Ms() const;
  WebRtc_Word32 SamplingFrequency() const;
  void Close();
  bool EndOfFile() const {
    return end_of_file_;
  }
  void Rewind();
  static WebRtc_Word16 ChooseFile(std::string* file_name,
                                  WebRtc_Word16 max_len,
                                  WebRtc_UWord16* frequency_hz);
  static WebRtc_Word16 ChooseFile(std::string* file_name,
                                  WebRtc_Word16 max_len);
  bool Rewinded();
  void SaveStereo(bool is_stereo = true);
  void ReadStereo(bool is_stereo = true);
 private:
  FILE* pcm_file_;
  WebRtc_UWord16 samples_10ms_;
  WebRtc_Word32 frequency_;
  bool end_of_file_;
  bool auto_rewind_;
  bool rewinded_;
  WebRtc_UWord32 timestamp_;
  bool read_stereo_;
  bool save_stereo_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_PCMFILE_H_
