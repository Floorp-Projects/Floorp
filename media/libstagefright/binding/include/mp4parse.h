// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef _MP4PARSE_RUST_H
#define _MP4PARSE_RUST_H

#ifdef __cplusplus
extern "C" {
#endif

struct mp4parse_state;

#define MP4PARSE_TRACK_TYPE_H264 0 // "video/avc"
#define MP4PARSE_TRACK_TYPE_AAC  1 // "audio/mp4a-latm"

struct mp4parse_track_audio_info {
  uint16_t channels;
  uint16_t bit_depth;
  uint32_t sample_rate;
  //int32_t profile;
  //int32_t extended_profile; // check types

  // TODO(kinetik):
  // extra_data
  // codec_specific_config
};

struct mp4parse_track_video_info {
  uint32_t display_width;
  uint32_t display_height;
  uint16_t image_width;
  uint16_t image_height;

  // TODO(kinetik):
  // extra_data
  // codec_specific_config
};

struct mp4parse_track_info {
  uint32_t track_type;
  uint32_t track_id;
  uint64_t duration;
  int64_t media_time;
  // TODO(kinetik): crypto guff
};

struct mp4parse_state* mp4parse_new(void);
void mp4parse_free(struct mp4parse_state* state);

int32_t mp4parse_read(struct mp4parse_state* state, uint8_t *buffer, size_t size);

int32_t mp4parse_get_track_info(struct mp4parse_state* state, int32_t track, struct mp4parse_track_info* track_info);

int32_t mp4parse_get_track_audio_info(struct mp4parse_state* state, int32_t track, struct mp4parse_track_audio_info* track_info);

int32_t mp4parse_get_track_video_info(struct mp4parse_state* state, int32_t track, struct mp4parse_track_video_info* track_info);

#ifdef __cplusplus
}
#endif

#endif
