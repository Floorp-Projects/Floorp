#include <vector>
#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/vp9_iface_common.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/simple_encode.h"
#include "vp9/vp9_cx_iface.h"

namespace vp9 {

// TODO(angiebird): Merge this function with vpx_img_plane_width()
static int img_plane_width(const vpx_image_t *img, int plane) {
  if (plane > 0 && img->x_chroma_shift > 0)
    return (img->d_w + 1) >> img->x_chroma_shift;
  else
    return img->d_w;
}

// TODO(angiebird): Merge this function with vpx_img_plane_height()
static int img_plane_height(const vpx_image_t *img, int plane) {
  if (plane > 0 && img->y_chroma_shift > 0)
    return (img->d_h + 1) >> img->y_chroma_shift;
  else
    return img->d_h;
}

// TODO(angiebird): Merge this function with vpx_img_read()
static int img_read(vpx_image_t *img, FILE *file) {
  int plane;

  for (plane = 0; plane < 3; ++plane) {
    unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = img_plane_width(img, plane) *
                  ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      if (fread(buf, 1, w, file) != (size_t)w) return 0;
      buf += stride;
    }
  }

  return 1;
}

class SimpleEncode::EncodeImpl {
 public:
  VP9_COMP *cpi;
  vpx_img_fmt_t img_fmt;
  vpx_image_t tmp_img;
  std::vector<FIRSTPASS_STATS> first_pass_stats;
};

static VP9_COMP *init_encoder(const VP9EncoderConfig *oxcf,
                              vpx_img_fmt_t img_fmt) {
  VP9_COMP *cpi;
  BufferPool *buffer_pool = (BufferPool *)vpx_calloc(1, sizeof(*buffer_pool));
  vp9_initialize_enc();
  cpi = vp9_create_compressor(oxcf, buffer_pool);
  vp9_update_compressor_with_img_fmt(cpi, img_fmt);
  return cpi;
}

static void free_encoder(VP9_COMP *cpi) {
  BufferPool *buffer_pool = cpi->common.buffer_pool;
  vp9_remove_compressor(cpi);
  // buffer_pool needs to be free after cpi because buffer_pool contains
  // allocated buffers that will be free in vp9_remove_compressor()
  vpx_free(buffer_pool);
}

static INLINE vpx_rational_t make_vpx_rational(int num, int den) {
  vpx_rational_t v;
  v.num = num;
  v.den = den;
  return v;
}

static INLINE FrameType
get_frame_type_from_update_type(FRAME_UPDATE_TYPE update_type) {
  // TODO(angiebird): Figure out if we need frame type other than key frame,
  // alternate reference and inter frame
  switch (update_type) {
    case KF_UPDATE: return kKeyFrame; break;
    case ARF_UPDATE: return kAlternateReference; break;
    default: return kInterFrame; break;
  }
}

static void update_encode_frame_result(
    EncodeFrameResult *encode_frame_result,
    const ENCODE_FRAME_RESULT *encode_frame_info) {
  encode_frame_result->coding_data_bit_size =
      encode_frame_result->coding_data_byte_size * 8;
  encode_frame_result->show_idx = encode_frame_info->show_idx;
  encode_frame_result->frame_type =
      get_frame_type_from_update_type(encode_frame_info->update_type);
  encode_frame_result->psnr = encode_frame_info->psnr;
  encode_frame_result->sse = encode_frame_info->sse;
  encode_frame_result->quantize_index = encode_frame_info->quantize_index;
}

SimpleEncode::SimpleEncode(int frame_width, int frame_height,
                           int frame_rate_num, int frame_rate_den,
                           int target_bitrate, int num_frames,
                           const char *infile_path) {
  impl_ptr_ = std::unique_ptr<EncodeImpl>(new EncodeImpl());
  frame_width_ = frame_width;
  frame_height_ = frame_height;
  frame_rate_num_ = frame_rate_num;
  frame_rate_den_ = frame_rate_den;
  target_bitrate_ = target_bitrate;
  num_frames_ = num_frames;
  // TODO(angirbid): Should we keep a file pointer here or keep the file_path?
  file_ = fopen(infile_path, "r");
  impl_ptr_->cpi = NULL;
  impl_ptr_->img_fmt = VPX_IMG_FMT_I420;
}

