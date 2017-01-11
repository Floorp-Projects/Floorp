/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./webmenc.h"

#include <string>

#include "third_party/libwebm/mkvmuxer.hpp"
#include "third_party/libwebm/mkvmuxerutil.hpp"
#include "third_party/libwebm/mkvwriter.hpp"

namespace {
const uint64_t kDebugTrackUid = 0xDEADBEEF;
const int kVideoTrackNumber = 1;
}  // namespace

void write_webm_file_header(struct EbmlGlobal *glob,
                            const vpx_codec_enc_cfg_t *cfg,
                            const struct vpx_rational *fps,
                            stereo_format_t stereo_fmt,
                            unsigned int fourcc,
                            const struct VpxRational *par) {
  mkvmuxer::MkvWriter *const writer = new mkvmuxer::MkvWriter(glob->stream);
  mkvmuxer::Segment *const segment = new mkvmuxer::Segment();
  segment->Init(writer);
  segment->set_mode(mkvmuxer::Segment::kFile);
  segment->OutputCues(true);

  mkvmuxer::SegmentInfo *const info = segment->GetSegmentInfo();
  const uint64_t kTimecodeScale = 1000000;
  info->set_timecode_scale(kTimecodeScale);
  std::string version = "vpxenc";
  if (!glob->debug) {
    version.append(std::string(" ") + vpx_codec_version_str());
  }
  info->set_writing_app(version.c_str());

  const uint64_t video_track_id =
      segment->AddVideoTrack(static_cast<int>(cfg->g_w),
                             static_cast<int>(cfg->g_h),
                             kVideoTrackNumber);
  mkvmuxer::VideoTrack* const video_track =
      static_cast<mkvmuxer::VideoTrack*>(
          segment->GetTrackByNumber(video_track_id));
  video_track->SetStereoMode(stereo_fmt);
  video_track->set_codec_id(fourcc == VP8_FOURCC ? "V_VP8" : "V_VP9");
  if (par->numerator > 1 || par->denominator > 1) {
    // TODO(fgalligan): Add support of DisplayUnit, Display Aspect Ratio type
    // to WebM format.
    const uint64_t display_width =
        static_cast<uint64_t>(((cfg->g_w * par->numerator * 1.0) /
                               par->denominator) + .5);
    video_track->set_display_width(display_width);
    video_track->set_display_height(cfg->g_h);
  }
  if (glob->debug) {
    video_track->set_uid(kDebugTrackUid);
  }
  glob->writer = writer;
  glob->segment = segment;
}

void write_webm_block(struct EbmlGlobal *glob,
                      const vpx_codec_enc_cfg_t *cfg,
                      const vpx_codec_cx_pkt_t *pkt) {
  mkvmuxer::Segment *const segment =
      reinterpret_cast<mkvmuxer::Segment*>(glob->segment);
  int64_t pts_ns = pkt->data.frame.pts * 1000000000ll *
                   cfg->g_timebase.num / cfg->g_timebase.den;
  if (pts_ns <= glob->last_pts_ns)
    pts_ns = glob->last_pts_ns + 1000000;
  glob->last_pts_ns = pts_ns;

  segment->AddFrame(static_cast<uint8_t*>(pkt->data.frame.buf),
                    pkt->data.frame.sz,
                    kVideoTrackNumber,
                    pts_ns,
                    pkt->data.frame.flags & VPX_FRAME_IS_KEY);
}

void write_webm_file_footer(struct EbmlGlobal *glob) {
  mkvmuxer::MkvWriter *const writer =
      reinterpret_cast<mkvmuxer::MkvWriter*>(glob->writer);
  mkvmuxer::Segment *const segment =
      reinterpret_cast<mkvmuxer::Segment*>(glob->segment);
  segment->Finalize();
  delete segment;
  delete writer;
  glob->writer = NULL;
  glob->segment = NULL;
}