void SimpleEncode::ComputeFirstPassStats() {
  vpx_rational_t frame_rate =
      make_vpx_rational(frame_rate_num_, frame_rate_den_);
  const VP9EncoderConfig oxcf =
      vp9_get_encoder_config(frame_width_, frame_height_, frame_rate,
                             target_bitrate_, VPX_RC_FIRST_PASS);
  VP9_COMP *cpi = init_encoder(&oxcf, impl_ptr_->img_fmt);
  struct lookahead_ctx *lookahead = cpi->lookahead;
  int i;
  int use_highbitdepth = 0;
#if CONFIG_VP9_HIGHBITDEPTH
  use_highbitdepth = cpi->common.use_highbitdepth;
#endif
  vpx_image_t img;
  vpx_img_alloc(&img, impl_ptr_->img_fmt, frame_width_, frame_height_, 1);
  rewind(file_);
  impl_ptr_->first_pass_stats.clear();
  for (i = 0; i < num_frames_; ++i) {
    assert(!vp9_lookahead_full(lookahead));
    if (img_read(&img, file_)) {
      int next_show_idx = vp9_lookahead_next_show_idx(lookahead);
      int64_t ts_start =
          timebase_units_to_ticks(&oxcf.g_timebase_in_ts, next_show_idx);
      int64_t ts_end =
          timebase_units_to_ticks(&oxcf.g_timebase_in_ts, next_show_idx + 1);
      YV12_BUFFER_CONFIG sd;
      image2yuvconfig(&img, &sd);
      vp9_lookahead_push(lookahead, &sd, ts_start, ts_end, use_highbitdepth, 0);
      {
        int64_t time_stamp;
        int64_t time_end;
        int flush = 1;  // Makes vp9_get_compressed_data process a frame
        size_t size;
        unsigned int frame_flags = 0;
        ENCODE_FRAME_RESULT encode_frame_info;
        // TODO(angiebird): Call vp9_first_pass directly
        vp9_get_compressed_data(cpi, &frame_flags, &size, NULL, &time_stamp,
                                &time_end, flush, &encode_frame_info);
        // vp9_get_compressed_data only generates first pass stats not
        // compresses data
        assert(size == 0);
      }
      impl_ptr_->first_pass_stats.push_back(vp9_get_frame_stats(&cpi->twopass));
    }
  }
  vp9_end_first_pass(cpi);
  // TODO(angiebird): Store the total_stats apart form first_pass_stats
  impl_ptr_->first_pass_stats.push_back(vp9_get_total_stats(&cpi->twopass));
  free_encoder(cpi);
  rewind(file_);
  vpx_img_free(&img);
}

std::vector<std::vector<double>> SimpleEncode::ObserveFirstPassStats() {
  std::vector<std::vector<double>> output_stats;
  // TODO(angiebird): This function make several assumptions of
  // FIRSTPASS_STATS. 1) All elements in FIRSTPASS_STATS are double except the
  // last one. 2) The last entry of first_pass_stats is the total_stats.
  // Change the code structure, so that we don't have to make these assumptions

  // Note the last entry of first_pass_stats is the total_stats, we don't need
  // it.
  for (size_t i = 0; i < impl_ptr_->first_pass_stats.size() - 1; ++i) {
    double *buf_start =
        reinterpret_cast<double *>(&impl_ptr_->first_pass_stats[i]);
    // We use - 1 here because the last member in FIRSTPASS_STATS is not double
    double *buf_end =
        buf_start + sizeof(impl_ptr_->first_pass_stats[i]) / sizeof(*buf_end) -
        1;
    std::vector<double> this_stats(buf_start, buf_end);
    output_stats.push_back(this_stats);
  }
  return output_stats;
}

void SimpleEncode::StartEncode() {
  assert(impl_ptr_->first_pass_stats.size() > 0);
  vpx_rational_t frame_rate =
      make_vpx_rational(frame_rate_num_, frame_rate_den_);
  VP9EncoderConfig oxcf =
      vp9_get_encoder_config(frame_width_, frame_height_, frame_rate,
                             target_bitrate_, VPX_RC_LAST_PASS);
  vpx_fixed_buf_t stats;
  stats.buf = impl_ptr_->first_pass_stats.data();
  stats.sz = sizeof(impl_ptr_->first_pass_stats[0]) *
             impl_ptr_->first_pass_stats.size();

  vp9_set_first_pass_stats(&oxcf, &stats);
  assert(impl_ptr_->cpi == NULL);
  impl_ptr_->cpi = init_encoder(&oxcf, impl_ptr_->img_fmt);
  vpx_img_alloc(&impl_ptr_->tmp_img, impl_ptr_->img_fmt, frame_width_,
                frame_height_, 1);
  rewind(file_);
}

void SimpleEncode::EndEncode() {
  free_encoder(impl_ptr_->cpi);
  impl_ptr_->cpi = nullptr;
  vpx_img_free(&impl_ptr_->tmp_img);
  rewind(file_);
}

int SimpleEncode::GetKeyFrameGroupSize(int key_frame_index) const {
  const VP9_COMP *cpi = impl_ptr_->cpi;
  return vp9_get_frames_to_next_key(&cpi->oxcf, &cpi->frame_info,
                                    &cpi->twopass.first_pass_info,
                                    key_frame_index, cpi->rc.min_gf_interval);
}

void SimpleEncode::EncodeFrame(EncodeFrameResult *encode_frame_result) {
  VP9_COMP *cpi = impl_ptr_->cpi;
  struct lookahead_ctx *lookahead = cpi->lookahead;
  int use_highbitdepth = 0;
#if CONFIG_VP9_HIGHBITDEPTH
  use_highbitdepth = cpi->common.use_highbitdepth;
#endif
  // The lookahead's size is set to oxcf->lag_in_frames.
  // We want to fill lookahead to it's max capacity if possible so that the
  // encoder can construct alt ref frame in time.
  // In the other words, we hope vp9_get_compressed_data to encode a frame
  // every time in the function
  while (!vp9_lookahead_full(lookahead)) {
    // TODO(angiebird): Check whether we can move this file read logics to
    // lookahead
    if (img_read(&impl_ptr_->tmp_img, file_)) {
      int next_show_idx = vp9_lookahead_next_show_idx(lookahead);
      int64_t ts_start =
          timebase_units_to_ticks(&cpi->oxcf.g_timebase_in_ts, next_show_idx);
      int64_t ts_end = timebase_units_to_ticks(&cpi->oxcf.g_timebase_in_ts,
                                               next_show_idx + 1);
      YV12_BUFFER_CONFIG sd;
      image2yuvconfig(&impl_ptr_->tmp_img, &sd);
      vp9_lookahead_push(lookahead, &sd, ts_start, ts_end, use_highbitdepth, 0);
    } else {
      break;
    }
  }
  assert(encode_frame_result->coding_data.get() == nullptr);
  const size_t max_coding_data_byte_size = frame_width_ * frame_height_ * 3;
  encode_frame_result->coding_data = std::move(
      std::unique_ptr<uint8_t[]>(new uint8_t[max_coding_data_byte_size]));
  int64_t time_stamp;
  int64_t time_end;
  int flush = 1;  // Make vp9_get_compressed_data encode a frame
  unsigned int frame_flags = 0;
  ENCODE_FRAME_RESULT encode_frame_info;
  vp9_get_compressed_data(cpi, &frame_flags,
                          &encode_frame_result->coding_data_byte_size,
                          encode_frame_result->coding_data.get(), &time_stamp,
                          &time_end, flush, &encode_frame_info);
  // vp9_get_compressed_data is expected to encode a frame every time, so the
  // data size should be greater than zero.
  assert(encode_frame_result->coding_data_byte_size > 0);
  assert(encode_frame_result->coding_data_byte_size <
         max_coding_data_byte_size);

  update_encode_frame_result(encode_frame_result, &encode_frame_info);
}

void SimpleEncode::EncodeFrameWithQuantizeIndex(
    EncodeFrameResult *encode_frame_result, int quantize_index) {
  encode_command_set_external_quantize_index(&impl_ptr_->cpi->encode_command,
                                             quantize_index);
  EncodeFrame(encode_frame_result);
  encode_command_reset_external_quantize_index(&impl_ptr_->cpi->encode_command);
}

int SimpleEncode::GetCodingFrameNum() const {
  assert(impl_ptr_->first_pass_stats.size() - 1 > 0);
  // These are the default settings for now.
  const int multi_layer_arf = 0;
  const int allow_alt_ref = 1;
  vpx_rational_t frame_rate =
      make_vpx_rational(frame_rate_num_, frame_rate_den_);
  const VP9EncoderConfig oxcf =
      vp9_get_encoder_config(frame_width_, frame_height_, frame_rate,
                             target_bitrate_, VPX_RC_LAST_PASS);
  FRAME_INFO frame_info = vp9_get_frame_info(&oxcf);
  FIRST_PASS_INFO first_pass_info;
  fps_init_first_pass_info(&first_pass_info, impl_ptr_->first_pass_stats.data(),
                           num_frames_);
  return vp9_get_coding_frame_num(&oxcf, &frame_info, &first_pass_info,
                                  multi_layer_arf, allow_alt_ref);
}

SimpleEncode::~SimpleEncode() {
  if (this->file_ != NULL) {
    fclose(this->file_);
  }
}

}  // namespace vp9
